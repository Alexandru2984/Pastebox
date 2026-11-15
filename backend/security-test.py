#!/usr/bin/env python3
"""Isolated API regressions. Starts its own server and disposable SQLite DB.
Never accepts an external base URL and never touches the production database.
"""
import argparse
import concurrent.futures
import hashlib
import json
import os
from pathlib import Path
import socket
import sqlite3
import subprocess
import tempfile
import time
import unittest
import urllib.error
import urllib.request

ORIGIN = 'https://pastebox.micutu.com'

class API(unittest.TestCase):
    base = ''
    database = ''
    sequence = 0

    def setUp(self):
        API.sequence += 1
        self.ip = f'192.0.2.{API.sequence}'

    def request(self, path='/api/pastes', method='GET', data=None, token='', password='', headers=None):
        sent = {'X-Real-IP': self.ip, 'X-Requested-With': 'PasteBox'}
        if data is not None:
            sent['Content-Type'] = 'application/json'
        if token:
            sent['X-Owner-Token'] = token
        if password:
            sent['X-Password'] = password
        if headers:
            for key, value in headers.items():
                if value is None:
                    sent.pop(key, None)
                else:
                    sent[key] = value
        req = urllib.request.Request(self.base + path, method=method, headers=sent,
                                     data=json.dumps(data).encode() if data is not None else None)
        try:
            reply = urllib.request.urlopen(req, timeout=15)
        except urllib.error.HTTPError as error:
            reply = error
        raw = reply.read().decode()
        try:
            body = json.loads(raw)
        except ValueError:
            body = raw
        return reply.status, body, reply.headers

    def create(self, **overrides):
        code, body, _ = self.request(method='POST', data={'content': 'hello secret', **overrides})
        self.assertEqual(code, 201, body)
        self.assertEqual(len(body['owner_token']), 64)
        return body

    def test_owner_required_for_edit_and_delete(self):
        paste = self.create()
        path = '/api/pastes/' + paste['id']
        for method, data in [('PUT', {'title': 'hijacked'}), ('DELETE', None)]:
            self.assertEqual(self.request(path, method, data)[0], 403)
            self.assertEqual(self.request(path, method, data, token='f'*64)[0], 403)
        self.assertEqual(self.request(path, 'PUT', {'title': 'owned'}, token=paste['owner_token'],headers={'If-Match':f'"paste-{paste["id"]}-v1"'})[0], 200)
        self.assertEqual(self.request(path)[1]['title'], 'owned')
        self.assertEqual(self.request(path, 'DELETE', token=paste['owner_token'])[0], 200)
        self.assertEqual(self.request(path)[0], 404)

    def test_password_is_read_only(self):
        paste = self.create(password='a good password')
        path = '/api/pastes/' + paste['id']
        self.assertTrue(self.request(path)[1]['password_required'])
        self.assertEqual(self.request(path, password='a good password')[0], 200)
        self.assertEqual(self.request(path, 'PUT', {'content': 'tampered'}, password='a good password')[0], 403)
        self.assertEqual(self.request(path, 'DELETE', password='a good password')[0], 403)
        self.assertTrue(self.request(path, token=paste['owner_token'])[1]['can_manage'])

    def test_private_requires_owner_on_every_route(self):
        paste = self.create(visibility='private')
        path = '/api/pastes/' + paste['id']
        self.assertEqual(self.request(path)[0], 404)
        self.assertEqual(self.request(path+'/raw')[0], 404)
        self.assertEqual(self.request(path+'/fork','POST',{})[0], 404)
        self.assertEqual(self.request(path,token=paste['owner_token'])[0], 200)
        self.assertEqual(self.request(path+'/raw',token=paste['owner_token'])[1], 'hello secret')
        self.assertNotIn(paste['id'], json.dumps(self.request()[1]))

    def test_atomic_burn_concurrent_raw_and_json(self):
        paste = self.create(burn_after_read=True)
        path = '/api/pastes/' + paste['id']
        with concurrent.futures.ThreadPoolExecutor(max_workers=8) as pool:
            results = list(pool.map(lambda i: self.request(path + ('/raw' if i % 2 else ''),headers={'X-Confirm-Burn':'yes'})[0], range(8)))
        self.assertEqual(results.count(200), 1, results)
        self.assertEqual(results.count(404), 7, results)

    def test_burn_raw_cannot_be_reread_or_forked(self):
        paste = self.create(burn_after_read=True)
        path = '/api/pastes/' + paste['id']
        self.assertEqual(self.request(path+'/fork','POST',{})[0], 409)
        self.assertEqual(self.request(path+'/raw')[0], 409)
        self.assertTrue(self.request(path)[1]['burn_confirmation_required'])
        self.assertEqual(self.request(path+'/raw',headers={'X-Confirm-Burn':'yes'})[0], 200)
        self.assertEqual(self.request(path)[0], 404)

    def test_fork_inherits_privacy_password_expiration(self):
        source = self.create(visibility='unlisted',password='read pass',expires_in='1h',tags=['one','two'])
        path = '/api/pastes/' + source['id'] + '/fork'
        self.assertEqual(self.request(path,'POST',{})[0], 404)
        self.assertEqual(self.request(path,'POST',{'visibility':'public'},password='read pass')[0], 400)
        code, fork, _ = self.request(path,'POST',{},password='read pass')
        self.assertEqual(code, 201)
        self.assertEqual(fork['visibility'],'unlisted')
        self.assertEqual(fork['expires_at'],source['expires_at'])
        self.assertTrue(fork['has_password'])
        self.assertIsNone(fork['parent_id'])
        self.assertEqual(fork['tags'],['one','two'])
        self.assertNotEqual(fork['owner_token'],source['owner_token'])
        self.assertEqual(self.request('/api/pastes/'+fork['id'])[0],404)

    def test_duplicate_tags_commit_and_invalid_tags_do_not_mutate(self):
        paste = self.create(tags=['x','x','y'])
        path = '/api/pastes/' + paste['id']
        self.assertEqual(self.request(path,'PUT',{'tags':['z','z']},token=paste['owner_token'],headers={'If-Match':f'"paste-{paste["id"]}-v1"'})[0],200)
        self.assertEqual(self.request(path)[1]['tags'],['z'])
        for tags in [[{}], [''], ['a']*11, ['bad,tag'], ['bad\x01tag']]:
            self.assertEqual(self.request(path,'PUT',{'title':'bad','tags':tags},token=paste['owner_token'])[0],400)
        self.assertNotEqual(self.request(path)[1]['title'],'bad')
        self.assertEqual(self.request(path,'PUT',{'tags':[]},token=paste['owner_token'],headers={'If-Match':f'"paste-{paste["id"]}-v2"'})[0],200)

    def test_json_type_validation(self):
        for body in [[], 1, {'content':{}}, {'content':'ok','burn_after_read':{}},
                     {'content':'ok','tags':{}}, {'content':'ok','visibility':'bad'},
                     {'content':'ok','language':'<script>'}, {'content':'ok','password':1},
                     {'content':'ok','expires_in':'1year'}, {'content':'ok','owner_hash':'fake'}]:
            self.assertEqual(self.request(method='POST',data=body)[0],400,body)
        self.assertEqual(self.request('/api/health')[0],200)

    def test_csrf_and_origin(self):
        for headers in [{'X-Requested-With':None}, {'X-Requested-With':'wrong'},
                        {'Origin':'https://evil.example'}, {'Sec-Fetch-Site':'cross-site'}]:
            self.assertEqual(self.request(method='POST',data={'content':'no'},headers=headers)[0],403)
        self.assertEqual(self.request(method='POST',data={'content':'yes'},headers={'Origin':ORIGIN})[0],201)
        self.assertEqual(self.request(method='OPTIONS',headers={'Origin':'https://evil.example'})[0],403)

    def test_legacy_fails_closed_and_password_upgrade(self):
        self.assertEqual(self.request('/api/pastes/legacyPlain','PUT',{'title':'bad'})[0],403)
        self.assertEqual(self.request('/api/pastes/legacyPrivate')[0],404)
        self.assertEqual(self.request('/api/pastes/legacyPassword',password='old-password')[0],200)
        with sqlite3.connect(self.database) as connection:
            stored = connection.execute("SELECT password_hash FROM pastes WHERE id='legacyPassword'").fetchone()[0]
        self.assertTrue(stored.startswith('scrypt$16384$8$5$'))

    def test_owner_key_verification_does_not_consume_burn(self):
        paste = self.create(burn_after_read=True,visibility='private')
        path = '/api/pastes/'+paste['id']
        self.assertEqual(self.request(path+'/ownership',token='f'*64)[0],403)
        code, meta, _ = self.request(path+'/ownership',token=paste['owner_token'])
        self.assertEqual(code,200)
        self.assertTrue(meta['can_manage'])
        self.assertNotIn('content',meta)
        self.assertEqual(self.request(path,token=paste['owner_token'],headers={'X-Confirm-Burn':'yes'})[0],200)

    def test_storage_uses_hashes_and_never_returns_them(self):
        paste = self.create(password='good password')
        with sqlite3.connect(self.database) as connection:
            owner, password = connection.execute('SELECT owner_hash,password_hash FROM pastes WHERE id=?',(paste['id'],)).fetchone()
        self.assertEqual(owner, hashlib.sha256(paste['owner_token'].encode()).hexdigest())
        self.assertTrue(password.startswith('scrypt$'))
        result = self.request('/api/pastes/'+paste['id'],token=paste['owner_token'])[1]
        for secret in ['owner_token','owner_hash','password_hash']:
            self.assertNotIn(secret,result)

    def test_security_headers_errors_and_static(self):
        for path in ['/api/health','/api/not-existing','/api/pastes/missing']:
            _, body, headers = self.request(path)
            self.assertEqual(headers['Cache-Control'],'no-store')
            self.assertEqual(headers['X-Content-Type-Options'],'nosniff')
            self.assertIn("object-src 'none'",headers['Content-Security-Policy'])
        _, body, headers = self.request('/api/health')
        self.assertNotIn('paste_count',body)
        self.assertEqual(headers['X-Robots-Tag'],'noindex, nofollow, noarchive')
        self.assertIsInstance(self.request('/api/not-existing')[1],dict)
        self.assertIn("base-uri 'none'",self.request('/')[2]['Content-Security-Policy'])

    def test_password_challenge_does_not_lock_out(self):
        paste = self.create(password='read pass')
        path = '/api/pastes/'+paste['id']
        for _ in range(6):
            self.assertEqual(self.request(path)[0],404)
        self.assertEqual(self.request(path,password='read pass')[0],200)

    def test_password_budget_applies_to_raw_and_fork(self):
        paste = self.create(password='read pass')
        path = '/api/pastes/'+paste['id']
        for i in range(5):
            route = path+'/raw' if i % 2 else path
            self.assertEqual(self.request(route,password='wrong')[0],404)
        self.assertEqual(self.request(path+'/fork','POST',{},password='wrong')[0],429)
        self.assertEqual(self.request(path,token=paste['owner_token'])[0],200)

    def test_pagination_search_and_exact_has_more(self):
        for title, language in [('needle two','python'),('needle one','javascript')]:
            self.create(title=title,language=language,tags=['findme'])
        result = self.request('/api/pastes?q=needle&tag=findme&limit=2')[1]
        self.assertEqual(len(result['data']),2)
        self.assertFalse(result['has_more'])
        result = self.request('/api/pastes?q=needle&language=python&sort=popular')[1]
        self.assertEqual([p['title'] for p in result['data']],['needle two'])
        for suffix in ['page=2147483647','page=1x','limit=-1','sort=content','page=0']:
            self.assertEqual(self.request('/api/pastes?'+suffix)[0],400)

    def test_utf8_byte_limit(self):
        self.assertEqual(self.request(method='POST',data={'content':'😀'*(128*1024+1)})[0],413)
        paste = self.create(content='😀'*100)
        result = self.request('/api/pastes/'+paste['id'])[1]
        self.assertEqual(result['size'],400)

    def test_embed_refuses_all_protected_content(self):
        for options in [{'visibility':'private'},{'visibility':'unlisted'},{'password':'read pass'},{'burn_after_read':True}]:
            paste = self.create(**options)
            self.assertEqual(self.request('/api/pastes/'+paste['id']+'/embed',token=paste['owner_token'])[0],404)
        paste = self.create(content='<script>alert(1)</script>')
        self.assertEqual(self.request('/api/pastes/'+paste['id']+'/embed')[1]['content'],'<script>alert(1)</script>')
        status, _, headers = self.request('/embed/'+paste['id'])
        self.assertEqual(status,200)
        self.assertNotIn('X-Frame-Options',headers)
        self.assertIn('frame-ancestors *',headers['Content-Security-Policy'])

    def test_revision_preconditions_prevent_lost_updates(self):
        paste = self.create(content='original')
        path = '/api/pastes/'+paste['id']
        token = paste['owner_token']
        code, body, headers = self.request(path,token=token)
        self.assertEqual(body['revision'],1)
        tag = headers['ETag']
        self.assertEqual(self.request(path,'PUT',{'content':'missing guard'},token=token)[0],428)
        with concurrent.futures.ThreadPoolExecutor(max_workers=2) as pool:
            results = list(pool.map(lambda text: self.request(path,'PUT',{'content':text},token=token,headers={'If-Match':tag})[0], ['first tab','second tab']))
        self.assertEqual(sorted(results),[200,412])
        self.assertEqual(self.request(path,token=token)[1]['revision'],2)
        code, history, _ = self.request(path+'/revisions',token=token)
        self.assertEqual(code,200)
        self.assertEqual([r['revision'] for r in history['data']],[1])
        self.assertEqual(self.request(path+'/revisions/1',token=token)[1]['content'],'original')

    def test_history_requires_owner_and_never_leaks_credentials(self):
        paste = self.create(content='old secret',password='read pass')
        path = '/api/pastes/'+paste['id']
        token = paste['owner_token']
        self.assertEqual(self.request(path,'PUT',{'content':'redacted'},token=token,headers={'If-Match':f'"paste-{paste["id"]}-v1"'})[0],200)
        for suffix, method, data in [('/revisions','GET',None),('/revisions/1','GET',None),('/revisions/1/restore','POST',{}),('/revisions','DELETE',None)]:
            self.assertEqual(self.request(path+suffix,method,data,password='read pass')[0],403)
        meta = self.request(path+'/revisions',token=token)[1]
        self.assertNotIn('old secret',json.dumps(meta))
        saved = self.request(path+'/revisions/1',token=token)[1]
        self.assertEqual(saved['content'],'old secret')
        for key in ['password_hash','owner_hash','owner_token','visibility','expires_at']:
            self.assertNotIn(key,saved)
        self.assertEqual(self.request(path+'/revisions/1abc',token=token)[0],404)

    def test_restore_creates_new_revision_and_preserves_current_protection(self):
        paste = self.create(content='original',language='python',visibility='private',password='read pass',expires_in='1h',tags=['original'])
        path = '/api/pastes/'+paste['id']
        token = paste['owner_token']
        self.assertEqual(self.request(path,'PUT',{'content':'new','language':'json','tags':['new'],'visibility':'unlisted'},token=token,headers={'If-Match':f'"paste-{paste["id"]}-v1"'})[0],200)
        route = path+'/revisions/1/restore'
        self.assertEqual(self.request(route,'POST',{},token=token,headers={'If-Match':f'"paste-{paste["id"]}-v1"'})[0],412)
        code, restored, _ = self.request(route,'POST',{},token=token,headers={'If-Match':f'"paste-{paste["id"]}-v2"'})
        self.assertEqual(code,200)
        self.assertEqual(restored['revision'],3)
        self.assertEqual(restored['visibility'],'unlisted')
        self.assertEqual(restored['expires_at'],paste['expires_at'])
        self.assertTrue(restored['has_password'])
        current = self.request(path,password='read pass')[1]
        self.assertEqual(current['content'],'original')
        self.assertEqual(current['language'],'python')
        self.assertEqual(current['tags'],['original'])
        self.assertEqual(self.request(path+'/revisions/2',token=token)[1]['content'],'new')

    def test_history_retention_clear_and_lifecycle(self):
        paste = self.create()
        path = '/api/pastes/'+paste['id']
        token = paste['owner_token']
        for revision in range(1,13):
            if revision == 7:
                self.ip = f'198.51.100.{API.sequence}'
            code, _, _ = self.request(path,'PUT',{'content':f'content {revision}'},token=token,headers={'If-Match':f'"paste-{paste["id"]}-v{revision}"'})
            self.assertEqual(code,200)
        history = self.request(path+'/revisions',token=token)[1]
        self.assertEqual(len(history['data']),10)
        self.assertEqual(history['current_revision'],13)
        self.assertEqual(self.request(path+'/revisions/1',token=token)[0],404)
        self.assertEqual(self.request(path+'/revisions','DELETE',token=token,headers={'If-Match':f'"paste-{paste["id"]}-v12"'})[0],412)
        self.assertEqual(self.request(path+'/revisions','DELETE',token=token,headers={'If-Match':f'"paste-{paste["id"]}-v13"'})[0],200)
        self.assertEqual(self.request(path+'/revisions',token=token)[1]['data'],[])
        self.assertEqual(self.request(path,token=token)[1]['content'],'content 12')
        self.assertEqual(self.request(path,'PUT',{'title':'kept'},token=token,headers={'If-Match':f'"paste-{paste["id"]}-v13"'})[0],200)
        self.assertEqual(self.request(path,'DELETE',token=token)[0],200)
        with sqlite3.connect(self.database) as connection:
            count = connection.execute('SELECT count(*) FROM paste_revisions WHERE paste_id=?',(paste['id'],)).fetchone()[0]
        self.assertEqual(count,0)

    def test_one_time_history_disabled_and_forks_start_fresh(self):
        paste = self.create(burn_after_read=True)
        path = '/api/pastes/'+paste['id']
        token = paste['owner_token']
        self.assertEqual(self.request(path+'/revisions',token=token)[0],409)
        self.assertEqual(self.request(path,'PUT',{'content':'another copy'},token=token,headers={'If-Match':f'"paste-{paste["id"]}-v1"'})[0],409)
        self.assertEqual(self.request(path,token=token,headers={'X-Confirm-Burn':'yes'})[1]['content'],'hello secret')
        regular = self.create()
        route = '/api/pastes/'+regular['id']
        self.assertEqual(self.request(route,'PUT',{'content':'new'},token=regular['owner_token'],headers={'If-Match':f'"paste-{regular["id"]}-v1"'})[0],200)
        code, fork, _ = self.request(route+'/fork','POST',{})
        self.assertEqual(code,201)
        self.assertEqual(fork['revision'],1)
        self.assertEqual(self.request('/api/pastes/'+fork['id']+'/revisions',token=fork['owner_token'])[1]['data'],[])
        self.assertEqual(self.request(route+'/revisions/1',token=fork['owner_token'])[0],403)

    def test_expired_history_is_inaccessible_and_cascades(self):
        paste = self.create()
        path = '/api/pastes/'+paste['id']
        self.assertEqual(self.request(path,'PUT',{'content':'new'},token=paste['owner_token'],headers={'If-Match':f'"paste-{paste["id"]}-v1"'})[0],200)
        with sqlite3.connect(self.database) as connection:
            connection.execute("UPDATE pastes SET expires_at=datetime('now','-1 second') WHERE id=?",(paste['id'],))
        self.assertEqual(self.request(path+'/revisions/1',token=paste['owner_token'])[0],404)
        with sqlite3.connect(self.database) as connection:
            connection.execute('PRAGMA foreign_keys=OFF')
            connection.execute("DELETE FROM pastes WHERE expires_at<=datetime('now')")
            self.assertEqual(connection.execute('SELECT count(*) FROM paste_revisions WHERE paste_id=?',(paste['id'],)).fetchone()[0],0)

    def test_failed_tag_write_rolls_back_content_and_snapshot(self):
        paste = self.create(content='keep original',tags=['keep'])
        path = '/api/pastes/'+paste['id']
        with sqlite3.connect(self.database) as connection:
            connection.execute(f"CREATE TRIGGER fail_revision_fixture BEFORE INSERT ON tags WHEN NEW.paste_id='{paste['id']}' BEGIN SELECT RAISE(ABORT,'fixture'); END")
        try:
            self.assertEqual(self.request(path,'PUT',{'content':'partial change','tags':['new']},token=paste['owner_token'],headers={'If-Match':f'"paste-{paste["id"]}-v1"'})[0],503)
            current = self.request(path,token=paste['owner_token'])[1]
            self.assertEqual(current['revision'],1)
            self.assertEqual(current['content'],'keep original')
            self.assertEqual(current['tags'],['keep'])
            self.assertEqual(self.request(path+'/revisions',token=paste['owner_token'])[1]['data'],[])
        finally:
            with sqlite3.connect(self.database) as connection:
                connection.execute('DROP TRIGGER fail_revision_fixture')

    def test_forged_forwarded_for_does_not_bypass_limiter(self):
        codes = []
        for i in range(40):
            codes.append(self.request('/api/health',headers={'X-Forwarded-For':f'198.51.100.{i}'})[0])
        self.assertIn(429,codes)

    def test_z_distributed_password_work_has_a_global_budget(self):
        # Last scenario: intentionally exhausts the shared cryptographic budget.
        owned = self.create(content='owner access stays available')
        def attempt(number):
            return self.request(method='POST',data={'content':'fixture','password':'distributed password'},headers={'X-Real-IP':f'203.0.113.{number+1}'})
        with concurrent.futures.ThreadPoolExecutor(max_workers=4) as pool:
            results = list(pool.map(attempt,range(32)))
        codes = [code for code,_,_ in results]
        self.assertIn(201,codes)
        self.assertIn(429,codes)
        self.assertTrue(all(code in (201,429) for code in codes),codes)
        for code,body,headers in results:
            if code == 429:
                self.assertIn('Password checks are busy',body['error'])
                self.assertEqual(headers['Retry-After'],'1')
                self.assertEqual(headers['Cache-Control'],'no-store')
        self.assertEqual(self.request('/api/health')[0],200)
        self.assertEqual(self.request('/api/pastes/'+owned['id'],token=owned['owner_token'])[0],200)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--binary',type=Path,required=True)
    args = parser.parse_args()
    binary = args.binary.resolve(strict=True)
    with tempfile.TemporaryDirectory(prefix='pastebox-regression-') as directory:
        root = Path(directory)
        (root/'public').mkdir()
        (root/'public'/'index.html').write_text('<!doctype html><title>test</title>')
        with socket.socket() as sock:
            sock.bind(('127.0.0.1',0))
            port = sock.getsockname()[1]
        config = json.loads((Path(__file__).parent/'config.json').read_text())
        config['listeners'][0]['port'] = port
        config['app']['document_root'] = str(root/'public')
        config['db_clients'][0]['filename'] = str(root/'test.db')
        (root/'config.json').write_text(json.dumps(config))
        with sqlite3.connect(root/'test.db') as connection:
            connection.execute("CREATE TABLE pastes(id TEXT PRIMARY KEY,title TEXT DEFAULT 'Legacy',content TEXT,language TEXT DEFAULT 'plaintext',created_at DATETIME DEFAULT CURRENT_TIMESTAMP,views INTEGER DEFAULT 0,password_hash TEXT,visibility TEXT DEFAULT 'public')")
            connection.execute("INSERT INTO pastes(id,content) VALUES('legacyPlain','fixture')")
            connection.execute("INSERT INTO pastes(id,content,visibility) VALUES('legacyPrivate','fixture','private')")
            connection.execute("INSERT INTO pastes(id,content,password_hash) VALUES('legacyPassword','fixture',?)",(hashlib.sha256(b'old-password').hexdigest(),))
        env = {**os.environ,'PASTEBOX_CONFIG':str(root/'config.json'),'PASTEBOX_TRUST_PROXY':'1'}
        API.base = f'http://127.0.0.1:{port}'
        API.database = str(root/'test.db')
        with (root/'server.log').open('w+') as logfile:
            server = subprocess.Popen([str(binary)],cwd=root,env=env,stdout=logfile,stderr=logfile)
            try:
                for _ in range(100):
                    try:
                        urllib.request.urlopen(API.base+'/api/health',timeout=0.2).close()
                        break
                    except (OSError,urllib.error.URLError):
                        if server.poll() is not None:
                            logfile.seek(0)
                            raise RuntimeError(logfile.read())
                        time.sleep(0.05)
                suite = unittest.defaultTestLoader.loadTestsFromTestCase(API)
                result = unittest.TextTestRunner(verbosity=2).run(suite)
                if not result.wasSuccessful():
                    logfile.seek(0)
                    print(logfile.read()[-4000:])
                return 0 if result.wasSuccessful() else 1
            finally:
                server.terminate()
                try:
                    server.wait(timeout=10)
                except subprocess.TimeoutExpired:
                    server.kill()
                    server.wait()

if __name__ == '__main__':
    raise SystemExit(main())
