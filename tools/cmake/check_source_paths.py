#!/usr/bin/env python3
"""CMakeに列挙したAHO/VSLソースのパスを、大小文字も含めて検証する。"""

import argparse
from pathlib import Path
import re
import sys


SOURCE_PATH = re.compile(r'\b(?:AHO|VSL)/(?:include|src)/[\w./-]+\.(?:hpp|cpp|h|c|mm|m)\b')
MANIFESTS = ('AHOLi/CMakeLists.txt', 'VSLi/CMakeLists.txt', 'AHO/CMakeLists.txt')


def exact_path(path):
    current = Path(path.anchor)
    for part in path.parts[1:]:
        if not current.is_dir() or part not in {entry.name for entry in current.iterdir()}:
            return False
        current /= part
    return current.is_file()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--root', type=Path, default=Path(__file__).resolve().parents[2])
    root = parser.parse_args().root.resolve()
    errors = 0
    checked = 0
    for name in MANIFESTS:
        manifest = root / name
        for number, line in enumerate(manifest.read_text(encoding='utf-8-sig').splitlines(), 1):
            for match in SOURCE_PATH.finditer(line.split('#', 1)[0]):
                checked += 1
                if not exact_path(manifest.parent / match[0]):
                    print(f'{name}:{number}: missing or incorrectly cased source: {match[0]}', file=sys.stderr)
                    errors += 1
    print(f'Checked {checked} source paths; {errors} errors.')
    return bool(errors)


if __name__ == '__main__':
    sys.exit(main())
