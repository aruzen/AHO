#!/usr/bin/env python3
"""GitHubのイベントからstyle検査の基点と実行範囲を決める。"""

import json
import os
from pathlib import Path
import subprocess

import check


CONTROL_FILES = {'.clang-format', '.clang-tidy', '.github/workflows/style.yml', 'cmake/StyleChecks.cmake'}


def parent_or_empty_tree(root):
    try:
        return check.git_output(root, 'rev-parse', '--verify', 'HEAD^').decode().strip()
    except subprocess.CalledProcessError:
        return check.git_output(root, 'hash-object', '-w', '-t', 'tree', '--stdin', input=b'').decode().strip()


def plan(root, event_name, event):
    inputs = event.get('inputs') or {}
    full = event_name == 'release' or (event_name == 'workflow_dispatch' and inputs.get('full') in (True, 'true'))
    base = ''
    if full:
        changed = set()
        selected = set(check.tracked_files(root))
    else:
        if event_name == 'pull_request':
            pull = event['pull_request']
            head = pull['head']['sha']
            if check.git_output(root, 'rev-parse', 'HEAD').decode().strip() != head:
                raise ValueError('PRのhead commitをcheckoutしてから検査してください')
            base = check.git_output(root, 'merge-base', pull['base']['sha'], head).decode().strip()
        elif event_name == 'push':
            before = event['before']
            base = before if before.strip('0') else check.git_output(
                root, 'hash-object', '-w', '-t', 'tree', '--stdin', input=b'').decode().strip()
        else:
            base = inputs.get('base_ref') or parent_or_empty_tree(root)
        # Output only a resolved object ID, never a user-provided ref expression.
        base = check.git_output(root, 'rev-parse', '--verify', '--end-of-options', base).decode().strip()
        changed = check.changed_files(root, base)
        selected = set(check.tracked_files(root)) & changed
    contracts = full or any(file.startswith('tools/style/') or file in CONTROL_FILES for file in changed)
    return dict(full=full, base=base, files=sorted(selected), checks=bool(selected), contracts=contracts,
                needed=bool(selected) or contracts)


def main():
    root = Path(__file__).resolve().parents[2]
    event = json.loads(Path(os.environ['GITHUB_EVENT_PATH']).read_text())
    result = plan(root, os.environ['GITHUB_EVENT_NAME'], event)
    output = root / 'build/style-scope.json'
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(json.dumps(result, indent=2) + '\n')
    with open(os.environ['GITHUB_OUTPUT'], 'a') as destination:
        for name in ['full', 'base', 'checks', 'contracts', 'needed']:
            value = result[name]
            destination.write(f'{name}={str(value).lower() if isinstance(value, bool) else value}\n')
    print(json.dumps(result, ensure_ascii=False))


if __name__ == '__main__':
    main()
