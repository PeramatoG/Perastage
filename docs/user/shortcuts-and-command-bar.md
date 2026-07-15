# Shortcuts and Command Bar

This reference page consolidates the most practical keyboard, mouse, and command-bar workflows from the in-app help so you can work faster without leaving the docs shell.

## Global shortcuts

| Shortcut | Action |
| --- | --- |
| `Ctrl+N` | New project |
| `Ctrl+L` | Load project |
| `Ctrl+S` | Save project |
| `Ctrl+Q` | Close application |
| `Ctrl+Z / Ctrl+Y` | Undo / Redo |
| `Del` | Delete selection |
| `Ctrl+G` | Group the active cross-table selection |
| `Ctrl+U` | Ungroup selected objects from their direct group |
| `F1` | Open help |
| `F` | Focus CLI and prefill `fixture ` (outside editable widgets) |
| `1 / 2 / 3 / 4` | Switch to Fixtures / Trusses / Hoists / Objects |

## Viewer shortcuts

| Shortcut | Action |
| --- | --- |
| `Z` | Fit view (focused 2D/3D viewer) |
| `NumPad1` | Front view |
| `NumPad3` | Side/Right view |
| `NumPad7` | Top view |
| `NumPad5` | Reset 3D camera orientation |
| `Arrow keys` (3D) | Orbit camera |
| `Shift + Arrow keys` (3D) | Pan camera |
| `Alt + Arrow keys` (2D/3D) | Zoom in/out |
| `Arrow keys` (2D) | Pan view |

## Console input shortcuts

| Shortcut | Action |
| --- | --- |
| `Esc` | Exit prompt and re-enable app shortcuts |
| `Up / Down` | Navigate command history |
| `Home` | Move to start of input (after prompt) |
| `Left / Backspace` | Cannot move before prompt |

## Mouse shortcuts (3D viewer)

| Action | Result |
| --- | --- |
| Left drag | Orbit camera |
| Shift + left drag or middle drag | Pan camera |
| Mouse wheel | Zoom in/out |
| Left click | Select fixture/truss/object under cursor |
| Shift/Ctrl + left click | Toggle selection |
| Ctrl + left drag | Rectangle select |
| Double click fixture label | Open fixture patch dialog |

## Command bar quick workflow

1. Focus the command bar.
2. Optionally prefill with `P`, `R`, or `F`.
3. Edit the command text.
4. Execute and verify the result in tables/viewers.
5. Reuse command history for repetitive changes.

## Console commands (frequent and useful)

### Selection commands

| Command | Description |
| --- | --- |
| `clear` | Clear fixtures, trusses, and objects selections |
| `f ...` | Select fixtures by ID |
| `t ...` | Select trusses by unit number (clears truss selection first) |

Selection syntax examples:

- Single ID: `f 12`
- Range: `f 1-5`, `f 1 thru 5`, `f 1 t 5`
- Add/remove: `f + 10 - 3`
- Mixed: `f 1 3 5 7-9`

### Position and rotation commands

| Command | Description |
| --- | --- |
| `pos x <values>` | Set X positions |
| `pos y <values>` | Set Y positions |
| `pos z <values>` | Set Z positions |
| `pos <x>,<y>,<z>` | Set X/Y/Z together |
| `x <values>` / `y <values>` / `z <values>` | Axis shortcuts for `pos` |
| `rot x <values>` / `rot y <values>` / `rot z <values>` | Set roll/pitch/yaw |
| `rot x\|y\|z <values> --group` or `--g` | Rotate full selection as one group |

Notes:

- One value applies to all selected items.
- Two values distribute linearly from start to end across selection; `t` and `thru` are accepted as optional range separators, so `pos x -7 t 7`, `pos x -7 thru 7`, and `pos x -7 7` are equivalent.
- Use `++` / `--` for relative offsets (for example `pos x ++ 1.5`).
- Group rotation pivot defaults to selection bounding-box center.
- You can override pivot with a trailing `x,y,z` triplet, for example `rot y ++45 --g -2.5,0,0`.

## Focus and priority behavior

- Shortcuts are blocked while typing in editable text fields/cells.
- Viewer shortcuts run in the currently focused viewer.
- Dialog-local shortcuts remain local to that dialog.

## Create from text dialog local shortcuts

| Shortcut | Action |
| --- | --- |
| `↑` / `↓` | Move through suggestions |
| `Enter` or `Tab` | Accept selected suggestion |
| `Esc` | Close suggestions without closing dialog |
