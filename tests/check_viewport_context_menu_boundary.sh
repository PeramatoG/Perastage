#!/usr/bin/env bash
set -euo pipefail
source "$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/test_tool_requirements.sh"
require_ripgrep
root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
model=("$root/viewer_common/context_menu_model.h" "$root/viewer_common/context_menu_model.cpp")
if rg -n '#include.*(wx|GL|configmanager|gui/|viewer2d/|viewer3d/)' "${model[@]}"; then
    echo "ERROR: the shared viewport action policy must remain GUI/render independent."
    exit 1
fi
for dimension in 2d 3d; do
    panel="$root/viewer$dimension/viewer${dimension}panel.cpp"
    if rg -n 'FixturePatchDialog|GetPopupMenuSelectionFromUser|wxMenu rootMenu' "$panel"; then
        echo "ERROR: viewport menus and patch editing belong to the shared GUI adapters."
        exit 1
    fi
done
echo "Viewport context menu ownership boundary passed."
