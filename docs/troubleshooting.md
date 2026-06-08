# Troubleshooting

If Perastage does not behave as expected, use these quick checks.

## MVR file does not open

- Confirm the file extension is `.mvr`.
- Try a different MVR file to isolate whether the issue is file-specific.
- Restart Perastage and retry import/open.

## Fixtures look missing or incorrect

- Download required profiles in **Tools → Download GDTF**.
- Check local mappings in **Tools → Edit dictionaries**.
- Reopen the scene after downloading missing profiles.

## Cannot find local library content

Open the location from:

- **Tools → Open user library folder**

On Windows, the default path is:

- `%APPDATA%\Perastage\library\`

## Layout or print output is not as expected

- Recheck the scene in 2D before exporting.
- Verify table values for fixtures/trusses/hoists/objects.
- Export PDF again after confirming layout adjustments.

## Scene seems inconsistent after many edits

- Save the project.
- Close and reopen it.
- Revalidate critical items in both 2D and 3D views.

## Export diagnostics after a crash or bug

Perastage writes local diagnostics only; it does not upload logs or crash reports automatically.

Use **Help → Open Logs Folder** to view the local logs and crash reports folder. Use **Help → Export Diagnostic Report** to create a plain-text report that includes build information, platform details, captured OpenGL information when available, and recent log lines. Share this file manually only if you are comfortable sending it to the developer.
