#!/usr/bin/env python3
"""Read-only production smoke checks; never opens or creates a customer paste."""
import argparse
import json
from pathlib import Path
import re
import subprocess
import tempfile
from urllib.parse import urlsplit

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('origin',nargs='?',default='https://pastebox.micutu.com')
args = parser.parse_args()
origin = args.origin.rstrip('/')
parsed=urlsplit(origin)
production=parsed.scheme=='https' and parsed.hostname=='pastebox.micutu.com' and parsed.port in (None,443)
preview=parsed.scheme=='http' and parsed.hostname=='127.0.0.1' and parsed.port is not None
if not (production or preview) or parsed.username or parsed.password or parsed.path or parsed.query or parsed.fragment:
    parser.error('Use the PasteBox production origin or an explicit loopback preview')

def fetch(path):
    with tempfile.TemporaryDirectory(prefix='pastebox-smoke-') as directory:
        root=Path(directory)
        result=subprocess.run(['curl','--silent','--show-error','--max-time','20','--dump-header',str(root/'headers'),'--output',str(root/'body'),'--write-out','%{http_code}',origin+path],check=True,capture_output=True,text=True)
        headers={}
        for line in (root/'headers').read_text().splitlines():
            if ':' in line:
                key,value=line.split(':',1)
                headers.setdefault(key.lower(),[]).append(value.strip())
        return int(result.stdout),headers,(root/'body').read_bytes()

for path, expected in [('/',200),('/api/health',200),('/api/not-existing',404),('/robots.txt',200),('/sitemap.xml',200),('/share-card.png',200),('/embed/ReleaseSmokeCheck',200)]:
    status,headers,body=fetch(path)
    assert status==expected,(path,status)
    assert headers.get('x-content-type-options')==['nosniff'],(path,headers)
    if path.startswith('/api/'):
        assert headers.get('cache-control')==['no-store'],headers
        assert 'noindex' in headers.get('x-robots-tag',[''])[0]
        assert isinstance(json.loads(body),dict)
    if path=='/api/health': assert json.loads(body)=={'status':'ok'}
    if path=='/':
        assert b'rel="canonical"' in body and b'Code snippets, notes, and ideas worth sharing' in body
        assert headers.get('x-frame-options')==['SAMEORIGIN']
        assert "object-src 'none'" in headers['content-security-policy'][0]
        for asset in re.findall(rb'(?:src|href)="(/assets/[^\"]+)"',body):
            asset_status,asset_headers,_=fetch(asset.decode())
            assert asset_status==200
            assert 'immutable' in ','.join(asset_headers.get('cache-control',[]))
    if path=='/robots.txt': assert b'Disallow: /api/' in body and b'Disallow: /embed/' in body
    if path=='/sitemap.xml': assert body.count(b'<loc>')==1
    if path.startswith('/embed/'):
        assert 'x-frame-options' not in headers
        assert 'frame-ancestors *' in headers['content-security-policy'][0]
    print(f'{path}: {status}, checks passed')
