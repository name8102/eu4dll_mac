#!/usr/bin/env python3
"""Build a platform ZIP from verified build outputs and repository resources."""
import argparse
from pathlib import Path
import platform
import subprocess
import zipfile

ROOT = Path(__file__).resolve().parents[1]


def package(build, output, target):
    files = {
        'install.sh': ROOT / 'install.sh',
        'README.md': ROOT / 'readme.md',
        'LICENSE': ROOT / 'LICENSE',
        'CHANGELOG.md': ROOT / 'CHANGELOG.md',
        'THIRD_PARTY_NOTICES.md': ROOT / 'THIRD_PARTY_NOTICES.md',
        'licenses/cpp-pinyin-LICENSE': build / '_deps/cpp-pinyin-src/LICENSE',
    }
    if target == 'linux':
        files.update({
            'libeu4dll_linux.so': build / 'libeu4dll_linux.so',
            'install.py': ROOT / 'scripts/linux/install.py',
            'launch-eu4.sh': ROOT / 'scripts/linux/launch-eu4.sh',
        })
    else:
        files.update({
            'libeu4dll_mac.dylib': build / 'libeu4dll_mac.dylib',
            'insert_dylib': build / 'tool/insert_dylib',
            'eu4dll_manifest_tool': build / 'tool/eu4dll_manifest_tool',
        })
    for name, directory in [('chinese_dict', build / 'chinese_dict'),
                            ('docs', ROOT / 'docs'), ('img', ROOT / 'img')]:
        for path in directory.rglob('*'):
            if path.is_file():
                files[f'{name}/{path.relative_to(directory).as_posix()}'] = path
    required_files = ['docs/LINUX_ADAPTATION.zh-CN.md']
    required_files += [f'chinese_dict/{dialect}/{name}'
                       for dialect in ('mandarin', 'cantonese')
                       for name in ('word.txt', 'user_dict.txt', 'trans_word.txt', 'phrases_map.txt', 'phrases_dict.txt', 'License.txt')]
    for required in required_files:
        if required not in files:
            raise FileNotFoundError(f'missing package resource: {required}')
    for path in files.values():
        if not path.is_file():
            raise FileNotFoundError(f'missing package input: {path}')
    commit = subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=ROOT, text=True).strip()
    dirty = bool(subprocess.check_output(['git', 'status', '--porcelain'], cwd=ROOT, text=True).strip())
    metadata = f'{commit}\nSource tree: {"modified" if dirty else "clean"}\nPlatform: {target} x86_64\nBuilder: {platform.platform()}\n'
    if target == 'linux':
        metadata += subprocess.check_output(['readelf', '--version-info', str(build / 'libeu4dll_linux.so')], text=True)
    output.parent.mkdir(parents=True, exist_ok=True)
    with zipfile.ZipFile(output, 'w', zipfile.ZIP_DEFLATED) as archive:
        for name, path in sorted(files.items()):
            archive.write(path, name)
        archive.writestr('BUILD_INFO.txt', metadata)
    with zipfile.ZipFile(output) as archive:
        if archive.testzip() is not None:
            raise RuntimeError('package integrity check failed')
    print(f'Packaged {target}: {output} ({len(files) + 1} files, commit {commit})')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--platform', choices=('linux', 'macos'), required=True)
    parser.add_argument('--build-dir', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    package(args.build_dir.resolve(), args.output.resolve(), args.platform)
