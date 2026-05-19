# Shortcuts and Command Bar

This page summarizes keyboard shortcuts and command-bar workflows available to end users.

## Global and viewer shortcuts

The current shortcut map includes:

- `Z` → Fit view (2D/3D, depending on focused viewer)
- `P` → Prefill position command in the command bar
- `R` → Prefill rotation command in the command bar
- `F` → Prefill fixture command in the command bar
- `1` → Open Fixtures tab
- `2` → Open Trusses tab
- `3` → Open Supports/Hoists tab
- `4` → Open Objects tab
- `NumPad1` → Front view (2D/3D)
- `NumPad3` → Side view (2D/3D)
- `NumPad7` → Top view (2D/3D)
- `NumPad5` → Reset 3D viewport

## Command bar workflow

Perastage includes a command-bar/console workflow for fast scene operations.

Practical use:

1. Focus the command bar.
2. Use shortcut prefill (`P`, `R`, `F`) when helpful.
3. Edit the command text.
4. Execute and review the result in tables and viewers.
5. Reuse command history for repetitive tasks.

## Focus behavior and priority

- Shortcuts are blocked while typing in editable text fields/cells.
- Viewer shortcuts run in the currently focused viewer context.
- Dialog-local shortcuts (for example, Create from text autocomplete) stay local to that dialog.

## Create from text local shortcuts

Inside **Tools → Create from text**, the editor autocomplete supports:

- `↑` / `↓` to move through suggestions
- `Enter` or `Tab` to accept the selected suggestion
- `Esc` to close suggestions (without closing the dialog)
