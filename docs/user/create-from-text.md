# Create from Text

Use **Tools → Create from text** to turn rider-style text into a starting scene. Review the generated result before using it as show documentation.

## Inputs and workflow

Pasted text, `.txt` files and `.pdf` files are supported; PDFs have their text extracted before parsing.

1. Open **Tools → Create from text** and paste text or load a rider file.
2. Choose **Apply filter** to clean candidate lines, then review and edit the filtered text.
3. Choose **Create**. If fixture definitions need review, complete **Resolve fixture types**.
4. Choose **Resolve and create** to confirm the plan and begin any selected downloads.

## Fixture-type resolution

Known types use the active fixture dictionary. For unresolved types, review cached GDTF Share suggestions, use **Use suggested** for a strong match, or **Search...** to choose a profile. You can correct a type in the table and clear **Create** to omit that row's fixtures while retaining other rider elements.

Opening the review does not require sign-in. A first catalog acquisition or download may require it. Accepted mappings are remembered for later riders only after confirmation. Cancelling before creation leaves scene content and dictionary mappings unchanged.

### Generic fallback

**Use generic** provides a one-import fallback without saving a dummy mapping. An absent catalog, cancelled sign-in or network failure still permits Generic creation. Recoverable download, validation, mode, authentication or dictionary errors fall back only for the affected type; the remaining import continues.

### DMX modes

Choose the intended mode explicitly when a profile has several modes. Double-click the **Mode** cell to review choices. Suggestions without a chosen mode use Generic fallback. Confirmed dictionary-mode changes are saved after **Resolve and create**.

## Created content and parser highlights

Depending on the text, the workflow creates fixtures, trusses, hoists/motors and scene objects such as screens. Screen/video-control entries are not treated as lighting fixtures.

Useful patterns include quantities (`12 Spot`), combined lines (`8 Wash + 4 Beam`), positions such as `LX1`, `FLOOR` and `SIDES`, and truss/pipe or hoist entries. Rider Import [preferences](preferences.md#import) control automatic patching, layer organization and default LX placement.

## Autocomplete and validation

The editor suggests common keywords and local dictionary terms. Use Up/Down to navigate, Enter or Tab to accept, and Esc to close suggestions.

After creation, check the Fixtures, Trusses, Hoists and Objects tables, DMX modes and patch. Verify placement in [2D and 3D](views.md), save PSTG and [export MVR](opening-mvr-files.md#export-mvr) if needed.

For detailed parsing and placement contracts, see [Text-to-scene rules](../developer/text_to_scene_rules.md).
