#!/bin/bash

set -u

ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
TMP_ROOT=$(mktemp -d "${TMPDIR:-/tmp}/eu4dll-installer-tests.XXXXXX") || exit 1
trap 'rm -rf "$TMP_ROOT"' EXIT
PASS=0

fail() { printf 'FAIL: %s\n' "$*" >&2; exit 1; }
assert_file() { [ -f "$1" ] || fail "missing file: $1"; }
assert_absent() { [ ! -e "$1" ] || fail "unexpected path: $1"; }
assert_same() { cmp -s "$1" "$2" || fail "files differ: $1 $2"; }
assert_no_temps() { ! find "$1" -name '*.tmp.*' -o -name '*.eu4dll-tmp' | grep -q . || fail "temporary paths remain in $1"; }

make_tools() {
    tools=$1
    mkdir -p "$tools"
    cp "$ROOT/install.sh" "$tools/install.sh"
    printf 'dylib-v1\n' > "$tools/libeu4dll_mac.dylib"
    mkdir -p "$tools/chinese_dict"
    printf 'dictionary\n' > "$tools/chinese_dict/data"
    for name in xattr codesign lsregister sudo; do
        printf '#!/bin/bash\n[ "${EU4DLL_FAIL_COMMAND:-}" = "%s" ] && exit 71\nif [ "%s" = sudo ]; then [ "${1:-}" = -v ] && exit 0; shift; [ "$#" -eq 0 ] || "$@"; fi\nexit 0\n' "$name" "$name" > "$tools/$name"
        chmod +x "$tools/$name"
    done
    printf '#!/bin/bash\nset -u\n[ "${EU4DLL_FAIL_COMMAND:-}" = xattr ] && exit 71\ntarget=${@: -1}\n[ -e "$target" ] || exit 74\n[ -n "${EU4DLL_CALL_LOG:-}" ] && printf "xattr:%%s\\n" "$target" >> "$EU4DLL_CALL_LOG"\nexit 0\n' > "$tools/xattr"
    printf '#!/bin/bash\nset -u\n[ "${EU4DLL_FAIL_COMMAND:-}" = codesign ] && exit 71\ntarget=${@: -1}\n[ -e "$target" ] || exit 74\n[ -n "${EU4DLL_CALL_LOG:-}" ] && printf "codesign:%%s\\n" "$target" >> "$EU4DLL_CALL_LOG"\nexit 0\n' > "$tools/codesign"
    chmod +x "$tools/xattr" "$tools/codesign"
    printf '#!/bin/bash\nset -u\n[ "${EU4DLL_FAIL_COMMAND:-}" = manifest ] && exit 72\nprintf "manifest:%%s\\n" "$(shasum -a 256 "$1" | cut -d " " -f 1)" > "$2"\n' > "$tools/eu4dll_manifest_tool"
    printf '#!/bin/bash\nset -u\n[ "${EU4DLL_FAIL_COMMAND:-}" = inject ] && { printf partial >> "$4"; exit 73; }\nprintf "\\n%%s\\n" "$3" >> "$4"\n' > "$tools/insert_dylib"
    printf '#!/bin/bash\nset -u\nfile=${@: -1}\ncase "$2" in *Delete*) sed "/LSUIPresentationMode/d;/DYLD_INSERT_LIBRARIES/d" "$file" > "$file.tmp"; mv "$file.tmp" "$file";; *Add*) printf "LSUIPresentationMode=4\\n" >> "$file";; esac\n' > "$tools/PlistBuddy"
    chmod +x "$tools/eu4dll_manifest_tool" "$tools/insert_dylib" "$tools/PlistBuddy"
}

make_app() {
    app=$1
    mkdir -p "$app/Contents/MacOS" "$app/Contents/Frameworks" "$app/Contents/Resources"
    printf 'ORIGINAL-EU4-BINARY\n' > "$app/Contents/MacOS/eu4"
    printf 'CFBundleName=eu4\nUNRELATED=keep\nLSUIPresentationMode=7\n' > "$app/Contents/Info.plist"
    printf 'unrelated\n' > "$app/Contents/Resources/keep.txt"
}

tree_digest() {
    target=$1
    find "$target" -print0 | LC_ALL=C sort -z | while IFS= read -r -d '' file; do
        printf '%s %s ' "${file#"$target"/}" "$(stat -f '%HT:%Sp' "$file")"
        if [ -L "$file" ]; then readlink "$file"
        elif [ -f "$file" ]; then shasum -a 256 "$file" | cut -d ' ' -f 1
        else printf '\n'; fi
    done | shasum -a 256 | cut -d ' ' -f 1
}

run_installer() {
    tools=$1 app=$2 action=$3
    PATH="$tools:/usr/bin:/bin" \
    EU4DLL_APP_PATH="$app" EU4DLL_ACTION="$action" EU4DLL_LANG=en \
    EU4DLL_MANIFEST_TOOL="${EU4DLL_MANIFEST_TOOL_OVERRIDE:-$tools/eu4dll_manifest_tool}" EU4DLL_INSERT_TOOL="$tools/insert_dylib" \
    EU4DLL_DYLIB_SOURCE="$tools/libeu4dll_mac.dylib" EU4DLL_DICT_SOURCE="$tools/chinese_dict" \
    EU4DLL_PLISTBUDDY="$tools/PlistBuddy" EU4DLL_CODESIGN="$tools/codesign" \
    EU4DLL_XATTR="$tools/xattr" EU4DLL_LSREGISTER="$tools/lsregister" EU4DLL_SUDO="$tools/sudo" \
    EU4DLL_FULLSCREEN_FIX=no EU4DLL_INSTALL_DICT=auto "$tools/install.sh"
}

TOOLS="$TMP_ROOT/package tools"
APP="$TMP_ROOT/Game Library With Spaces/eu4.app"
make_tools "$TOOLS"
make_app "$APP"
cp "$APP/Contents/MacOS/eu4" "$TMP_ROOT/original"

run_installer "$TOOLS" "$APP" install || fail "first install"
assert_file "$APP/Contents/MacOS/eu4_bak"
assert_same "$TMP_ROOT/original" "$APP/Contents/MacOS/eu4_bak"
assert_file "$APP/Contents/Frameworks/libeu4dll_mac.dylib"
assert_file "$APP/Contents/Resources/eu4dll-patch-manifest.bin"
grep -qF libeu4dll_mac.dylib "$APP/Contents/MacOS/eu4" || fail "not injected"
assert_no_temps "$APP"

PATH="$TOOLS:/usr/bin:/bin" EU4DLL_APP_PATH="$APP" EU4DLL_ACTION=install EU4DLL_LANG=en \
EU4DLL_MANIFEST_TOOL="$TOOLS/eu4dll_manifest_tool" EU4DLL_INSERT_TOOL="$TOOLS/insert_dylib" \
EU4DLL_DYLIB_SOURCE="$TOOLS/libeu4dll_mac.dylib" EU4DLL_DICT_SOURCE="$TOOLS/chinese_dict" \
EU4DLL_PLISTBUDDY="$TOOLS/PlistBuddy" EU4DLL_CODESIGN="$TOOLS/codesign" EU4DLL_XATTR="$TOOLS/xattr" \
EU4DLL_LSREGISTER="$TOOLS/lsregister" EU4DLL_SUDO="$TOOLS/sudo" EU4DLL_FULLSCREEN_FIX=yes \
EU4DLL_INSTALL_DICT=auto "$TOOLS/install.sh" >/dev/null || fail "plist update"
grep -qF 'LSUIPresentationMode=4' "$APP/Contents/Info.plist" || fail "plist not changed to 4"

run_installer "$TOOLS" "$APP" install || fail "reinstall"
assert_same "$TMP_ROOT/original" "$APP/Contents/MacOS/eu4_bak"

printf 'dylib-v2\n' > "$TOOLS/libeu4dll_mac.dylib"
run_installer "$TOOLS" "$APP" install || fail "update"
assert_same "$TOOLS/libeu4dll_mac.dylib" "$APP/Contents/Frameworks/libeu4dll_mac.dylib"

rm "$APP/Contents/Frameworks/libeu4dll_mac.dylib"
run_installer "$TOOLS" "$APP" install || fail "repair"
assert_file "$APP/Contents/Frameworks/libeu4dll_mac.dylib"

PRE=$(tree_digest "$APP")
if EU4DLL_FAIL_COMMAND=manifest run_installer "$TOOLS" "$APP" install >/dev/null 2>&1; then fail "preflight failure accepted"; fi
POST=$(tree_digest "$APP")
[ "$PRE" = "$POST" ] || fail "preflight changed app bytes"

printf 'stale-manifest\n' > "$APP/Contents/Resources/eu4dll-patch-manifest.bin"
NO_SIGN_LOG="$TMP_ROOT/no-sign.log"
: > "$NO_SIGN_LOG"
EU4DLL_CALL_LOG="$NO_SIGN_LOG" run_installer "$TOOLS" "$APP" install >/dev/null || fail "unsigned reinstall"
if grep -qE '^(codesign|xattr):' "$NO_SIGN_LOG"; then fail "installer invoked signing tools"; fi
assert_same "$TMP_ROOT/original" "$APP/Contents/MacOS/eu4_bak"

run_installer "$TOOLS" "$APP" uninstall || fail "uninstall"
assert_same "$TMP_ROOT/original" "$APP/Contents/MacOS/eu4"
grep -qF 'LSUIPresentationMode=7' "$APP/Contents/Info.plist" || fail "original plist value not restored"
assert_absent "$APP/Contents/MacOS/eu4_bak"
assert_absent "$APP/Contents/Frameworks/libeu4dll_mac.dylib"
assert_absent "$APP/Contents/Resources/eu4dll-patch-manifest.bin"
assert_file "$APP/Contents/Resources/keep.txt"
run_installer "$TOOLS" "$APP" uninstall || fail "idempotent uninstall"

UNOWNED="$TMP_ROOT/unowned/eu4.app"
make_app "$UNOWNED"
mkdir -p "$UNOWNED/Contents/Resources/chinese_dict"
printf 'user dictionary\n' > "$UNOWNED/Contents/Resources/chinese_dict/user"
printf 'user dylib\n' > "$UNOWNED/Contents/Frameworks/libeu4dll_mac.dylib"
UNOWNED_PRE=$(tree_digest "$UNOWNED")
run_installer "$TOOLS" "$UNOWNED" uninstall >/dev/null || fail "unowned uninstall"
[ "$UNOWNED_PRE" = "$(tree_digest "$UNOWNED")" ] || fail "unowned uninstall removed user files"

BAD_BACKUP="$TMP_ROOT/bad-backup/eu4.app"
make_app "$BAD_BACKUP"
printf '\nlibeu4dll_mac.dylib\n' >> "$BAD_BACKUP/Contents/MacOS/eu4"
cp "$BAD_BACKUP/Contents/MacOS/eu4" "$BAD_BACKUP/Contents/MacOS/eu4_bak"
BAD_PRE=$(tree_digest "$BAD_BACKUP")
if run_installer "$TOOLS" "$BAD_BACKUP" install >/dev/null 2>&1; then fail "injected backup accepted"; fi
[ "$BAD_PRE" = "$(tree_digest "$BAD_BACKUP")" ] || fail "bad backup rejection changed app"

CORRUPT_STATE="$TMP_ROOT/corrupt-state/eu4.app"
make_app "$CORRUPT_STATE"
mkdir -p "$CORRUPT_STATE/Contents/Resources/eu4dll-install-state"
printf 'fake\n' > "$CORRUPT_STATE/Contents/Resources/eu4dll-install-state/ownership"
CORRUPT_PRE=$(tree_digest "$CORRUPT_STATE")
if run_installer "$TOOLS" "$CORRUPT_STATE" install >/dev/null 2>&1; then fail "corrupt state install accepted"; fi
if run_installer "$TOOLS" "$CORRUPT_STATE" uninstall >/dev/null 2>&1; then fail "corrupt state uninstall accepted"; fi
[ "$CORRUPT_PRE" = "$(tree_digest "$CORRUPT_STATE")" ] || fail "corrupt state changed app"

GAME_UPDATE="$TMP_ROOT/game-update/eu4.app"
make_app "$GAME_UPDATE"
run_installer "$TOOLS" "$GAME_UPDATE" install >/dev/null || fail "game update setup"
printf 'UPDATED-ORIGINAL-EU4-BINARY\n' > "$GAME_UPDATE/Contents/MacOS/eu4"
cp "$GAME_UPDATE/Contents/MacOS/eu4" "$TMP_ROOT/updated-original"
run_installer "$TOOLS" "$GAME_UPDATE" install >/dev/null || fail "game update install"
assert_same "$TMP_ROOT/updated-original" "$GAME_UPDATE/Contents/MacOS/eu4_bak"

MISSING="$TMP_ROOT/missing-tools/eu4.app"
make_app "$MISSING"
if EU4DLL_MANIFEST_TOOL_OVERRIDE="$TMP_ROOT/no-such-tool" run_installer "$TOOLS" "$MISSING" install >/dev/null 2>&1; then fail "missing tool accepted"; fi
assert_same "$TMP_ROOT/original" "$MISSING/Contents/MacOS/eu4"

ROLLBACK="$TMP_ROOT/rollback/eu4.app"
make_app "$ROLLBACK"
if EU4DLL_FAIL_COMMAND=inject run_installer "$TOOLS" "$ROLLBACK" install >/dev/null 2>&1; then fail "partial injection accepted"; fi
assert_same "$TMP_ROOT/original" "$ROLLBACK/Contents/MacOS/eu4"
assert_absent "$ROLLBACK/Contents/MacOS/eu4_bak"
assert_absent "$ROLLBACK/Contents/Resources/eu4dll-patch-manifest.bin"

printf 'PASS: installer fake-app integration scenarios\n'
