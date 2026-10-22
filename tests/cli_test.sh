#!/usr/bin/env bash
set -euo pipefail
bin="$1"
dir="$(mktemp -d)"
trap 'rm -rf "$dir"' EXIT
run() { "$bin" --config-dir "$dir" "$@"; }

run --version | grep -q '^conV '
help="$(run --help)"
! grep -niE 'caelestia|noctalia|quickshell|waybar|island|hypr|shell|desktop' <<<"$help"

run theme preset amber >/dev/null
run theme show | grep -q '"accent": "#ffb000"'
[[ -f "$dir/icon.svg" ]] && grep -q '#ffb000' "$dir/icon.svg"

before="$(cat "$dir/theme.json")"
run theme set 'accent=#112233' --dry-run | grep -q '#112233'
[[ "$(cat "$dir/theme.json")" == "$before" ]]

run theme set 'background=#e00e1e36' >/dev/null
grep -q '"background": "#e00e1e36"' "$dir/theme.json"

! run theme set 'accent=red' 2>/dev/null
! run theme preset nonsense 2>/dev/null

mkdir "$dir/src"
cat > "$dir/src/Colors.qml" <<'QML'
Singleton {
    readonly property color background: "#e00e1e36"
    readonly property color foreground: "#f4b999"
    readonly property color primary: "#9279aa"
}
QML
run theme follow "$dir/src" >/dev/null
run theme show | grep -q '"background": "#e00e1e36"'
run theme show | grep -q '"kind": "file"'
before="$(cat "$dir/theme.json")"
run theme follow "$dir/src" >/dev/null
[[ "$(cat "$dir/theme.json")" == "$before" ]]
echo ok
