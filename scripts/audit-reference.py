#!/usr/bin/env python3
"""Reproducible source comparison, not a legal originality certification.

Usage: python scripts/audit-reference.py --reference NAME=/path/to/repository ...
Prints exact-file matches and shared 30-token windows after dropping C comments.
Identifiers, strings, constants and operators retain their original spelling.
Third-party license texts and generated/vendor code are excluded from app scope.
"""
import argparse
import collections
import hashlib
import json
from pathlib import Path
import re
import subprocess

TOKEN = re.compile(r'/\*.*?\*/|//[^\n]*|"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'|[A-Za-z_]\w*|0[xX][0-9a-fA-F]+|\d+(?:\.\d+)?|==|!=|<=|>=|->|&&|\|\||<<|>>|\S', re.S)
WINDOW = 30

def tokens(path):
    return [t for t in TOKEN.findall(path.read_text(encoding='utf-8', errors='replace'))
            if not t.startswith(('/*', '//'))]

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--reference', action='append', required=True)
    args = parser.parse_args()
    app = Path(__file__).resolve().parents[1]
    refs = {}
    revisions = {}
    exact = collections.defaultdict(list)
    windows = collections.defaultdict(set)
    for entry in args.reference:
        name, directory = entry.split('=', 1)
        base = Path(directory).resolve()
        if not base.is_dir(): parser.error(f'Missing reference directory: {base}')
        paths = [p for p in base.rglob('*') if p.is_file() and p.suffix in ('.c', '.h', '.overlay', '.dtsi', '.cmake') and '.git' not in p.parts]
        refs[name] = len(paths)
        result = subprocess.run(['git', '-C', str(base), 'rev-parse', 'HEAD'],
                                capture_output=True, text=True)
        revisions[name] = result.stdout.strip() if result.returncode == 0 else None
        for p in paths:
            key = name + '/' + p.relative_to(base).as_posix()
            exact[hashlib.sha256(p.read_bytes()).hexdigest()].append(key)
            ts = tokens(p)
            for i in range(len(ts) - WINDOW + 1):
                windows[tuple(ts[i:i+WINDOW])].add(key)
    results = []
    paths = sorted((app/'src').glob('*')) + [app/'boards/xiao_ble.overlay']
    for p in paths:
        if not p.is_file(): continue
        ts = tokens(p)
        matches = collections.Counter()
        for i in range(len(ts) - WINDOW + 1):
            matches.update(windows.get(tuple(ts[i:i+WINDOW]), ()))
        results.append({'file': p.relative_to(app).as_posix(),
                        'sha256': hashlib.sha256(p.read_bytes()).hexdigest(),
                        'tokens_without_comments': len(ts),
                        'identical_files': exact.get(hashlib.sha256(p.read_bytes()).hexdigest(), []),
                        'shared_30_token_windows': dict(sorted(matches.items()))})
    print(json.dumps({'window_tokens': WINDOW, 'reference_file_counts': refs,
                      'reference_revisions': revisions,
                      'application_files': results}, indent=2))

if __name__ == '__main__': main()
