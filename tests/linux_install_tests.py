"""Installer boundaries and rollback; uses only disposable fake game files."""
import contextlib
import importlib.util
import io
import json
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest
from unittest import mock

ROOT = Path(__file__).resolve().parents[1]


class InstallTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix='eu4 installer ')
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.game = self.root / 'Game 中文'
        self.bundle = self.root / 'Package 中文'
        self.game.mkdir()
        self.bundle.mkdir()
        (self.game / 'eu4').write_bytes(b'original fixture')
        (self.bundle / 'libeu4dll_linux.so').write_bytes(b'library v1')
        (self.bundle / 'launch-eu4.sh').write_text('#!/bin/bash\nexit 0\n')
        for dialect in ('mandarin', 'cantonese'):
            (self.bundle / 'chinese_dict' / dialect).mkdir(parents=True)
            for name in ('word.txt', 'user_dict.txt', 'trans_word.txt', 'phrases_map.txt', 'phrases_dict.txt', 'License.txt'):
                (self.bundle / 'chinese_dict' / dialect / name).write_text('fixture dictionary')
        spec = importlib.util.spec_from_file_location('installer', ROOT / 'scripts/linux/install.py')
        self.installer = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(self.installer)
        self.installer.SCRIPT_DIR = self.bundle
        self.installer.EXPECTED = self.installer.digest(self.game / 'eu4')

    def install(self):
        args = ['install.py', str(self.game), '--build-dir', str(self.bundle)]
        with mock.patch.object(sys, 'argv', args), contextlib.redirect_stdout(io.StringIO()):
            self.installer.main()

    def test_missing_launcher_rejected_before_mutation(self):
        (self.bundle / 'launch-eu4.sh').unlink()
        with contextlib.redirect_stderr(io.StringIO()), self.assertRaises(SystemExit):
            self.install()
        self.assertEqual(list(self.game.iterdir()), [self.game / 'eu4'])
        self.assertEqual((self.game / 'eu4').read_bytes(), b'original fixture')

    def test_install_and_update_preserve_original_and_extra_dictionary(self):
        (self.game / 'chinese_dict').mkdir()
        (self.game / 'chinese_dict/user.txt').write_text('keep')
        self.install()
        self.assertEqual((self.game / 'eu4.orig').read_bytes(), b'original fixture')
        self.assertTrue((self.game / 'launch-eu4.sh').stat().st_mode & 0o111)
        (self.bundle / 'libeu4dll_linux.so').write_bytes(b'library v2')
        self.install()
        self.assertEqual((self.game / 'eu4.orig').read_bytes(), b'original fixture')
        self.assertEqual((self.game / 'libeu4dll_linux.so').read_bytes(), b'library v2')
        self.assertEqual((self.game / 'chinese_dict/user.txt').read_text(), 'keep')

    def test_publish_failure_restores_first_install_and_update(self):
        for update in (False, True):
            with self.subTest(update=update):
                if update:
                    self.install()
                before = {str(p.relative_to(self.game)): p.read_bytes()
                          for p in self.game.rglob('*') if p.is_file()
                          and 'eu4dll-backups' not in p.parts}
                real_replace = self.installer.os.replace

                def fail_dictionary(source, target):
                    if Path(source).name == 'chinese_dict':
                        raise OSError('simulated dictionary publication failure')
                    return real_replace(source, target)

                with mock.patch.object(self.installer.os, 'replace', side_effect=fail_dictionary):
                    with self.assertRaises(OSError):
                        self.install()
                after = {str(p.relative_to(self.game)): p.read_bytes()
                         for p in self.game.rglob('*') if p.is_file()
                         and 'eu4dll-backups' not in p.parts}
                self.assertEqual(before, after)
                self.assertFalse(list(self.game.glob('.eu4dll-stage-*')))

    def test_unified_entry_preserves_explicit_and_interactive_paths(self):
        shutil.copy2(ROOT / 'install.sh', self.bundle / 'install.sh')
        (self.bundle / 'install.py').write_text('import json, sys; print(json.dumps(sys.argv[1:]))\n')
        entry = ['bash', str(self.bundle / 'install.sh')]
        result = subprocess.run(entry + [str(self.game), '--build-dir', str(self.bundle)],
                                text=True, capture_output=True, check=True)
        self.assertEqual(json.loads(result.stdout), [str(self.game), '--build-dir', str(self.bundle)])
        result = subprocess.run(entry, input=f'"{self.game}"\n',
                                text=True, capture_output=True, check=True)
        self.assertEqual(json.loads(result.stdout[result.stdout.index('['):]), [str(self.game)])


if __name__ == '__main__':
    unittest.main()
