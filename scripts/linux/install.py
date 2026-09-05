#!/usr/bin/env python3
"""Install the canonical Linux build with dated rollback copies."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import tempfile
from datetime import datetime

EXPECTED = 'af115d3b0e54a05eca0198ed569db90ca225728afda03b5ac4ded251520a7ce3'
SCRIPT_DIR = Path(__file__).resolve().parent
BUNDLED = (SCRIPT_DIR / 'libeu4dll_linux.so').is_file()
ROOT = SCRIPT_DIR if BUNDLED else SCRIPT_DIR.parents[1]
PAYLOAD = ('libeu4dll_linux.so', 'launch-eu4.sh', 'chinese_dict', 'eu4')

def copy_path(source, destination):
    if source.is_symlink():
        destination.symlink_to(os.readlink(source))
    elif source.is_dir():
        shutil.copytree(source, destination, symlinks=True)
    else:
        shutil.copy2(source, destination)

def remove_path(path):
    if path.is_symlink() or path.is_file():
        path.unlink()
    elif path.is_dir():
        shutil.rmtree(path)

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
    if not game.is_dir():
        parser.error('game directory does not exist')
    if game == build:
        parser.error('extract/build the installer outside the game directory')
    binary = game / ('eu4.orig' if (game / 'eu4.orig').exists() else 'eu4')
    if not binary.is_file():
        parser.error('game directory must contain eu4 or eu4.orig')
    if digest(binary) != EXPECTED:
        parser.error('unsupported EU4 ELF; expected native Linux 1.37.5')
    library = build / 'libeu4dll_linux.so'
    dictionary = build / 'chinese_dict'
    launcher = SCRIPT_DIR / 'launch-eu4.sh'
    dictionary_files = ('word.txt', 'user_dict.txt', 'trans_word.txt', 'phrases_map.txt', 'phrases_dict.txt', 'License.txt')
    complete_dictionary = all((dictionary / dialect / name).is_file()
                              for dialect in ('mandarin', 'cantonese') for name in dictionary_files)
    if not library.is_file() or not complete_dictionary:
        parser.error('missing library or dictionary files; extract the complete package or build it first')
    if not launcher.is_file():
        parser.error('missing launch-eu4.sh; extract the complete installation package')
    for name in PAYLOAD:
        path = game / name
        if name != 'chinese_dict' and path.is_dir():
            parser.error(f'expected a file, found directory: {path}')
    # Stage the complete payload before touching the original executable.
    with tempfile.TemporaryDirectory(prefix='.eu4dll-stage-', dir=game) as staging:
        stage = Path(staging)
        shutil.copy2(library, stage / library.name)
        shutil.copy2(launcher, stage / launcher.name)
        (stage / launcher.name).chmod(0o755)
        if (game / 'chinese_dict').is_dir():
            shutil.copytree(game / 'chinese_dict', stage / 'chinese_dict')
        shutil.copytree(dictionary, stage / 'chinese_dict', dirs_exist_ok=True)
        wrapper = stage / 'eu4'
        wrapper.write_text('''#!/usr/bin/env bash
set -euo pipefail
script_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
exec "${script_dir}/launch-eu4.sh" "${script_dir}/eu4.orig" "$@"
''')
        wrapper.chmod(0o755)
        backup = game / 'eu4dll-backups' / datetime.now().strftime('%Y%m%d-%H%M%S-%f')
        backup.mkdir(parents=True)
        first_install = binary.name == 'eu4'
        for name in PAYLOAD:
            path = game / name
            if name == 'eu4' and first_install:
                continue  # The original is preserved by renaming to eu4.orig.
            if path.exists() or path.is_symlink():
                copy_path(path, backup / name)
        applied = []
        original_moved = False
        try:
            if first_install:
                binary.rename(game / 'eu4.orig')
                original_moved = True
            for name in PAYLOAD:
                applied.append(name)
                if name == 'chinese_dict':
                    remove_path(game / name)
                os.replace(stage / name, game / name)
            manifest = {'game_elf_sha256': EXPECTED, 'library_sha256': digest(game / library.name),
                        'source_build': str(build), 'backup': str(backup)}
            (backup / 'installation.json').write_text(json.dumps(manifest, indent=2) + '\n')
        except (OSError, KeyboardInterrupt):
            try:
                for name in reversed(applied):
                    if name == 'eu4' and first_install:
                        continue
                    remove_path(game / name)
                    saved = backup / name
                    if saved.exists() or saved.is_symlink():
                        copy_path(saved, game / name)
                if original_moved:
                    os.replace(game / 'eu4.orig', game / 'eu4')
            except OSError as restore_error:
                raise RuntimeError(f'rollback incomplete; restore from {backup}: {restore_error}') from restore_error
            raise
    print(json.dumps(manifest, indent=2))

if __name__ == '__main__':
    main()
