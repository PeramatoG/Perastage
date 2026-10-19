# Preferences

Open **Edit → Preferences**. Switching categories retains pending edits. **Apply** saves without closing, **OK** saves and closes, and **Cancel** discards changes since the last Apply.

## General

- **Language:** choose the interface language; restart Perastage to apply it.
- **Units:** choose distance and weight units. Unit changes apply immediately.
- **Updates:** **Check on startup (recommended)** checks at most once every 24 hours. You can suppress reminders for a specific version. **Manual only** disables startup checks; **Help → Check for Updates** still checks on demand.

### Cache

**General → Cache** shows the read-only cache location and current disk usage.
**Open Cache Folder** opens the folder in your system file manager.
**Clear Cache...** asks for confirmation and immediately deletes regenerable
Perastage caches, independently of Apply, OK or Cancel. Geometry is rebuilt as
needed; the GDTF catalog downloads again when online. Configuration, logs,
credentials, libraries, GDTF files, dictionaries and projects are preserved.
If some files cannot be deleted, Perastage reports the failure and updates the
shown size. Persistent caches live under the per-user application data directory
in `cache/`, rather than beside source assets.

## Import

**Rider Import** controls **Auto patch after import**, automatic layers by position or fixture type, and default LX heights, positions and margins. Distance fields follow your unit settings. See [Create from Text](create-from-text.md).

## Viewer

### 3D Viewer

Choose the rendering presentation and **Performance → Model detail**:

- **Standard** (default) displays available geometry first and prepares reduced detail while idle.
- **Low** simplifies heavy models more aggressively and always uses navigation proxies.
- **High** shows all available authored detail.

**Use a simplified proxy while navigating** is enabled by default in Standard and automatic in Low. High preserves full static detail; you can disable its navigation proxy for full-detail inspection. Available proxies temporarily simplify unselected fixtures during fast navigation, then restore the chosen detail. Large scenes fill in progressively so you can start navigating before every model is ready. Apply changes on the next redraw without reopening the project. Source geometry, GDTF resources, 2D symbols and exported geometry remain unchanged.

During loading, the bottom-left status bar shows completed resource requests, the total and a percentage. New geometry appears automatically, without moving the camera. Loading pauses during navigation and resumes from the same position afterwards. A separate preparation message covers navigation proxies; **Ready** appears after the final scene update. If a resource is unavailable, the status bar points to the Console when loading finishes.

Closing the application pauses viewport work before asking whether to save. **Cancel** resumes pending loading; confirming the close discards remaining runtime preparation.

### Selection & Movement

Choose which object types move their containing root group during mouse, Console and Magnet transforms. Grouped trusses move their group by default; fixtures, supports/hoists and scene objects move independently. Direct group selection moves the group. Table edits affect only the edited object.

**Show anchor references while moving or inserting elements** displays compatible Magnet anchors during movement or insertion. These guides are not scene or export content. Changing movement preferences does not change the project's hierarchy or mark it as edited.

## Formats

### GDTF resources

GDTF preferences manage Share credentials. Perastage automatically stores
fixture-wide Top/Front/Side/Bottom symbols in PSTG. Save the project to retain
generated symbols and user overrides; they do not become GDTF or MVR resources.
The Fixture Editor inspects authored SVGs per Model and preserves their bytes
and offsets. Explicit physical-property edits remain available.

### MVR Import / Export

**Automatically close the GDTF download window when finished** is off by
default, including for existing installations. After downloading, the queue
keeps its final statuses, colors, progress and summary visible until you click
**OK** or close the window. The import then continues. Enable this option to
continue automatically when the queue finishes. During downloads, **Cancel**
or the window close button requests cancellation; completed downloads are kept
and remaining fixtures use the existing fallbacks. This preference is saved
across restarts and is independent of GDTF Share credentials.

**Truss geometry export mode** offers:

- **Standard MVR representation** (default): preserve imported symbol references where possible.
- **Direct Geometry3D for truss symbols:** expand truss symbol references for receiving applications that need direct geometry.

Both write MVR 1.6-compatible output. Project saves keep the standard representation. Import choices are described in [Opening MVR Files](opening-mvr-files.md).

Normal export always uses canonical MVR, matching `scene.mvr` saved in PSTG for the same scene state. To create an alternate file, enable **Use compatibility export for this file** in the export dialog and choose the representation. The saved preference suggests that choice; it does not enable compatibility export automatically. Direct Geometry3D expands truss Symbol/Symdef references without changing the project scene or its identifiers.
