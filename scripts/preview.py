#!/usr/bin/env python3
"""Serve a built PasteBox against disposable data on a separate loopback port."""
import argparse
import json
import os
from pathlib import Path
import signal
import socket
import subprocess
import tempfile

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--binary',type=Path,required=True)
parser.add_argument('--public',type=Path,required=True)
parser.add_argument('--port',type=int,default=0)
args = parser.parse_args()
binary, public = args.binary.resolve(strict=True), args.public.resolve(strict=True)
if not (public/'index.html').is_file():
    parser.error('--public must contain a built index.html')
with tempfile.TemporaryDirectory(prefix='pastebox-preview-') as directory:
    root = Path(directory)
    with socket.socket() as sock:
        sock.bind(('127.0.0.1',args.port))
        port = sock.getsockname()[1]
    config = json.loads((Path(__file__).resolve().parent.parent/'backend/config.json').read_text())
    config['listeners'][0]['port'] = port
    config['app']['document_root'] = str(public)
    config['db_clients'][0]['filename'] = str(root/'preview.db')
    (root/'config.json').write_text(json.dumps(config))
    env = {**os.environ,'PASTEBOX_CONFIG':str(root/'config.json'),'PASTEBOX_ORIGIN':f'http://127.0.0.1:{port}','PASTEBOX_TRUST_PROXY':'0'}
    server = subprocess.Popen([str(binary)],cwd=root,env=env)
    def stop(*_):
        server.terminate()
    signal.signal(signal.SIGTERM,stop)
    signal.signal(signal.SIGINT,stop)
    print(f'Preview: http://127.0.0.1:{port}',flush=True)
    try:
        server.wait()
    finally:
        if server.poll() is None:
            server.terminate()
            server.wait(timeout=10)
