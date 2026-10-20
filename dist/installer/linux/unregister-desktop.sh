#!/bin/bash
# Removes the registration of HelloUtau with the desktop of the current user that
# register-desktop.sh beside this file creates: the entry in the application menu, the shortcut on
# the desktop, the icons and the file types. The entries are removed whichever folder registered
# them. The settings, the voice banks and the plugins of the user are kept.
#
#     unregister-desktop.sh
set -euo pipefail

id=org.openvpi.helloutau
root="$(cd "$(dirname "$0")" && pwd)"
data="${XDG_DATA_HOME:-$HOME/.local/share}"

if [ "$#" -gt 0 ]; then
    echo "Usage: unregister-desktop.sh" >&2
    exit 2
fi

echo "Removing the registration of HelloUtau"

rm -f "$data/applications/$id.desktop"
desktopDirectory="$HOME/Desktop"
if command -v xdg-user-dir > /dev/null; then
    desktopDirectory="$(xdg-user-dir DESKTOP)"
fi
rm -f "$desktopDirectory/$id.desktop"

for type in usth ust uar; do
    rm -f "$data/mime/packages/$id.$type.xml"
done

# The icons that register-desktop.sh copies, by the names in this folder
while IFS= read -r -d '' file; do
    rm -f "$data/icons/hicolor/${file#"$root/share/icons/hicolor/"}"
done < <(find "$root/share/icons/hicolor" -type f -print0)

# The caches of the desktop, as register-desktop.sh updates them
if command -v update-mime-database > /dev/null && [ -d "$data/mime" ]; then
    update-mime-database "$data/mime" || true
fi
if command -v update-desktop-database > /dev/null && [ -d "$data/applications" ]; then
    update-desktop-database "$data/applications" || true
fi
if [ -d "$data/icons/hicolor" ]; then
    touch "$data/icons/hicolor"
    if [ -f "$data/icons/hicolor/icon-theme.cache" ] && command -v gtk-update-icon-cache > /dev/null; then
        gtk-update-icon-cache -f -t -q "$data/icons/hicolor" || true
    fi
fi

echo "Removed."
