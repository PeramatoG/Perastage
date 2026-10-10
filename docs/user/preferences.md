# Preferences

Open **Edit → Preferences**. Switching categories retains pending edits. **Apply** saves without closing, **OK** saves and closes, and **Cancel** discards changes since the last Apply.

## General

- **Language:** choose the interface language; restart Perastage to apply it.
- **Units:** choose distance and weight units. Unit changes apply immediately.
- **Updates:** **Check on startup (recommended)** checks at most once every 24 hours. You can suppress reminders for a specific version. **Manual only** disables startup checks; **Help → Check for Updates** still checks on demand.

## Import

**Rider Import** controls **Auto patch after import**, automatic layers by position or fixture type, and default LX heights, positions and margins. Distance fields follow your unit settings. See [Create from Text](create-from-text.md).

## Viewer

### 3D Viewer

Choose the rendering presentation and **Performance → Model detail**:

- **Standard** (default) balances detail and performance.
- **Low** simplifies heavy models more aggressively.
- **High** shows all available authored detail.

**Use a simplified proxy while navigating** is separate and disabled by default. It temporarily simplifies unselected fixtures during fast navigation, then restores the chosen detail. Apply changes on the next redraw without reopening the project. Source geometry, GDTF resources, 2D symbols and exported geometry remain unchanged.

### Selection & Movement

Choose which object types move their containing root group during mouse, Console and Magnet transforms. Grouped trusses move their group by default; fixtures, supports/hoists and scene objects move independently. Direct group selection moves the group. Table edits affect only the edited object.

**Show anchor references while moving or inserting elements** displays compatible Magnet anchors during movement or insertion. These guides are not scene or export content. Changing movement preferences does not change the project's hierarchy or mark it as edited.

## Formats

### GDTF definition completion

Under **GDTF → GDTF definition completion**:

- **Complete and improve GDTF definitions (recommended)** may fill missing standard Top/Side/Front views from geometry in an audited `@Perastage.gdtf` derivative. Existing authored views, including unusable ones needing explicit repair, are preserved.
- **Preserve imported GDTF definitions** disables automatic completion.

Both allow normal scene editing and explicit GDTF edits. Manually applied Perastage views are project symbols stored in PSTG, rather than replacements for authored GDTF views. Save the project to keep them; they are not exported into MVR or embedded GDTFs.

### MVR Import / Export

**Truss geometry export mode** offers:

- **Standard MVR representation** (default): preserve imported symbol references where possible.
- **Direct Geometry3D for truss symbols:** expand truss symbol references for receiving applications that need direct geometry.

Both write MVR 1.6-compatible output. Project saves keep the standard representation. Import choices are described in [Opening MVR Files](opening-mvr-files.md).
