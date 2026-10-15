# Perastage 1.7.0

Perastage 1.7.0 is a major update focused on better MVR/GDTF inspection,
fixture resources, performance, documentation and external integration.

It also includes many usability, compatibility and stability improvements
across the application.

## Highlights

- New read-only **MVR / GDTF Inspector** for projects and external files.
- Improved **GDTF Share** browser with faster loading and more useful fixture information.
- Custom fixture symbols can now travel with the **Perastage project**.
- New **Low / Standard / High 3D Model Detail** settings for better performance control.
- Reorganized Preferences and improved cache management.
- Experimental **command-line, OSC and external integration tools**.
- Offline Help now available in **English, Spanish, French, German and Simplified Chinese**.

## MVR / GDTF Inspector

The new Inspector makes it easier to understand the contents of MVR and GDTF files
without modifying them.

You can:

- Inspect the current project or open external MVR/GDTF files.
- Browse scene hierarchy, embedded fixture definitions and package contents.
- Review fixture information, DMX modes, channels, functions and wheels.
- Inspect original XML with search and folding.
- Preview supported SVG, image and 3D resources.
- Review compatibility issues and diagnostics.

Large packages are loaded in the background where possible so the application
remains responsive while inspecting complex files.

## GDTF and fixture symbols

Fixture symbols can now be stored directly with the **PSTG project**, allowing
custom symbols to travel with the project between computers.

Perastage also handles manufacturer-provided GDTF symbols more carefully,
preserving original resources while allowing project-specific representations
where needed.

Symbol rendering has been improved, including correct transparency and holes.

## GDTF Share

Browsing GDTF Share is now faster and more informative.

- Cached fixture information can appear immediately and remains available offline.
- Catalog updates run in the background.
- Search can match manufacturer, fixture, UUID, revision and DMX mode information.
- Results provide more revision and DMX mode information before downloading.

Online fixture downloads still require a GDTF Share account.

## Performance and usability

Perastage now provides three levels of 3D model detail:

- **Low**
- **Standard** (default)
- **High**

This makes it easier to balance visual detail and performance, especially with
large scenes.

An optional simplified representation can also be used while navigating complex
3D scenes.

Other improvements include:

- Reorganized Preferences.
- New cache information and cleanup controls.
- Improved table sorting.
- Smoother object dragging in 2D and 3D.
- Improved Layout Viewer interaction.
- Better handling of large and complex MVR/GDTF packages.
- Numerous stability and cross-platform fixes.

## Experimental CLI and integrations

Perastage 1.7.0 introduces the first experimental tools for controlling and
integrating Perastage with other applications.

The included **perastage-cli** can:

- Inspect MVR and GDTF files.
- Query information from a running Perastage instance.
- Perform supported scene operations.
- Process MVR files without modifying the original input.

An experimental local **OSC interface** is also available for a limited set of
scene operations.

Perastage also provides an experimental path for external MCP integrations.
This is intended for advanced users and developers and is not included as a
separate end-user component.

These interfaces are still experimental and may evolve in future releases.

## Compatibility and stability

This release includes a large number of fixes and compatibility improvements,
including:

- Improved Linux AppImage compatibility.
- More reliable MVR import and inspection.
- Better handling of embedded GDTFs and fixture resources.
- Inspector stability improvements on Windows and macOS.
- Improved Unicode path handling.
- Safer grouping, ungrouping and object conversion operations.
- Improved Undo behavior for scene editing operations.

## Documentation and Help

Perastage now uses a unified documentation system for the website and built-in Help.

Offline Help is included in:

- English
- Spanish
- French
- German
- Simplified Chinese

Press **F1** at any time to open the documentation.

## Downloads

Choose the package for your platform from the **Perastage 1.7.0 GitHub Release**:

| Platform | Package |
|---|---|
| Windows 64-bit | Installer |
| Linux x86-64 | AppImage |
| macOS 15 Apple Silicon | DMG |
| Current macOS Apple Silicon | DMG |
| Arch Linux x86-64 | Package |

The experimental CLI is included with the normal Perastage package and does not
require a separate download.

Documentation:
https://perastage.luismaperamato.com/

Issues and feedback:
https://github.com/PeramatoG/Perastage/issues
