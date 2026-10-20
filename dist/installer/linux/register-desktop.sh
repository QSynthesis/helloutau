#!/bin/bash
# Registers this folder of HelloUtau with the desktop of the current user: the entry in the
# application menu, the shortcut on the desktop, the icons, and the file types that HelloUtau is
# offered for in Open With. unregister-desktop.sh beside this file removes them. Both are in the
# root of the package of Linux, see docs/Distribution.md. Running this script again after the
# folder moves updates the entries, and the options of the last run replace those of the earlier
# runs.
#
#     register-desktop.sh [--no-menu] [--no-desktop] [--no-usth] [--ust] [--uar]
#
#     --no-menu     hides the entry in the application menu. The entry still exists, because the
#                   desktop takes the icon of the windows and the programs of the file types from
#                   it.
#     --no-desktop  creates no shortcut on the desktop
#     --no-usth     does not offer HelloUtau for .usth files
#     --ust         offers HelloUtau for .ust files
#     --uar         offers HelloUtau for .uar files
#
# .ust and .uar are the formats of UTAU, which the user may keep for UTAU, and are therefore off by
# default, as in the installer of Windows. No option changes the default program of a file type.
set -euo pipefail

id=org.openvpi.helloutau
root="$(cd "$(dirname "$0")" && pwd)"
program="$root/bin/helloutau"
data="${XDG_DATA_HOME:-$HOME/.local/share}"

# Prints the usage from the comment above.
usage() {
    sed -n '/^#     register-desktop.sh/,/^#     --uar/s/^# \{0,1\}//p' "$0"
}

menu=1
desktop=1
declare -A types=([usth]=1 [ust]=0 [uar]=0)
for argument in "$@"; do
    case "$argument" in
        --no-menu) menu=0 ;;
        --no-desktop) desktop=0 ;;
        --no-usth) types[usth]=0 ;;
        --ust) types[ust]=1 ;;
        --uar) types[uar]=1 ;;
        -h | --help)
            usage
            exit 0
            ;;
        *)
            echo "Unknown option: $argument" >&2
            usage >&2
            exit 2
            ;;
    esac
done

if [ ! -x "$program" ]; then
    echo "The program $program does not exist." >&2
    exit 1
fi
# A desktop entry cannot hold a line break in a value. A percent sign is written as %% in the Exec
# key, but GLib looks for the program before it restores the percent sign, and therefore ignores
# the entry.
case "$root" in
    *$'\n'*)
        echo "The path of the folder contains a line break." >&2
        exit 1
        ;;
    *%*)
        echo "The path of the folder contains a percent sign, which the desktop does not accept." >&2
        exit 1
        ;;
esac

# Returns the program as the first argument of the Exec key of the Desktop Entry Specification.
# A reserved character in a quoted argument is escaped with a backslash, and each backslash is
# escaped again because the value is a string. The path has no percent sign, see above.
quotedProgram() {
    printf '"%s"' "$(printf '%s' "$program" | sed -e 's/[\\"`$]/\\&/g' -e 's/\\/\\\\/g')"
}

# Returns the program as a value of the type string, in which a backslash is escaped, because
# sequences such as \s denote other characters
escapedProgram() {
    printf '%s' "$program" | sed -e 's/\\/\\\\/g'
}

# Writes the entry of the package with the program of this folder and the chosen file types into
# the file $1. $2 is 1 if the entry is hidden in the application menu.
writeEntry() {
    local mimeTypes="" type line
    for type in usth ust uar; do
        if [ "${types[$type]}" = 1 ]; then
            mimeTypes+="application/x-$type;"
        fi
    done
    mkdir -p "$(dirname "$1")"
    while IFS= read -r line; do
        case "$line" in
            Exec=*) printf 'Exec=%s %%F\n' "$(quotedProgram)" ;;
            TryExec=*) printf 'TryExec=%s\n' "$(escapedProgram)" ;;
            MimeType=*) [ -z "$mimeTypes" ] || printf 'MimeType=%s\n' "$mimeTypes" ;;
            *) printf '%s\n' "$line" ;;
        esac
        if [ "$line" = "[Desktop Entry]" ] && [ "$2" = 1 ]; then
            printf 'NoDisplay=true\n'
        fi
    done < "$root/share/applications/$id.desktop" > "$1"
}

echo "Registering $root"

# The icons, all of which are copied because the entry and the file types name them
while IFS= read -r -d '' file; do
    install -D -m 644 "$file" "$data/icons/hicolor/${file#"$root/share/icons/hicolor/"}"
done < <(find "$root/share/icons/hicolor" -type f -print0)

# The file types, each of which is declared only if HelloUtau is offered for it
for type in usth ust uar; do
    package="$data/mime/packages/$id.$type.xml"
    if [ "${types[$type]}" = 1 ]; then
        install -D -m 644 "$root/share/mime/packages/$id.$type.xml" "$package"
        echo "  Offered for .$type files"
    else
        rm -f "$package"
    fi
done

writeEntry "$data/applications/$id.desktop" "$((1 - menu))"
if [ "$menu" = 1 ]; then
    echo "  Added to the application menu"
fi

# The shortcut, if the user has a desktop directory. xdg-user-dir returns the home directory if
# none is configured. GNOME starts a shortcut only if it is executable and trusted.
desktopDirectory="$HOME/Desktop"
if command -v xdg-user-dir > /dev/null; then
    desktopDirectory="$(xdg-user-dir DESKTOP)"
fi
shortcut="$desktopDirectory/$id.desktop"
if [ "$desktop" = 1 ]; then
    if [ "$desktopDirectory" != "$HOME" ] && [ -d "$desktopDirectory" ]; then
        writeEntry "$shortcut" 0
        chmod 755 "$shortcut"
        if command -v gio > /dev/null; then
            gio set "$shortcut" metadata::trusted true 2> /dev/null || true
        fi
        echo "  Added a shortcut to $desktopDirectory"
    else
        echo "  No shortcut is created, because no desktop directory exists."
    fi
else
    rm -f "$shortcut"
fi

# The caches of the desktop, if their tools are installed. GTK uses the cache of the icons only if
# it is newer than the directory.
if command -v update-mime-database > /dev/null; then
    update-mime-database "$data/mime" || true
fi
if command -v update-desktop-database > /dev/null; then
    update-desktop-database "$data/applications" || true
fi
touch "$data/icons/hicolor"
if [ -f "$data/icons/hicolor/icon-theme.cache" ] && command -v gtk-update-icon-cache > /dev/null; then
    gtk-update-icon-cache -f -t -q "$data/icons/hicolor" || true
fi

echo "Registered. unregister-desktop.sh removes the registration."
