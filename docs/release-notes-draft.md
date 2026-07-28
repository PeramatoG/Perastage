# Perastage v1.6.0 Release Notes

Changes since **v1.5.0**.

## Highlights

## New features and workflow improvements

## Compatibility, stability, and performance

## Important fixes

- Improved portable PDF text extraction so intentionally positioned words keep
  their separation, empty documents are distinguishable from parser failures,
  and PDF output retains valid numeric and cross-reference serialization across
  platforms and locale settings. Cross-platform integration coverage now uses
  deterministic temporary fixtures, avoiding checkout line-ending differences.

- Corrected Rider truss dictionary matching for finish and length variants, and
  ensured GDTF SVG symbols no longer masquerade as renderable 3D geometry when
  an imported truss requires an honest dummy fallback, while resolved truss
  dimensions remain consistent with portable managed dictionary resources.

- Corrected Rider imports so side lighting trusses use their documented default height and LED screens retain accurate dimensions through the shared cube primitive transform.

- Corrected Rider rigging imports so side-fill hoists retain their audio grouping and pipes for lighting bridges, including explicit-length model-free entries, expand consistently across LX positions.

- Standardized filtered Rider previews with compact, stable section spacing across line-ending styles while preventing removed comments from leaving blank sections.
- Stabilized MVR layer and scene-object identities so damaged legacy UUIDs and dependent hierarchy or hoist links recover consistently across project save, reload, and export.
- Preserved Support hoist and Truss metadata through standards-compliant MVR and project roundtrips, including portable auxiliary Truss GDTF resources.
- Hardened MVR metadata recovery against foreign or unsupported legacy payloads, unknown and duplicate identities, invalid numeric and fixture-link values, unsafe auxiliary paths, and missing Truss resources without silent fallback substitution.
- Rejected duplicate and case-colliding MVR archive entries consistently across platforms and surfaced tolerant-recovery diagnostics through normal imports.
- Preserved explicit fixture categories, colors, and valid GDTF mode references across repeated MVR roundtrips while making missing-category inference deterministic.
- Preserved per-fixture visual color overrides, including intentional empty colors, and meaningful fixture ID text in Perastage projects without leaking project-only instance metadata into normal MVR exports or other fixtures of the same type.
- Improved GDTF compatibility and safety for Unicode filesystem paths and archive entry names, including deterministic rejection of malformed UTF-8 identities.

## Current limitations

## Technical and packaging changes

- Standardized embedded layout-image resource names in project archives as
  portable UTF-8 paths with forward slashes on every platform, and strengthened
  layout-package validation against unsafe names hidden by ZIP path
  normalization.

- Improved maintainer issue intake and triage with clearer diagnostic guidance and revision-aware, failure-resilient automation limited to reports explicitly awaiting feedback.

- Aligned CI policy checks with the validated Windows Git Bash initial-cache data flow and strengthened cross-platform filesystem path boundary coverage.

- Added native object-cache acceleration with GitHub-scoped CI isolation, resource-aware Windows embedded-debug validation, and measurable compiler-cache diagnostics without changing installer, dependency-cache, or test behavior.

- Improved dependency-cache warming reliability with complete Linux and macOS build prerequisites, serialized package publication, and credential-safe failure diagnostics.

- Fixed GitHub Packages dependency-cache initialization so clean GitHub Actions runners receive an isolated NuGet configuration before the cache source is added.

- Improved GitHub Packages dependency-cache setup with reliable structural validation and safe, redacted failure diagnostics across all supported build platforms.

- Added a secure, platform-compatible persistent dependency cache with main-only publishing for trusted GitHub Actions builds while retaining fast local workflow caches and read-only behavior for CI and installers.

- Improved GitHub Actions vcpkg caching so dependency builds are saved immediately after successful installation and can be reused across compatible CI and installer workflows.

## Downloads and installation

Choose the package that matches your operating system:

| Operating system | Download |
|---|---|
| **Windows 64-bit** | `Perastage_1.6.0_Setup.exe` |
| **macOS 15 — Apple Silicon** | `Perastage-1.6.0-macOS15-arm64.dmg` |
| **macOS 26 — Apple Silicon** | `Perastage-1.6.0-macOS26-arm64.dmg` |
| **Linux x86-64** | `Perastage-1.6.0-x86_64.AppImage` |
| **Arch Linux x86-64** | `Perastage-1.6.0-arch-x86_64.pkg.tar.zst` |

> **Do not download `Perastage-1.6.0-Debug-Symbols-Developers-Only.zip` unless it is requested for crash analysis or you specifically need the developer debug information.**
>
> This archive is not required to install or run Perastage.

### Windows

Windows SmartScreen may warn that the application is from an unknown publisher because Perastage is an independent open-source project and is not currently code-signed.

Select **More info**, then **Run anyway** to continue.

### macOS

Perastage is not currently notarized by Apple. If macOS blocks the first launch:

1. Open **System Settings → Privacy & Security**.
2. Select **Open Anyway** for Perastage.
3. Confirm that you want to open the application.

### Linux

The AppImage may need executable permission:

```bash
chmod +x Perastage-1.6.0-x86_64.AppImage
```

### Arch Linux

Install the package with:

```bash
sudo pacman -U Perastage-1.6.0-arch-x86_64.pkg.tar.zst
```

## Need help?

Please open a GitHub issue if you encounter a problem. Include the Perastage version, operating system, clear steps to reproduce the issue, and a diagnostic report from the **Help** menu whenever possible.

You can contact the project at **perastage.app@gmail.com**.
