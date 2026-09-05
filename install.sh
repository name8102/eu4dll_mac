#!/bin/bash

set -u

SCRIPT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)

# One public entry point for source checkouts and both release packages.
# Source-only mode preserves the existing macOS installer test helpers.
if [ "${EU4DLL_SOURCE_ONLY:-0}" != 1 ]; then
    case "${1:-}" in
        -h|--help)
            cat <<'EOF'
Usage: bash install.sh [GAME_PATH]

macOS: GAME_PATH is eu4.app.
Linux: GAME_PATH is the Europa Universalis IV game directory.
Omit GAME_PATH to enter it interactively. Paths containing spaces must be quoted.

Linux source builds may add: --build-dir /path/to/build-linux
macOS automation retains EU4DLL_APP_PATH and EU4DLL_ACTION=install|uninstall.
EOF
            exit 0
            ;;
    esac
    case "$(uname -s)" in
        Linux)
            if [ -f "$SCRIPT_DIR/install.py" ]; then
                LINUX_INSTALLER="$SCRIPT_DIR/install.py"
            else
                LINUX_INSTALLER="$SCRIPT_DIR/scripts/linux/install.py"
            fi
            [ -f "$LINUX_INSTALLER" ] || { printf '%s\n' 'Missing Linux installer. Download the Linux package.' >&2; exit 1; }
            command -v python3 >/dev/null 2>&1 || { printf '%s\n' 'Python 3.11 or later is required for Linux installation.' >&2; exit 1; }
            python3 -c 'import sys; sys.exit(sys.version_info < (3, 11))' || { printf '%s\n' 'Python 3.11 or later is required for Linux installation.' >&2; exit 1; }
            if [ "$#" -eq 0 ]; then
                printf '%s' '请输入游戏目录 / Enter the Europa Universalis IV game directory: '
                IFS= read -r GAME_PATH || exit 1
                GAME_PATH=${GAME_PATH#\'}; GAME_PATH=${GAME_PATH%\'}
                GAME_PATH=${GAME_PATH#\"}; GAME_PATH=${GAME_PATH%\"}
                GAME_PATH=${GAME_PATH//\\ / }
                [ -n "$GAME_PATH" ] || { printf '%s\n' 'Game path is required.' >&2; exit 1; }
                set -- "$GAME_PATH"
            fi
            exec python3 "$LINUX_INSTALLER" "$@"
            ;;
        Darwin)
            [ "$#" -le 1 ] || { printf '%s\n' 'Usage: bash install.sh [eu4.app]' >&2; exit 2; }
            if [ "$#" -eq 1 ]; then export EU4DLL_APP_PATH="$1"; fi
            ;;
        *) printf '%s\n' 'Supported installation platforms: macOS and Linux.' >&2; exit 2 ;;
    esac
fi

DYLIB_NAME="libeu4dll_mac.dylib"
MANIFEST_NAME="eu4dll-patch-manifest.bin"
STATE_NAME="eu4dll-install-state"
DICT_NAME="chinese_dict"
ARTIFACT_DIR=$SCRIPT_DIR
TOOL_DIR=$SCRIPT_DIR
if [ ! -f "$SCRIPT_DIR/$DYLIB_NAME" ] && [ -f "$SCRIPT_DIR/build/$DYLIB_NAME" ]; then
    ARTIFACT_DIR="$SCRIPT_DIR/build"
    TOOL_DIR="$ARTIFACT_DIR/tool"
fi
DYLIB_SOURCE=${EU4DLL_DYLIB_SOURCE:-"$ARTIFACT_DIR/$DYLIB_NAME"}
INSERT_TOOL=${EU4DLL_INSERT_TOOL:-"$TOOL_DIR/insert_dylib"}
MANIFEST_TOOL=${EU4DLL_MANIFEST_TOOL:-"$TOOL_DIR/eu4dll_manifest_tool"}
DICT_SOURCE=${EU4DLL_DICT_SOURCE:-"$ARTIFACT_DIR/$DICT_NAME"}
PLISTBUDDY=${EU4DLL_PLISTBUDDY:-/usr/libexec/PlistBuddy}
CODESIGN=${EU4DLL_CODESIGN:-codesign}
XATTR=${EU4DLL_XATTR:-xattr}
LSREGISTER=${EU4DLL_LSREGISTER:-/System/Library/Frameworks/CoreServices.framework/Frameworks/LaunchServices.framework/Support/lsregister}
SUDO=${EU4DLL_SUDO:-sudo}
CURRENT_PHASE="startup"
TX_DIR=""
MUTATION_STARTED=0
ROLLBACK_RUNNING=0

msg() {
    key=$1
    case "${EU4DLL_LANG:-${LANG:-en}}" in
        zh*) case "$key" in
            prompt) printf '%s' "请将 eu4.app 拖入此终端窗口，然后按回车键确认: ";;
            bad_app) printf '%s\n' "错误：路径无效或不是可用的 eu4.app。";;
            preflight) printf '%s\n' "正在一次性检查游戏兼容性并生成补丁清单（启动时不会重复扫描）...";;
            preflight_failed) printf '%s\n' "错误：兼容性预检失败，游戏未被修改。";;
            installed) printf '%s\n' "EU4 MAC 双字节补丁安装完成。";;
            uninstalled) printf '%s\n' "补丁已卸载，游戏可执行文件已恢复。";;
            choose) printf '%s' "检测到已安装补丁。[1] 更新/修复 [2] 卸载: ";;
            sudo) printf '%s\n' "修改此 App 需要管理员权限。";;
            *) printf '%s\n' "$key";;
        esac ;;
        ja*) case "$key" in
            prompt) printf '%s' "eu4.app をこのターミナルにドラッグし、Enter を押してください: ";;
            bad_app) printf '%s\n' "エラー: 有効な eu4.app ではありません。";;
            preflight) printf '%s\n' "互換性を一度確認し、パッチマニフェストを生成しています...";;
            preflight_failed) printf '%s\n' "エラー: 互換性チェックに失敗しました。ゲームは変更されていません。";;
            installed) printf '%s\n' "EU4 MAC 2バイトパッチのインストールが完了しました。";;
            uninstalled) printf '%s\n' "パッチを削除し、実行ファイルを復元しました。";;
            choose) printf '%s' "パッチを検出しました。[1] 更新/修復 [2] 削除: ";;
            sudo) printf '%s\n' "この App の変更には管理者権限が必要です。";;
            *) printf '%s\n' "$key";;
        esac ;;
        ko*) case "$key" in
            prompt) printf '%s' "eu4.app를 이 터미널로 끌어온 뒤 Enter를 누르십시오: ";;
            bad_app) printf '%s\n' "오류: 유효한 eu4.app가 아닙니다.";;
            preflight) printf '%s\n' "호환성을 한 번 검사하고 패치 매니페스트를 생성하는 중...";;
            preflight_failed) printf '%s\n' "오류: 호환성 검사 실패. 게임은 변경되지 않았습니다.";;
            installed) printf '%s\n' "EU4 MAC 더블바이트 패치 설치 완료.";;
            uninstalled) printf '%s\n' "패치를 제거하고 실행 파일을 복원했습니다.";;
            choose) printf '%s' "설치된 패치 감지. [1] 업데이트/복구 [2] 제거: ";;
            sudo) printf '%s\n' "이 App을 변경하려면 관리자 권한이 필요합니다.";;
            *) printf '%s\n' "$key";;
        esac ;;
        *) case "$key" in
            prompt) printf '%s' "Drag eu4.app into this terminal and press Enter: ";;
            bad_app) printf '%s\n' "Error: the path is not a usable eu4.app.";;
            preflight) printf '%s\n' "Checking compatibility once and generating the patch manifest...";;
            preflight_failed) printf '%s\n' "Error: compatibility preflight failed; the game was not modified.";;
            installed) printf '%s\n' "EU4 MAC Double-Byte Patch installation complete.";;
            uninstalled) printf '%s\n' "Patch uninstalled and original executable restored.";;
            choose) printf '%s' "Patch detected. [1] Update/repair [2] Uninstall: ";;
            sudo) printf '%s\n' "Administrator privileges are required to modify this App.";;
            *) printf '%s\n' "$key";;
        esac ;;
    esac
}

fail() { printf 'install.sh: phase=%s: %s\n' "$CURRENT_PHASE" "$*" >&2; return 1; }

discover_app() {
    if [ -n "${EU4DLL_APP_PATH:-}" ]; then APP_PATH=$EU4DLL_APP_PATH
    else msg prompt; IFS= read -r APP_PATH || return 1; fi
    APP_PATH=${APP_PATH#\'}; APP_PATH=${APP_PATH%\'}
    APP_PATH=${APP_PATH#\"}; APP_PATH=${APP_PATH%\"}
    APP_PATH=${APP_PATH//\\ / }
    CONTENTS_DIR="$APP_PATH/Contents"
    EXEC_PATH="$CONTENTS_DIR/MacOS/eu4"
    FRAMEWORKS_DIR="$CONTENTS_DIR/Frameworks"
    RESOURCES_DIR="$CONTENTS_DIR/Resources"
    PLIST_PATH="$CONTENTS_DIR/Info.plist"
    BACKUP_PATH="${EXEC_PATH}_bak"
    DYLIB_DEST="$FRAMEWORKS_DIR/$DYLIB_NAME"
    MANIFEST_DEST="$RESOURCES_DIR/$MANIFEST_NAME"
    STATE_DIR="$RESOURCES_DIR/$STATE_NAME"
}

validate_inputs() {
    [ -d "$APP_PATH" ] && [ -f "$EXEC_PATH" ] && [ -f "$PLIST_PATH" ] || { msg bad_app >&2; return 1; }
}

validate_install_artifacts() {
    [ -f "$DYLIB_SOURCE" ] || { fail "missing $DYLIB_SOURCE"; return 1; }
    [ -f "$INSERT_TOOL" ] || { fail "missing $INSERT_TOOL"; return 1; }
    [ -f "$MANIFEST_TOOL" ] || { fail "missing $MANIFEST_TOOL"; return 1; }
}

detect_state() {
    IS_INJECTED=0
    grep -qF "$DYLIB_NAME" "$EXEC_PATH" && IS_INJECTED=1
    ACTION=${EU4DLL_ACTION:-}
    if [ -z "$ACTION" ]; then
        if [ "$IS_INJECTED" -eq 1 ]; then msg choose; IFS= read -r choice; [ "$choice" = 2 ] && ACTION=uninstall || ACTION=install
        else ACTION=install; fi
    fi
    [ "$ACTION" = install ] || [ "$ACTION" = uninstall ] || fail "invalid action: $ACTION"
}

validate_ownership() {
    if [ -e "$STATE_DIR" ]; then
        if [ ! -d "$STATE_DIR" ]; then fail "ownership state is not a directory"; return 1; fi
        if [ ! -f "$STATE_DIR/ownership" ] || [ ! -f "$STATE_DIR/Info.plist.original" ]; then fail "ownership state is incomplete"; return 1; fi
        if [ "$(cat "$STATE_DIR/ownership")" != "eu4dll-installer-state-v1" ]; then fail "ownership state schema is invalid"; return 1; fi
        return 0
    fi
    if [ "$ACTION" = install ]; then
        [ ! -e "$DYLIB_DEST" ] || { fail "unowned dylib already exists"; return 1; }
        [ ! -e "$MANIFEST_DEST" ] || { fail "unowned manifest already exists"; return 1; }
        [ ! -e "$RESOURCES_DIR/$DICT_NAME" ] || { fail "unowned dictionary already exists"; return 1; }
    fi
}

configure_privilege() {
    USE_SUDO=0
    if [ ! -w "$CONTENTS_DIR" ]; then
        msg sudo
        "$SUDO" -v || return 1
        USE_SUDO=1
    fi
}

mutate() { if [ "$USE_SUDO" -eq 1 ]; then "$SUDO" "$@"; else "$@"; fi; }

run_preflight() {
    CURRENT_PHASE=preflight
    PREFLIGHT_EXEC=$EXEC_PATH
    if [ "$IS_INJECTED" -eq 1 ]; then
        [ -f "$BACKUP_PATH" ] || fail "injected executable has no verified original backup"
        if grep -qF "$DYLIB_NAME" "$BACKUP_PATH"; then fail "backup is injected"; return 1; fi
        PREFLIGHT_EXEC=$BACKUP_PATH
    fi
    PREFLIGHT_DIR=$(mktemp -d "${TMPDIR:-/tmp}/eu4dll-preflight.XXXXXX") || return 1
    PREFLIGHT_MANIFEST="$PREFLIGHT_DIR/$MANIFEST_NAME"
    msg preflight
    "$MANIFEST_TOOL" "$PREFLIGHT_EXEC" "$PREFLIGHT_MANIFEST" || { msg preflight_failed >&2; return 1; }
    [ -s "$PREFLIGHT_MANIFEST" ] || fail "manifest tool produced no manifest"
}

snapshot_path() {
    label=$1; path=$2
    if [ -e "$path" ]; then mkdir -p "$TX_DIR/snap"; cp -pR "$path" "$TX_DIR/snap/$label" || return 1; : > "$TX_DIR/snap/$label.exists"
    fi
}

prepare_transaction() {
    CURRENT_PHASE=transaction
    TX_DIR=$(mktemp -d "${TMPDIR:-/tmp}/eu4dll-install.XXXXXX") || return 1
    snapshot_path exec "$EXEC_PATH" || return 1
    snapshot_path backup "$BACKUP_PATH" || return 1
    snapshot_path dylib "$DYLIB_DEST" || return 1
    snapshot_path manifest "$MANIFEST_DEST" || return 1
    snapshot_path dictionary "$RESOURCES_DIR/$DICT_NAME" || return 1
snapshot_path plist "$PLIST_PATH" || return 1
    snapshot_path state "$STATE_DIR" || return 1
    snapshot_path signature "$CONTENTS_DIR/_CodeSignature" || return 1
}

restore_snapshot() {
    label=$1; path=$2
    mutate rm -rf "$path" || return 1
    if [ -f "$TX_DIR/snap/$label.exists" ]; then mutate cp -pR "$TX_DIR/snap/$label" "$path" || return 1; fi
}

rollback() {
    [ "$MUTATION_STARTED" -eq 1 ] || return 0
    ROLLBACK_RUNNING=1
    printf 'install.sh: rolling back failed phase=%s\n' "$CURRENT_PHASE" >&2
    restore_snapshot exec "$EXEC_PATH"
    restore_snapshot backup "$BACKUP_PATH"
    restore_snapshot dylib "$DYLIB_DEST"
    restore_snapshot manifest "$MANIFEST_DEST"
    restore_snapshot dictionary "$RESOURCES_DIR/$DICT_NAME"
    restore_snapshot plist "$PLIST_PATH"
    restore_snapshot state "$STATE_DIR"
    restore_snapshot signature "$CONTENTS_DIR/_CodeSignature"
    cleanup_app_temps
    ROLLBACK_RUNNING=0
}

ensure_original_backup() {
    CURRENT_PHASE=backup
    if [ -f "$BACKUP_PATH" ]; then
        if [ "$IS_INJECTED" -eq 0 ] && ! cmp -s "$EXEC_PATH" "$BACKUP_PATH"; then
            mutate cp -p "$EXEC_PATH" "$BACKUP_PATH.tmp.$$" || return 1
            mutate mv -f "$BACKUP_PATH.tmp.$$" "$BACKUP_PATH" || return 1
        fi
        return 0
    fi
    [ "$IS_INJECTED" -eq 0 ] || fail "refusing to back up an injected executable"
    mutate cp -p "$EXEC_PATH" "$BACKUP_PATH"
}

inject_executable() {
    CURRENT_PHASE=injection
    [ "$IS_INJECTED" -eq 1 ] && return 0
    mutate cp -p "$EXEC_PATH" "$EXEC_PATH.eu4dll-tmp" || return 1
    output=$(mutate "$INSERT_TOOL" --inplace --all-yes "@executable_path/../Frameworks/$DYLIB_NAME" "$EXEC_PATH.eu4dll-tmp" 2>&1) || { printf '%s\n' "$output" >&2; return 1; }
    grep -qF "$DYLIB_NAME" "$EXEC_PATH.eu4dll-tmp" || fail "injector did not produce the load command"
    mutate mv -f "$EXEC_PATH.eu4dll-tmp" "$EXEC_PATH" || return 1
}

cleanup_app_temps() {
    mutate rm -rf "$EXEC_PATH.eu4dll-tmp" "$BACKUP_PATH.tmp.$$" \
        "$DYLIB_DEST.tmp.$$" "$MANIFEST_DEST.tmp.$$" \
        "$RESOURCES_DIR/$DICT_NAME.tmp.$$" "$STATE_DIR.tmp.$$"
}

publish_state() {
    CURRENT_PHASE=state-publish
    if [ -d "$STATE_DIR" ]; then mutate cp -pR "$STATE_DIR" "$STATE_DIR.tmp.$$" || return 1
    else mutate mkdir -p "$STATE_DIR.tmp.$$" || return 1; mutate cp -p "$TX_DIR/snap/plist" "$STATE_DIR.tmp.$$/Info.plist.original" || return 1; fi
    printf 'eu4dll-installer-state-v1\n' > "$TX_DIR/state"
    mutate cp "$TX_DIR/state" "$STATE_DIR.tmp.$$/ownership" || return 1
    mutate rm -rf "$STATE_DIR" || return 1
    mutate mv "$STATE_DIR.tmp.$$" "$STATE_DIR"
}

publish_payload() {
    CURRENT_PHASE=mutation
    mutate mkdir -p "$FRAMEWORKS_DIR" "$RESOURCES_DIR" || return 1
    mutate cp "$DYLIB_SOURCE" "$DYLIB_DEST.tmp.$$" || return 1
    mutate mv -f "$DYLIB_DEST.tmp.$$" "$DYLIB_DEST" || return 1
    if [ "${EU4DLL_INSTALL_DICT:-auto}" != skip ] && [ -e "$DICT_SOURCE" ]; then
        mutate rm -rf "$RESOURCES_DIR/$DICT_NAME.tmp.$$" || return 1
        mutate cp -R "$DICT_SOURCE" "$RESOURCES_DIR/$DICT_NAME.tmp.$$" || return 1
        mutate rm -rf "$RESOURCES_DIR/$DICT_NAME" || return 1
        mutate mv "$RESOURCES_DIR/$DICT_NAME.tmp.$$" "$RESOURCES_DIR/$DICT_NAME" || return 1
    fi
}

publish_manifest() {
    CURRENT_PHASE=manifest-publish
    mutate cp "$PREFLIGHT_MANIFEST" "$MANIFEST_DEST.tmp.$$" || return 1
    mutate mv -f "$MANIFEST_DEST.tmp.$$" "$MANIFEST_DEST"
}

configure_plist() {
    CURRENT_PHASE=configuration
    [ "${EU4DLL_FULLSCREEN_FIX:-no}" = yes ] || return 0
    mutate "$PLISTBUDDY" -c "Delete :LSUIPresentationMode" "$PLIST_PATH" >/dev/null 2>&1 || true
    mutate "$PLISTBUDDY" -c "Add :LSUIPresentationMode integer 4" "$PLIST_PATH"
}

sign_app() {
    CURRENT_PHASE=sign
    return 0
}

sign_uninstalled_app() {
    CURRENT_PHASE=sign-uninstall
    return 0
}

refresh_registration() { CURRENT_PHASE=registration; "$LSREGISTER" -f "$APP_PATH" >/dev/null 2>&1 || true; }

install_patch_after_preflight() {
    prepare_transaction || return 1
    MUTATION_STARTED=1
    ensure_original_backup || return 1
    inject_executable || return 1
    publish_payload || return 1
    publish_manifest || return 1
    configure_plist || return 1
    publish_state || return 1
    cleanup_app_temps || return 1
    sign_app || return 1
    refresh_registration
    MUTATION_STARTED=0
    msg installed
}

uninstall_patch() {
    CURRENT_PHASE=uninstall
    [ -d "$STATE_DIR" ] || { msg uninstalled; return 0; }
    prepare_transaction || return 1
    MUTATION_STARTED=1
    if [ -f "$BACKUP_PATH" ]; then
        mutate cp -p "$BACKUP_PATH" "$EXEC_PATH" || return 1
        mutate rm -f "$BACKUP_PATH" || return 1
    elif [ "$IS_INJECTED" -eq 1 ]; then fail "cannot restore injected executable without original backup"; fi
    mutate rm -f "$DYLIB_DEST" "$MANIFEST_DEST" "$CONTENTS_DIR/MacOS/$DYLIB_NAME" || return 1
    mutate rm -rf "$RESOURCES_DIR/$DICT_NAME" || return 1
    if [ -f "$STATE_DIR/Info.plist.original" ]; then mutate cp -p "$STATE_DIR/Info.plist.original" "$PLIST_PATH" || return 1; fi
    mutate rm -rf "$STATE_DIR" || return 1
    cleanup_app_temps || return 1
    sign_uninstalled_app || return 1
    refresh_registration
    MUTATION_STARTED=0
    msg uninstalled
}

cleanup() { [ -n "${PREFLIGHT_DIR:-}" ] && rm -rf "$PREFLIGHT_DIR"; [ -n "$TX_DIR" ] && rm -rf "$TX_DIR"; }
on_exit() { status=$?; if [ "$status" -ne 0 ] && [ "$ROLLBACK_RUNNING" -eq 0 ]; then rollback; fi; cleanup; exit "$status"; }
trap on_exit EXIT HUP INT TERM

main() {
    CURRENT_PHASE=discovery; discover_app || return 1
    CURRENT_PHASE=validation; validate_inputs || return 1
    CURRENT_PHASE=state; detect_state || return 1
    if [ "$ACTION" = install ]; then CURRENT_PHASE=validation; validate_install_artifacts || return 1; fi
    CURRENT_PHASE=ownership; validate_ownership || return 1
    if [ "$ACTION" = install ]; then run_preflight || return 1; fi
    CURRENT_PHASE=privilege; configure_privilege || return 1
    if [ "$ACTION" = uninstall ]; then uninstall_patch; else install_patch_after_preflight; fi
}

if [ "${EU4DLL_SOURCE_ONLY:-0}" != 1 ]; then main "$@"; fi
