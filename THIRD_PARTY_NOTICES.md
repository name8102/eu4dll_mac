# Third-party components

- **cpp-pinyin 1.0.2** — https://github.com/wolfgitpr/cpp-pinyin
  Statically linked into the platform library. Its Apache-2.0 license is copied
  unchanged from the pinned dependency into `licenses/cpp-pinyin-LICENSE` in
  release packages. Copyright notices are retained in that file.
- **Dictionary data** — copied from cpp-pinyin's `res/dict` without modification.
  Source attribution and terms remain inside `chinese_dict/mandarin/License.txt`
  and `chinese_dict/cantonese/License.txt`; do not remove those files.
- **insert_dylib** — https://github.com/tyilo/insert_dylib
  The macOS tool is built from the existing `tool/insert_dylib.c` adaptation,
  which identifies the upstream source. Refer to that upstream project for its
  source and notices; this document does not assign it a new license.

The project's own license is included as `LICENSE`. Game binaries, fonts and
localization mods are not part of the release archives.
