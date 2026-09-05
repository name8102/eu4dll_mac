#!/usr/bin/env bash
set -euo pipefail
script_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
root="$(cd -- "${script_dir}/../.." && pwd)"
binary="$(realpath -- "${1:?Usage: probe.sh /path/to/eu4.orig [build-dir]}")"
build="$(realpath -- "${2:-${root}/build-linux}")"
# The dynamic loader splits LD_PRELOAD on spaces, even inside a pathname.
links="$(mktemp -d /tmp/eu4dll-probe.XXXXXX)"
trap 'rm -rf -- "$links"' EXIT
ln -s -- "${build}/libeu4dll_linux.so" "${links}/patch.so"
ln -s -- "${build}/tests/targets/libeu4dll_linux_live_probe.so" "${links}/probe.so"
ln -s -- "${build}/chinese_dict" "${links}/chinese_dict"
for feature in BASE TEXT_LAYOUT MAIN_TEXT TOOLTIP_TEXT LOCALIZATION_UTF8 MAP_TEXT TEXT3D INPUT_IME CLIPBOARD_PASTE PINYIN_SEARCH SAVE_FILENAME DISPLAY_FORMATTING; do
    export "EU4DLL_ENABLE_${feature}=1"
done
cd -- "$(dirname -- "$binary")"
LD_PRELOAD="${links}/patch.so:${links}/probe.so" "$binary"
