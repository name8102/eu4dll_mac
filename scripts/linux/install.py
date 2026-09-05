#!/usr/bin/env python3
"""Install the canonical Linux build with dated rollback copies."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
from datetime import datetime

EXPECTED = 'af115d3b0e54a05eca0198ed569db90ca225728afda03b5ac4ded251520a7ce3'
SCRIPT_DIR = Path(__file__).resolve().parent
BUNDLED = (SCRIPT_DIR / 'libeu4dll_linux.so').is_file()
ROOT = SCRIPT_DIR if BUNDLED else SCRIPT_DIR.parents[1]

def digest(path):
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('game_dir', type=Path)
    parser.add_argument('--build-dir', type=Path, default=ROOT if BUNDLED else ROOT / 'build-linux')
    args = parser.parse_args()
    game = args.game_dir.resolve()
    build = args.build_dir.resolve()
    binary = game / ('eu4.orig' if (game / 'eu4.orig').exists() else 'eu4')
    if digest(binary) != EXPECTED:
        parser.error('unsupported EU4 ELF; expected native Linux 1.37.5')
    library = build / 'libeu4dll_linux.so'
    dictionary = build / 'chinese_dict'
    if not library.is_file() or not (dictionary / 'mandarin/word.txt').is_file():
        parser.error('build libeu4dll_linux.so and its chinese_dict first')
    backup = game / 'eu4dll-backups' / datetime.now().strftime('%Y%m%d-%H%M%S-%f')
    backup.mkdir(parents=True)
    for name in ('eu4', 'launch-eu4.sh', 'libeu4dll_linux.so'):
        path = game / name
        if path.is_file() and path != binary:
            shutil.copy2(path, backup / name)
    if (game / 'chinese_dict').exists():
        shutil.copytree(game / 'chinese_dict', backup / 'chinese_dict')
    if binary.name == 'eu4':
        # Preserve the original game executable exactly, including its mode.
        binary.rename(game / 'eu4.orig')
    def install_file(source, name):
        temporary = game / (name + '.eu4dll-new')
        shutil.copy2(source, temporary)
        os.replace(temporary, game / name)
    install_file(library, 'libeu4dll_linux.so')
    install_file(SCRIPT_DIR / 'launch-eu4.sh', 'launch-eu4.sh')
    # Dictionary data does not change game files; preserve a copy for rollback.
    destination = game / 'chinese_dict'
    if destination.is_symlink():
        destination.unlink()
    shutil.copytree(dictionary, destination, dirs_exist_ok=True)
    wrapper = game / 'eu4.eu4dll-new'
    wrapper.write_text('''#!/usr/bin/env bash
set -euo pipefail
script_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
exec "${script_dir}/launch-eu4.sh" "${script_dir}/eu4.orig" "$@"
''')
    wrapper.chmod(0o755)
    os.replace(wrapper, game / 'eu4')
    manifest = {'game_elf_sha256': EXPECTED, 'library_sha256': digest(game / 'libeu4dll_linux.so'),
                'source_build': str(build), 'backup': str(backup)}
    (backup / 'installation.json').write_text(json.dumps(manifest, indent=2) + '\n')
    print(json.dumps(manifest, indent=2))

if __name__ == '__main__':
    main()
