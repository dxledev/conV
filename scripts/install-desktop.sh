#!/usr/bin/env bash
# Install conV for the current user: binary, launcher entry, icon, app-list entry, and theme source.
set -euo pipefail

project_directory="$(cd -- "${BASH_SOURCE[0]%/*}/.." && pwd)"
build_directory="$project_directory/build"
install_prefix="$HOME/.local"
config_directory="${XDG_CONFIG_HOME:-$HOME/.config}/conV"
apps_list="${XDG_CONFIG_HOME:-$HOME/.config}/apps.list"
follow="${XDG_CONFIG_HOME:-$HOME/.config}/themes/.caelestia-use"
dry_run=0
desktop_id=io.github.conv

usage() {
    printf '%s\n' 'Usage: install-desktop.sh [--dry-run] [--build-dir DIR] [--prefix DIR]' \
        '       [--config-dir DIR] [--apps-list FILE] [--follow PATH|none]'
}

fail() {
    printf 'install-desktop: %s\n' "$*" >&2
    exit 1
}

parse_options() {
    while (( $# )); do
        case "$1" in
            --dry-run) dry_run=1; shift ;;
            --help|-h) usage; exit 0 ;;
            --build-dir|--prefix|--config-dir|--apps-list|--follow)
                (( $# >= 2 )) || fail "Missing value for $1"
                [[ "$2" == none || "$2" == /* && "$2" != *$'\n'* && "$2" != *'|'* && "$2" != *'"'* && "$2" != *'%'* && "$2" != *'\\'* ]] || fail "Expected an absolute path for $1"
                case "$1" in
                    --build-dir) build_directory="${2%/}" ;;
                    --prefix) install_prefix="${2%/}" ;;
                    --config-dir) config_directory="${2%/}" ;;
                    --apps-list) apps_list="$2" ;;
                    --follow) follow="${2%/}" ;;
                esac
                shift 2 ;;
            *) fail "Unknown option: $1" ;;
        esac
    done
}

validate_installation() {
    [[ -x "$build_directory/conV" ]] || fail "Build conV first: $build_directory/conV"
    [[ "$install_prefix" != / && -n "$install_prefix" ]] || fail 'Invalid installation prefix'
    [[ ! -e "$apps_list" || -f "$apps_list" ]] || fail "App list is not a regular file: $apps_list"
    [[ ! -L "$apps_list" ]] || fail "Refusing to replace a symlinked app list: $apps_list"
    [[ ! -L "$install_prefix/bin/conV" ]] || fail 'Refusing to replace a symlinked executable'
    local registered="$install_prefix/share/applications/$desktop_id.desktop"
    local desktop="$install_prefix/bin/applications/$desktop_id.desktop"
    [[ ! -e "$registered" && ! -L "$registered" || -L "$registered" && "$(/usr/bin/readlink -- "$registered")" == "$desktop" ]] ||
        fail "Existing desktop registration is unmanaged: $registered"
    [[ ! -L "$desktop" ]] || fail "Refusing to replace a symlinked desktop entry: $desktop"
    # A default palette that does not exist on this machine falls back to the built-in theme.
    if [[ "$follow" != none && ! -e "$follow" ]]; then
        printf 'install-desktop: %s not found; using the built-in theme.\n' "$follow" >&2
        follow=none
    fi
    if [[ "$follow" != none ]]; then
        "$build_directory/conV" --config-dir "$config_directory" theme follow "$follow" --dry-run >/dev/null
    fi
}

backup_file() {
    [[ -f "$1" ]] || return 0
    /usr/bin/cp -p -- "$1" "$1.conV-backup.$(/usr/bin/date +%Y%m%dT%H%M%S).$$"
}

install_files() {
    local desktop="$install_prefix/bin/applications/$desktop_id.desktop"
    local registered="$install_prefix/share/applications/$desktop_id.desktop"
    /usr/bin/mkdir -p -- "$install_prefix/bin"
    local executable_temporary
    executable_temporary="$(/usr/bin/mktemp "$install_prefix/bin/.conV-binary.XXXXXX")"
    /usr/bin/install -m755 -- "$build_directory/conV" "$executable_temporary"
    /usr/bin/mv -T -- "$executable_temporary" "$install_prefix/bin/conV"
    /usr/bin/install -Dm644 -- "$project_directory/assets/$desktop_id.svg" \
        "$install_prefix/share/icons/hicolor/scalable/apps/$desktop_id.svg"
    /usr/bin/mkdir -p -- "${desktop%/*}" "${registered%/*}"
    backup_file "$desktop"
    local temporary
    temporary="$(/usr/bin/mktemp "${desktop%/*}/.conV-desktop.XXXXXX")"
    /usr/bin/awk -v executable="$install_prefix/bin/conV" -v config="$config_directory" \
        '/^Exec=/ { print "Exec=\"" executable "\" --config-dir \"" config "\" %F"; next } { print }' \
        "$project_directory/assets/$desktop_id.desktop" > "$temporary"
    /usr/bin/chmod 644 -- "$temporary"
    /usr/bin/mv -T -- "$temporary" "$desktop"
    [[ -L "$registered" ]] || /usr/bin/ln -s -- "$desktop" "$registered"
    if [[ -x /usr/bin/update-desktop-database ]]; then
        /usr/bin/update-desktop-database "$install_prefix/share/applications"
    fi
}

register_app() {
    local icon="$install_prefix/share/icons/hicolor/scalable/apps/$desktop_id.svg"
    local entry="$desktop_id.desktop|$icon|conV"
    local temporary
    /usr/bin/mkdir -p -- "${apps_list%/*}"
    backup_file "$apps_list"
    temporary="$(/usr/bin/mktemp "${apps_list%/*}/.conV-apps.XXXXXX")"
    if [[ -f "$apps_list" ]]; then
        /usr/bin/awk -F '|' -v OFS='|' -v id="$desktop_id.desktop" -v icon="$icon" \
            '$1 == id { if (!found) { $2 = icon; print; found = 1 } next } { print } END { if (!found) print id, icon, "conV" }' \
            "$apps_list" > "$temporary"
        /usr/bin/chmod --reference="$apps_list" "$temporary"
    else
        printf '%s\n' "$entry" > "$temporary"
    fi
    /usr/bin/mv -T -- "$temporary" "$apps_list"
}

configure_theme() {
    backup_file "$config_directory/theme.json"
    if [[ "$follow" == none ]]; then
        [[ -f "$config_directory/theme.json" ]] || "$install_prefix/bin/conV" --config-dir "$config_directory" theme preset phosphor >/dev/null
    else
        "$install_prefix/bin/conV" --config-dir "$config_directory" theme follow "$follow" >/dev/null
    fi
}

main() {
    parse_options "$@"
    validate_installation
    if (( dry_run )); then
        printf 'Install executable: %s/bin/conV\nDesktop entry: %s/bin/applications/%s.desktop\nRegister desktop: %s/share/applications/%s.desktop\nApp list: %s\nFollow palette: %s\nSave theme: %s/theme.json\n' \
            "$install_prefix" "$install_prefix" "$desktop_id" "$install_prefix" "$desktop_id" \
            "$apps_list" "$follow" "$config_directory"
        return
    fi
    install_files
    register_app
    configure_theme
    printf 'Installed conV.\n'
}

main "$@"
