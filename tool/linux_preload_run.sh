#!/bin/bash
# Repeatable manual validation for the Linux base vertical slice.
# Usage:
#   EU4DLL_GAME=/path/to/eu4 ./tool/linux_preload_run.sh [extra-args...]
#   EU4DLL_ALLOW_UNSUPPORTED_ELF=1 EU4DLL_GAME=/path/to/eu4 \
#       ./tool/linux_preload_run.sh   # development override only
set -u

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT="$(cd "${HERE}/.." && pwd)"
BUILD_DIR="${BUILD_DIR:-${ROOT}/build-linux}"

LIB="${BUILD_DIR}/libeu4dll_linux.so"
GAME="${EU4DLL_GAME:-}"

if [[ ! -f "${LIB}" ]]; then
    echo "missing ${LIB}; configure and build first:" >&2
    echo "  cmake -S '${ROOT}' -B '${BUILD_DIR}' -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo" >&2
    echo "  cmake --build '${BUILD_DIR}'" >&2
    exit 2
fi
if [[ -z "${GAME}" ]]; then
    echo "set EU4DLL_GAME=/path/to/eu4 (native Linux x86-64 EU4 1.37.5) to run." >&2
    exit 2
fi
if [[ "${EU4DLL_ALLOW_UNSUPPORTED_ELF:-}" == "1" ]]; then
    echo "WARNING: EU4DLL_ALLOW_UNSUPPORTED_ELF=1 is a development override, not support." >&2
fi

echo "lib=${LIB}"
echo "game=${GAME}"
LD_PRELOAD="${LIB}" exec "${GAME}" "$@"
