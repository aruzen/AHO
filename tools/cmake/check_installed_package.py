#!/usr/bin/env python3
"""install先を移動し、依存を別prefixへ分離してAHO::AHOLiだけの利用を検証する。"""

import argparse
from pathlib import Path
import shutil
import subprocess
import tempfile


def run(*arguments):
    subprocess.run(arguments, check=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--build-dir', type=Path, required=True)
    parser.add_argument('--cmake-arg', action='append', default=[])
    args = parser.parse_args()
    build = args.build_dir.resolve()
    cache = {}
    for line in (build / 'CMakeCache.txt').read_text().splitlines():
        if line and not line.startswith(('#', '//')) and '=' in line:
            name, value = line.split('=', 1)
            cache[name.split(':', 1)[0]] = value

    work = Path(tempfile.mkdtemp(prefix='package-check-', dir=build))
    original, relocated, dependency = (work / name for name in ('original', 'relocated', 'dependency'))
    run('cmake', '--install', str(build), '--prefix', str(original), '--config', cache.get('CMAKE_BUILD_TYPE') or 'Release')
    original.rename(relocated)
    header = Path('include/saitekikuukan.hpp')
    (dependency / header.parent).mkdir(parents=True)
    shutil.move(relocated / header, dependency / header)
    config = next(relocated.rglob('saitekikuukanConfig.cmake')).parent
    destination = dependency / config.relative_to(relocated)
    destination.parent.mkdir(parents=True, exist_ok=True)
    shutil.move(config, destination)

    for package in ('AHO', 'VSL'):
        for export in (relocated / 'share' / package / 'cmake').glob('*Targets*.cmake'):
            text = export.read_text()
            if str(build) in text or cache.get('CMAKE_HOME_DIRECTORY', '\0') in text:
                raise RuntimeError(f'開発ディレクトリがexportに残っています: {export}')

    prefixes = [str(relocated), str(dependency), cache.get('CMAKE_PREFIX_PATH', '')]
    source = Path(__file__).resolve().parent / 'consumer'
    consumer = work / 'consumer'
    options = [f'-DCMAKE_PREFIX_PATH={";".join(prefixes)}']
    for name in ('CMAKE_CXX_COMPILER', 'CMAKE_BUILD_TYPE', 'CMAKE_OSX_SYSROOT', 'CMAKE_OSX_ARCHITECTURES'):
        if cache.get(name):
            options.append(f'-D{name}={cache[name]}')
    run('cmake', '-S', str(source), '-B', str(consumer), '-G', 'Ninja', *options, *args.cmake_arg)
    run('cmake', '--build', str(consumer), '--parallel', '2')
    run('ctest', '--test-dir', str(consumer), '--output-on-failure')
    print(f'Relocated package verified: {work}')


if __name__ == '__main__':
    main()
