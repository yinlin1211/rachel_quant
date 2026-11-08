#!/usr/bin/env python3
"""Upload bt5_engine/ to GitHub via the REST API (github.com:443 is blocked)."""
import base64
import json
import os
import sys
import time
import urllib.error
import urllib.request

TOKEN = open('/home/intern2/rachel/githubtoken.txt').readline().split('github token:')[1].strip()
API = 'https://api.github.com'
REPO = 'yinlin1211/rachel_quant'
ROOT = '/home/intern2/rachel/gh_stage'


def req(method, path, data=None, retries=8, timeout=90):
    body = json.dumps(data).encode() if data is not None else None
    last = None
    for i in range(retries):
        try:
            r = urllib.request.Request(API + path, data=body, method=method)
            r.add_header('Authorization', 'token ' + TOKEN)
            r.add_header('Accept', 'application/vnd.github+json')
            r.add_header('Content-Type', 'application/json')
            with urllib.request.urlopen(r, timeout=timeout) as resp:
                return json.load(resp)
        except urllib.error.HTTPError as e:
            last = 'HTTP %s %s' % (e.code, e.read().decode()[:300])
            print('  retry %d: %s' % (i + 1, last), file=sys.stderr, flush=True)
            if e.code in (401, 403, 404, 422):
                raise
        except Exception as e:
            last = '%s: %s' % (type(e).__name__, e)
            print('  retry %d: %s' % (i + 1, last), file=sys.stderr, flush=True)
        time.sleep(min(2 ** i, 10))
    raise RuntimeError('failed %s: %s' % (path, last))


def main():
    ref = req('GET', '/repos/%s/git/ref/heads/main' % REPO)
    head = ref['object']['sha']
    commit = req('GET', '/repos/%s/git/commits/%s' % (REPO, head))
    base_tree = commit['tree']['sha']
    print('head=%s tree=%s' % (head, base_tree), flush=True)

    entries = []
    n = 0
    for dirpath, dirnames, filenames in os.walk(ROOT):
        dirnames.sort()
        filenames.sort()
        for fn in filenames:
            local = os.path.join(dirpath, fn)
            rel = os.path.relpath(local, ROOT)
            data = open(local, 'rb').read()
            blob = req('POST', '/repos/%s/git/blobs' % REPO,
                       {'content': base64.b64encode(data).decode('ascii'),
                        'encoding': 'base64'})
            mode = '100755' if (fn.endswith('.so') or fn.endswith('.sh')) else '100644'
            entries.append({'path': rel, 'mode': mode, 'type': 'blob', 'sha': blob['sha']})
            n += 1
            print('[%d] %s (%d bytes)' % (n, rel, len(data)), flush=True)

    tree = req('POST', '/repos/%s/git/trees' % REPO,
               {'base_tree': base_tree, 'tree': entries})
    msg = ('Add bt5 engine package, benchmark, and optimized tick loop\n\n'
           'Co-Authored-By: Claude Code <noreply@anthropic.com>')
    new_commit = req('POST', '/repos/%s/git/commits' % REPO,
                     {'message': msg, 'tree': tree['sha'], 'parents': [head]})
    req('PATCH', '/repos/%s/git/refs/heads/main' % REPO, {'sha': new_commit['sha']})
    print('DONE commit %s' % new_commit['sha'], flush=True)


if __name__ == '__main__':
    main()
