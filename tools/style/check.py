#!/usr/bin/env python3
"""確定済みのAHO/VSL規約を検査し、既存違反との差分を報告する。"""

import argparse
from collections import Counter
from concurrent.futures import ThreadPoolExecutor, as_completed
import difflib
import hashlib
import json
import os
from pathlib import Path
import re
import shlex
import shutil
import subprocess
import sys


ROOT = Path(__file__).resolve().parents[2]
ROOTS = ('AHOLi/AHO/', 'VSLi/VSL/', 'tools/style/')
SUFFIXES = {'.c', '.cc', '.cpp', '.cxx', '.h', '.hh', '.hpp', '.hxx', '.inl'}
NAMING_DIAGNOSTIC = re.compile(r'^(.*):(\d+):(\d+): (?:warning|error): (.*?) \[readability-identifier-naming\]$')


def run(command, **kwargs):
    return subprocess.run(command, text=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE, **kwargs)


def require_tool(name):
    path = shutil.which(name)
    if not path:
        raise RuntimeError(f'必要なツールがありません: {name}')
    version = run([path, '--version'])
    if not re.search(r'\bversion 20\.', version.stdout + version.stderr):
        raise RuntimeError(f'ツールのメジャーバージョンを20へ揃えてください: {name}')
    return path


def tracked_files(root):
    result = subprocess.run(['git', 'ls-files', '-z'], cwd=root, check=True, stdout=subprocess.PIPE)
    return sorted(path.decode() for path in result.stdout.split(b'\0')
                  if path and path.decode().startswith(ROOTS) and Path(path.decode()).suffix in SUFFIXES)


def diagnostic(root, file, line, column, rule, symbol, message, source=None, code_lines=None):
    if source is None:
        lines = (root / file).read_text().splitlines()
        source = lines[line - 1].strip() if 0 < line <= len(lines) else ''
    identity = [file, rule, symbol, source]
    result = dict(file=file, line=line, column=column, rule=rule, symbol=symbol, message=message,
                  source=source, key=hashlib.sha256(json.dumps(identity, ensure_ascii=False).encode()).hexdigest())
    if code_lines is not None:
        result['code_lines'] = code_lines
    return result


def format_check(root, file, tool):
    original = (root / file).read_bytes()
    result = subprocess.run([tool, '--style=file', str(root / file)], stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    if result.returncode:
        raise RuntimeError(result.stderr.decode())
    if result.stdout == original:
        return []
    difference = list(difflib.unified_diff(original.decode().splitlines(), result.stdout.decode().splitlines(), n=0))
    hunk = next((line for line in difference if line.startswith('@@')), '@@ -1 +1 @@')
    line = int(re.search(r'-(\d+)', hunk)[1])
    # 変更されたファイルは全体の整形を要求し、既存の整形違反へ紛れ込ませない。
    return [diagnostic(root, file, line, 1, 'clang-format', '', 'clang-formatの結果と一致しません',
                       hashlib.sha256(original).hexdigest())]


def compile_jobs(databases, selected, root):
    jobs = {}
    for database in databases:
        database = database.resolve()
        if database.is_dir():
            database /= 'compile_commands.json'
        for entry in json.loads(database.read_text()):
            directory = Path(entry['directory']).resolve()
            file = (directory / entry['file']).resolve()
            try:
                relative = file.relative_to(root).as_posix()
            except ValueError:
                continue
            if relative not in selected:
                continue
            arguments = entry.get('arguments') or shlex.split(entry['command'], posix=os.name != 'nt')
            normalized = []
            skip = False
            for argument in arguments:
                if skip:
                    skip = False
                elif argument in ('-o', '-MF', '-MT', '-MQ'):
                    skip = True
                elif argument not in ('-c', '-MD', '-MMD'):
                    normalized.append(argument)
            key = hashlib.sha256(json.dumps([relative, str(directory), normalized]).encode()).hexdigest()[:20]
            jobs[key] = (relative, dict(directory=str(directory), file=str(file), arguments=arguments))
    return sorted(jobs.items())


def apple_arguments():
    if sys.platform != 'darwin':
        return []
    sdk = subprocess.check_output(['xcrun', '--show-sdk-path'], text=True).strip()
    return ['--extra-arg=-isysroot', '--extra-arg=' + sdk,
            '--extra-arg=-isystem', '--extra-arg=' + sdk + '/usr/include/c++/v1']


def translation_check(job, args, selected, tidy, extra):
    key, (file, entry) = job
    directory = args.output / 'tu' / key
    directory.mkdir(parents=True, exist_ok=True)
    (directory / 'compile_commands.json').write_text(json.dumps([entry]))
    diagnostics = []
    coverage = set()
    if 'naming' in args.checks:
        result = run([tidy, '-p=' + str(directory), '--config-file=' + str(args.root / '.clang-tidy'),
                      '--quiet', *extra, entry['file']])
        (directory / 'naming.log').write_text(result.stdout + result.stderr)
        if result.returncode:
            raise RuntimeError(f'{file}: clang-tidyの解析に失敗しました。{directory / "naming.log"}')
        for line in (result.stdout + result.stderr).splitlines():
            match = NAMING_DIAGNOSTIC.match(line)
            if not match:
                continue
            try:
                relative = Path(match[1]).resolve().relative_to(args.root).as_posix()
            except ValueError:
                continue
            if relative in selected:
                diagnostics.append(diagnostic(args.root, relative, int(match[2]), int(match[3]),
                                              'readability-identifier-naming', match[4], match[4]))
    if 'layout' in args.checks:
        result = run([str(args.ast_tool), '--root=' + str(args.root), '-p=' + str(directory),
                      '--in-class-min-lines=' + str(args.in_class_min_lines), *extra, entry['file']])
        (directory / 'layout.log').write_text(result.stderr)
        if result.returncode:
            raise RuntimeError(f'{file}: AST解析に失敗しました。{directory / "layout.log"}')
        for line in result.stdout.splitlines():
            item = json.loads(line)
            if 'coverage' in item:
                coverage.update(item['coverage'])
                continue
            if item['file'] in selected:
                diagnostics.append(diagnostic(args.root, **item))
    return diagnostics, coverage


def load_master(directory, selected, in_class_min_lines):
    diagnostics = []
    if directory.exists() and not directory.is_dir():
        raise ValueError(f'masterにはディレクトリを指定してください: {directory}')
    for file in sorted(selected):
        path = directory / (file + '.json')
        if not path.exists():
            continue
        master = json.loads(path.read_text())
        if (not isinstance(master, dict) or master.get('version') != 1 or master.get('llvm_major') != 20 or
                master.get('in_class_min_lines') != in_class_min_lines):
            raise ValueError(f'masterの形式・LLVM・行数条件が一致しません: {path}')
        if not isinstance(master.get('diagnostics'), list):
            raise ValueError(f'masterの診断一覧が不正です: {path}')
        for item in master['diagnostics']:
            if (not isinstance(item, dict) or item.get('file') != file or
                    not isinstance(item.get('key'), str) or
                    type(item.get('count')) is not int or item['count'] <= 0):
                raise ValueError(f'masterの対象ファイル・件数が不正です: {path}')
            diagnostics.append(item)
    return diagnostics


def update_master(directory, selected, diagnostics, in_class_min_lines, remove_deleted=False):
    # 検証済みの既存登録だけを減らす。新しい違反を許可へ追加しない。
    selected = set(selected)
    if remove_deleted:
        for path in directory.rglob('*.json'):
            file = path.relative_to(directory).as_posix()[:-5]
            if not file.startswith(ROOTS) or Path(file).suffix not in SUFFIXES:
                raise ValueError(f'masterの対象パスが不正です: {path}')
            selected.add(file)
    existing = load_master(directory, selected, in_class_min_lines)
    remaining = Counter(item['key'] for item in diagnostics)
    grouped = {}
    for item in existing:
        count = min(item['count'], remaining[item['key']])
        if count:
            grouped.setdefault(item['file'], []).append({**item, 'count': count})
            remaining[item['key']] -= count
    for file in sorted(selected):
        path = directory / (file + '.json')
        if file in grouped:
            master = dict(version=1, llvm_major=20, in_class_min_lines=in_class_min_lines,
                          diagnostics=grouped[file])
            path.write_text(json.dumps(master, indent=2, ensure_ascii=False) + '\n')
        elif path.exists():
            path.unlink()


def compare(diagnostics, master):
    # 同じ違反を複製した場合も検出する。TUをまたぐ重複除去は呼び出し側で行う。
    remaining = Counter()
    for item in master:
        remaining[item['key']] += item['count']
    new = []
    for item in diagnostics:
        if remaining[item['key']]:
            remaining[item['key']] -= 1
        else:
            new.append(item)
    return new


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--root', type=Path, default=ROOT)
    parser.add_argument('--database', type=Path, action='append', default=[])
    parser.add_argument('--ast-tool', type=Path)
    parser.add_argument('--checks', default='format,naming,layout')
    parser.add_argument('--clang-format', default='clang-format')
    parser.add_argument('--clang-tidy', default='clang-tidy')
    parser.add_argument('--jobs', type=int, default=2)
    parser.add_argument('--in-class-min-lines', type=int, default=5, help='クラス外定義を要求する本体の最小コード行数。既定5、0は全件')
    parser.add_argument('--output', type=Path)
    parser.add_argument('--master', type=Path, help='ソースごとの既存違反を記録するmasterディレクトリ')
    parser.add_argument('--update-master', action='store_true', help='検査したファイルの解消済み登録だけを削除する')
    parser.add_argument('--strict', action='store_true', help='既存違反も含めて失敗にする')
    parser.add_argument('files', nargs='*', help='指定時はtracked対象ファイルの一部を検査')
    args = parser.parse_args()
    args.root = args.root.resolve()
    args.checks = set(args.checks.split(','))
    if not args.checks or args.checks - {'format', 'naming', 'layout'}:
        parser.error('--checksはformat,naming,layoutの組み合わせです')
    if args.jobs < 1:
        parser.error('--jobsは1以上です')
    if args.in_class_min_lines < 0:
        parser.error('--in-class-min-linesは0以上です')
    args.output = (args.output or args.root / 'build/style-report').resolve()
    args.master = (args.master or args.root / 'tools/style/master').resolve()
    if args.update_master and (args.strict or args.checks != {'format', 'naming', 'layout'}):
        parser.error('masterの更新はstrictなし・全検査でのみ可能です')
    selected = set(tracked_files(args.root))
    if args.files:
        requested = {Path(file).as_posix() for file in args.files}
        if requested - selected:
            parser.error(f'対象外または未追跡のファイルです: {sorted(requested - selected)}')
        selected = requested
    if not selected:
        parser.error('検査対象がありません')
    formatter = require_tool(args.clang_format) if 'format' in args.checks else None
    tidy = require_tool(args.clang_tidy) if 'naming' in args.checks else None
    args.output.mkdir(parents=True, exist_ok=True)
    diagnostics = []
    coverage = set()
    if formatter:
        with ThreadPoolExecutor(max_workers=args.jobs) as executor:
            for items in executor.map(lambda file: format_check(args.root, file, formatter), sorted(selected)):
                diagnostics.extend(items)
    jobs = []
    if args.checks & {'naming', 'layout'}:
        if not args.database or ('layout' in args.checks and (not args.ast_tool or not args.ast_tool.is_file())):
            parser.error('命名・配置検査には--database、配置検査には--ast-toolが必要です')
        args.ast_tool = args.ast_tool.resolve() if args.ast_tool else None
        # ヘッダー指定では全TUを読み、ソースだけの指定では該当する全コンパイル条件を読む。
        sources_only = args.files and all(Path(file).suffix in {'.c', '.cc', '.cpp', '.cxx'} for file in selected)
        job_files = selected if sources_only else set(tracked_files(args.root))
        jobs = compile_jobs(args.database, job_files, args.root)
        if not jobs:
            parser.error('compile_commands.jsonに対象の翻訳単位がありません')
        extra = apple_arguments()
        with ThreadPoolExecutor(max_workers=args.jobs) as executor:
            futures = {executor.submit(translation_check, job, args, selected, tidy, extra): job for job in jobs}
            try:
                for count, future in enumerate(as_completed(futures), 1):
                    items, seen = future.result()
                    diagnostics.extend(items)
                    coverage.update(seen)
                    print(f'[{count}/{len(jobs)}] {futures[future][1][0]}', flush=True)
            except Exception:
                for future in futures:
                    future.cancel()
                raise
    mapped = {job[1][0] for job in jobs}
    unmapped = sorted(file for file in selected if Path(file).suffix in {'.c', '.cc', '.cpp', '.cxx'} and file not in mapped)
    unparsed = sorted(file for file in selected if Path(file).suffix in {'.h', '.hh', '.hpp', '.hxx', '.inl'} and file not in coverage)
    if args.checks & {'naming', 'layout'}:
        for file in unmapped:
            digest = hashlib.sha256((args.root / file).read_bytes()).hexdigest()
            diagnostics.append(diagnostic(args.root, file, 1, 1, 'aho-missing-command', '', 'コンパイルDBへソースを登録してください', digest))
    if 'layout' in args.checks:
        for file in unparsed:
            digest = hashlib.sha256((args.root / file).read_bytes()).hexdigest()
            diagnostics.append(diagnostic(args.root, file, 1, 1, 'aho-unparsed-header', '', 'どのTUからも解析されていないヘッダーです', digest))
    unique = {(item['file'], item['line'], item['column'], item['rule'], item['symbol']): item for item in diagnostics}
    diagnostics = sorted(unique.values(), key=lambda item: (item['file'], item['line'], item['column'], item['rule']))
    master = load_master(args.master, selected, args.in_class_min_lines) if not args.strict else []
    new = compare(diagnostics, master)
    report = dict(files=len(selected), translation_units=len(jobs), in_class_min_lines=args.in_class_min_lines,
                  diagnostics=diagnostics, new=new,
                  unmapped_sources=unmapped, unparsed_headers=unparsed if 'layout' in args.checks else [])
    (args.output / 'report.json').write_text(json.dumps(report, indent=2, ensure_ascii=False) + '\n')
    if args.update_master:
        if unmapped or unparsed:
            raise ValueError('未登録ソース・未解析ヘッダーがあるためmasterを更新しません')
        update_master(args.master, selected, diagnostics, args.in_class_min_lines, remove_deleted=not args.files)
    for item in new:
        print(f"{item['file']}:{item['line']}:{item['column']}: {item['message']} [{item['rule']}]")
    print(f'対象{len(selected)}ファイル/{len(jobs)}TU、違反{len(diagnostics)}件、新規{len(new)}件。{args.output / "report.json"}')
    if unmapped and args.checks & {'naming', 'layout'}:
        print('DB外のソース（整形のみ）: ' + ', '.join(unmapped))
    return bool(new)


if __name__ == '__main__':
    try:
        sys.exit(main())
    except (OSError, ValueError, RuntimeError, subprocess.CalledProcessError) as error:
        print(error, file=sys.stderr)
        sys.exit(2)
