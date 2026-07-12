#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "$0")/../../../../.." && pwd)"
cd "$repo_root"

compiler="${CXX:-/usr/bin/c++}"
objdump_tool="${OBJDUMP:-/usr/bin/objdump}"
nm_tool="${NM:-/usr/bin/nm}"
temp_dir="$(mktemp -d "${TMPDIR:-/tmp}/eu4dll-rendering-abi.XXXXXX")"
trap 'rm -rf "$temp_dir"' EXIT

sources=(
    src/mainText.cpp
    src/tooltipAndButtonText.cpp
    src/textLayout.cpp
    src/mapText.cpp
    src/text3D.cpp
)

common_flags=(
    -std=c++17
    -O0
    -g0
    -arch x86_64
    -I"$repo_root/src"
    -I"$repo_root"
)

legacy_defines=(
    -DESCAPE_SEQ_1=0x10
    -DESCAPE_SEQ_2=0x11
    -DESCAPE_SEQ_3=0x12
    -DESCAPE_SEQ_4=0x13
    -DSHIFT_2=0x000E
    -DSHIFT_3=0x0900
    -DSHIFT_4=0x08F2
)

symbol_bytes() {
    local object_file="$1"
    local symbol="$2"
    "$objdump_tool" -d --disassemble-symbols="$symbol" "$object_file" |
        sed -nE 's/^[[:space:]]*[0-9a-f]+:[[:space:]]*([^[:space:]].*)$/\1/p' |
        cut -f1 |
        tr -d ' \n'
}

symbol_relocations() {
    local object_file="$1"
    local symbol="$2"
    "$objdump_tool" -dr --disassemble-symbols="$symbol" "$object_file" |
        sed -nE 's/^[[:space:]]*[0-9a-f]+:[[:space:]]+(X86_64_RELOC_[^[:space:]]+)[[:space:]]+(.*)$/\1 \2/p'
}

checked=0
for source in "${sources[@]}"; do
    stem="$(basename "$source" .cpp)"
    old_source="$temp_dir/$stem.old.cpp"
    old_object="$temp_dir/$stem.old.o"
    new_object="$temp_dir/$stem.new.o"

    git show "HEAD:$source" > "$old_source"
    "$compiler" "${common_flags[@]}" "${legacy_defines[@]}" \
        -c "$old_source" -o "$old_object"
    "$compiler" "${common_flags[@]}" -c "$source" -o "$new_object"

    while IFS= read -r symbol; do
        [[ -n "$symbol" ]] || continue
        old_bytes="$(symbol_bytes "$old_object" "$symbol")"
        new_bytes="$(symbol_bytes "$new_object" "$symbol")"
        if [[ -z "$old_bytes" || -z "$new_bytes" ]]; then
            echo "missing disassembly for $source:$symbol" >&2
            exit 1
        fi
        if [[ "$old_bytes" != "$new_bytes" ]]; then
            echo "naked hook machine code changed: $source:$symbol" >&2
            diff -u \
                <("$objdump_tool" -d --disassemble-symbols="$symbol" "$old_object") \
                <("$objdump_tool" -d --disassemble-symbols="$symbol" "$new_object") || true
            exit 1
        fi
        old_relocations="$(symbol_relocations "$old_object" "$symbol")"
        new_relocations="$(symbol_relocations "$new_object" "$symbol")"
        if [[ "$old_relocations" != "$new_relocations" ]]; then
            echo "naked hook return/call binding changed: $source:$symbol" >&2
            diff -u <(printf '%s\n' "$old_relocations") \
                <(printf '%s\n' "$new_relocations") || true
            exit 1
        fi
        checked=$((checked + 1))
    done < <("$nm_tool" "$new_object" | awk \
        '$NF ~ /naked_|proxy_CGenerateNamesWork_AddNameArea_ToUpper_2/ {print $NF}')
done

if [[ "$checked" -ne 24 ]]; then
    echo "expected 24 naked rendering hook/proxy symbols, checked $checked" >&2
    exit 1
fi

echo "verified $checked rendering naked hook/proxy symbols: machine code and relocation bindings are identical"
