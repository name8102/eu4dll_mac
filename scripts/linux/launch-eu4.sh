#!/usr/bin/env bash
set -euo pipefail

usage() {
    cat <<'EOF'
Usage: launch-eu4.sh [--dry-run] [--no-enable-chinese-input] [--library PATH] [--log PATH] [EU4_PATH] [ARG...]

Preload libeu4dll_linux.so and launch the native Linux EU4 executable.

Options:
  --no-enable-chinese-input
                          Only install the always-on base patches.
                          By default the calibrated Chinese rendering modules
                          input, clipboard, search, save filenames, date and name formatting are enabled.
EOF
}

enable_chinese_input() {
    export EU4DLL_ENABLE_BASE=1
    export EU4DLL_ENABLE_TEXT_LAYOUT=1
    export EU4DLL_ENABLE_MAIN_TEXT=1
    export EU4DLL_ENABLE_TOOLTIP_TEXT=1
    export EU4DLL_ENABLE_LOCALIZATION_UTF8=1
    export EU4DLL_ENABLE_MAP_TEXT=1
    export EU4DLL_ENABLE_TEXT3D=1
    export EU4DLL_ENABLE_INPUT_IME=1
    export EU4DLL_ENABLE_CLIPBOARD_PASTE=1
    export EU4DLL_ENABLE_PINYIN_SEARCH=1
    export EU4DLL_ENABLE_SAVE_FILENAME=1
    export EU4DLL_ENABLE_DISPLAY_FORMATTING=1
    if [[ -z "${SDL_IM_MODULE:-}" ]] && pgrep -x fcitx5 >/dev/null; then
        export SDL_IM_MODULE=fcitx
        export XMODIFIERS="${XMODIFIERS:-@im=fcitx}"
    fi
}

script_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
project_dir="$(cd -- "${script_dir}/../.." && pwd)"
library="${script_dir}/libeu4dll_linux.so"
if [[ ! -f "${library}" ]]; then
    library="${project_dir}/build-linux/libeu4dll_linux.so"
fi
log_dir="${XDG_STATE_HOME:-${HOME}/.local/state}/eu4dll_linux"
log_path="${log_dir}/eu4dll.log"
dry_run=0
enable_chinese_input

while [[ $# -gt 0 ]]; do
    case "$1" in
        --dry-run)
            dry_run=1
            shift
            ;;
        --enable-chinese-input)
            enable_chinese_input
            shift
            ;;
        --no-enable-chinese-input)
            unset EU4DLL_ENABLE_BASE
            unset EU4DLL_ENABLE_TEXT_LAYOUT
            unset EU4DLL_ENABLE_MAIN_TEXT
            unset EU4DLL_ENABLE_TOOLTIP_TEXT
            unset EU4DLL_ENABLE_LOCALIZATION_UTF8
            unset EU4DLL_ENABLE_MAP_TEXT
            unset EU4DLL_ENABLE_TEXT3D
            unset EU4DLL_ENABLE_INPUT_IME
            unset EU4DLL_ENABLE_CLIPBOARD_PASTE
            unset EU4DLL_ENABLE_PINYIN_SEARCH
            unset EU4DLL_ENABLE_SAVE_FILENAME
            unset EU4DLL_ENABLE_DISPLAY_FORMATTING
            shift
            ;;
        --library)
            library="${2:?--library requires a path}"
            shift 2
            ;;
        --log)
            log_path="${2:?--log requires a path}"
            shift 2
            ;;
        -h|--help)
            usage
            exit 0
            ;;
        --)
            shift
            break
            ;;
        -*)
            printf 'Unknown option: %s\n' "$1" >&2
            usage >&2
            exit 2
            ;;
        *)
            break
            ;;
    esac
done

eu4_path="${1:-}"
if [[ -x "${script_dir}/eu4.orig" ]]; then
    eu4_path="${script_dir}/eu4.orig"
elif [[ -x "${script_dir}/eu4" ]]; then
    eu4_path="${script_dir}/eu4"
elif [[ -n "${eu4_path}" ]]; then
    shift
elif [[ "${dry_run}" -eq 1 ]]; then
    eu4_path="/path/to/eu4"
else
    printf 'EU4_PATH is required.\n' >&2
    usage >&2
    exit 2
fi

# Installed wrappers pass eu4.orig explicitly. Consume that path instead of
# forwarding it to the game as an extra argument.
if [[ $# -gt 0 && "$1" == "${eu4_path}" ]]; then
    shift
fi

if [[ "${dry_run}" -ne 1 ]]; then
    [[ -f "${library}" ]] || { printf 'Library not found: %s\n' "${library}" >&2; exit 1; }
    [[ -x "${eu4_path}" ]] || { printf 'EU4 executable not found: %s\n' "${eu4_path}" >&2; exit 1; }
fi

if [[ "${EU4DLL_ENABLE_INPUT_IME:-}" == "1" && "${EU4DLL_USE_SYSTEM_SDL:-1}" == "1" ]]; then
    # EU4's built-in X11 text-input backend has empty Start/Stop functions.
    # Use SDL's supported dynamic API override; callers may select a full path.
    export SDL_DYNAMIC_API="${SDL_DYNAMIC_API:-libSDL2-2.0.so.0}"
    export SDL_VIDEODRIVER="${SDL_VIDEODRIVER:-x11}"
fi

runtime_dir="${XDG_RUNTIME_DIR:-/tmp}"
if [[ "${runtime_dir}" =~ [[:space:]] ]]; then
    runtime_dir="/tmp"
fi
preload_dir="${runtime_dir}/eu4dll_linux-${UID}"
preload_link="${preload_dir}/libeu4dll_linux.so"
dictionary_link="${preload_dir}/chinese_dict"

prepare_preload_dir() {
    mkdir -p -- "${preload_dir}" 2>/dev/null &&
        ln -sfn -- "${library}" "${preload_link}" 2>/dev/null &&
        ln -sfn -- "$(dirname -- "${library}")/chinese_dict" "${dictionary_link}" 2>/dev/null
}

if ! prepare_preload_dir; then
    preload_dir="/tmp/eu4dll_linux-${UID}"
    preload_link="${preload_dir}/libeu4dll_linux.so"
    dictionary_link="${preload_dir}/chinese_dict"
    prepare_preload_dir
fi

# ld.so treats spaces in LD_PRELOAD as separators. EU4's default Steam path
# contains spaces, so preload through a whitespace-free runtime symlink.
preload="${preload_link}"
if [[ -n "${EU4DLL_EXTRA_PRELOAD:-}" ]]; then
    preload="${preload}:${EU4DLL_EXTRA_PRELOAD}"
fi
if [[ -n "${LD_PRELOAD:-}" ]]; then
    preload="${preload}:${LD_PRELOAD}"
fi

if [[ "${dry_run}" -eq 1 ]]; then
    printf 'SteamAppId=%q SteamGameId=%q EU4DLL_LOG_PATH=%q SDL_IM_MODULE=%q XMODIFIERS=%q SDL_DYNAMIC_API=%q SDL_VIDEODRIVER=%q LD_PRELOAD=%q %q' \
        "${SteamAppId:-236850}" "${SteamGameId:-236850}" "${log_path}" \
        "${SDL_IM_MODULE:-}" "${XMODIFIERS:-}" "${SDL_DYNAMIC_API:-}" "${SDL_VIDEODRIVER:-}" "${preload}" "${eu4_path}"
    if [[ $# -gt 0 ]]; then
        printf ' %q' "$@"
    fi
    printf '\n'
    exit 0
fi

mkdir -p -- "$(dirname -- "${log_path}")"

export EU4DLL_LOG_PATH="${log_path}"
export LD_PRELOAD="${preload}"
export SteamAppId="${SteamAppId:-236850}"
export SteamGameId="${SteamGameId:-236850}"
exec "${eu4_path}" "$@" >>"${log_path}" 2>&1
