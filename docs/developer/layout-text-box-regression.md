# Layout text box rendering and legacy editor verification

## Verified failure stages

The regression fixture models a black initial `P`, subsequent white letters,
a bold accented second line and an additional coloured run. It also includes
vendor metadata that the native rich-text handler cannot round-trip. The
wxWidgets text buffer, extracted runs, HarfBuzz composition and FreeType
outlines contain every supported letter. The preview raster test checks ink
inside each glyph's bounds and checks the transparent counter inside the O.

On the baseline, the editor put white letters on its white editing surface.
Opening the dialog also caused its first native layout to normalize the buffer
after the application had recorded its XML baseline. Accepting the untouched
dialog could therefore serialize the normalized buffer with a recovery copy.
The editor now lays out the native buffer before taking its snapshot. Both the
editor and the shared preview/PDF pipeline normalize legacy foreground colors
to black in memory. Text boxes edited with the new controls store an optional
`useRichTextColors` flag so explicitly chosen colors survive save/reopen. The
editor gives light ink contrast on a neutral canvas when such colors are used.

The Layout texture upload divided edge RGB by alpha even though `wxImage`
already exposes straight RGB. That raised the RGB values of translucent
coloured edges before GL blended them. The renderer now initializes an image
surface with explicit background pixels, completes the graphics context before
reading the image and copies its straight RGB and alpha to the texture. This
also avoids using a selected memory bitmap as the GDI+ alpha target on Windows.

The automated fixture reproduces the editor and texture handling conditions.
The Windows screenshots show white legacy text after the initial black `P`,
consistent with the mixed foreground colors in the regression fixture. An
explicitly chosen white foreground needs contrast in the Layout or PDF.
The reported Windows `CacheOverflowException` requires a native call stack
to identify the throwing component.

## Windows verification

Run the native GUI test from the repository with the Windows Debug
dependencies installed.

```powershell
cmake --preset win-x64-debug-ninja
cmake --build --preset win-debug-build-ninja --target layout_export_input_test
ctest --test-dir build/win-x64-debug-ninja -R "^LayoutExportInput$" --output-on-failure
```

For an end-to-end check in the application, export a Layout containing a text
box as a JSON template. Replace its `layouts[0].textViews[0].richText` with the
literal XML in
`tests/support/legacy_layout_text_fixture.h`. Give it an area of at least
280 × 120 document points and omit `useRichTextColors`. Import the template
with a transparent text background over the light page. Check that both lines
are black in the Layout, Edit Text and Document preview. The old recovery
notice should be absent. Cancel and reopen, then accept without edits. Both
operations should retain the template's exact XML and leave project history
alone.

Select a few letters and choose a color. Append a letter at the end of the
second line, confirm, and verify Undo and Redo. Save and reopen the PSTG, then
export the Layout as PDF. The edited XML contains the supported runs and one
recovery comment containing the original XML. The Layout and PDF agree on the
chosen color and keep the other runs black. Repeat with explicitly chosen white
ink on a dark area and with an opaque white box using dark ink.
