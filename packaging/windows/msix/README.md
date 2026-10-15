# Perastage Store packaging (STORE-100)

This is an **additional, opt-in Store target** for the 1.7.0 release line.
Inno Setup remains the official Windows direct/GitHub download. Both targets
consume the same validated `out/install/Release` tree; MSIX never rebuilds it.
The root `VERSION` remains canonical, including on pre-1.7.0 main. This change
does not bump it to 1.7.0 or automatically publish anything to Microsoft Store.

## Identity and version

The canonical `identity.json` contains the exact Partner Center Product Identity
supplied for Perastage:

| Manifest field | Exact value |
| --- | --- |
| `Package/Identity/Name` | `LuismaPeramato.Perastage` |
| `Package/Identity/Publisher` | `CN=B827ED57-156E-455A-8B2D-C8B1FEC883AE` |
| `Package/Properties/PublisherDisplayName` | `Luisma Peramato` |
| PFN (informational/testing only) | `LuismaPeramato.Perastage_vrw2ms6ps8f0j` |

The build uses that committed identity by default. `-IdentityFile` permits an
explicit local/test JSON input; `identity.example.json` documents its structure.
Never change the production values for a release. Values are used verbatim,
including case, punctuation and internal spaces; invalid/surrounding whitespace
is rejected, never trimmed. XML escaping preserves the decoded original value.
PFN is informational only: verify it against the installed package; never use it
as `Identity/Name` or generate one.
Keep Name, Publisher and Application IDs stable across releases.
See [Store identity requirements][requirements] and [Identity schema][identity].

One conversion in `msix.py` maps `major.minor.patch` to `major.minor.patch.0`:
`1.7.0` becomes `1.7.0.0`. Major must be 1..65535; minor/patch 0..65535.
Revision stays zero for Store use. Prefixes, leading zeros, prereleases and build
suffixes fail. No build timestamp, CI run number or second release-version source
is used. See [Store version numbering][requirements].

## Manifest and runtime choices

- The manifest language list derives from the existing `release_languages()`
  reader in `packaging/verify_localization.py`, whose owner is
  `PERASTAGE_TRANSLATION_LANGUAGES` in `cmake/PerastageLocalization.cmake`.
  Default English (`en-US`) is first; internal locale underscores become BCP-47
  hyphens (`zh_CN` → `zh-CN`). Generation and tests reject missing, unexpected,
  incorrectly mapped or reordered default language declarations.
- A sealed x64 `Windows.Desktop` package, using the documented modern
  `uap10:RuntimeBehavior="packagedClassicApp"` and `uap10:TrustLevel="mediumIL"`
  combination with `runFullTrust`. No UWP/AppContainer migration or bootstrapper.
  `Perastage.exe` remains the primary app. See [desktop package components][components]
  and [Application schema][application].
- Minimum Windows 10 version 2004, build `10.0.19041.0`: the chosen `uap10`
  activation/subsystem declarations require this floor. Audited CMake/MSVC,
  wxWidgets and Win32 socket code reveal no higher explicit OS requirement.
  Verify the final compiler/dependency build on that floor before submission;
  toolchain upgrades can raise it. `MaxVersionTested` currently equals this
  compatibility floor and is **not a claim of an executed Windows test**. Update
  it to the verified release test OS when recording readiness.
- The Microsoft retail x64 `Microsoft.VCLibs.140.00.UWPDesktop` dependency is
  read from the framework package selected for the release toolchain. Its real
  version becomes `MinVersion`; no example CRT floor is assumed. Staging currently
  skips `InstallRequiredSystemLibraries` installation, and hosted runners have
  runtimes preinstalled. Store dependency resolution must cover clean machines.
  Use the matching VS retail framework, not Debug or UWP-only VCLibs, and test on
  a VM without VS/VCRedist. See [Desktop C++ Runtime framework packages][vclibs].
- `desktop6:FileSystemWriteVirtualization` and `RegistryWriteVirtualization`
  are disabled with the restricted `unvirtualizedResources` capability. This
  preserves direct-install/external-process access to user libraries, caches and
  HKCU-backed MVR-xchange settings, including persistence after uninstall. Explain
  that scope to Partner Center if capability review is requested. It grants no
  elevation, HKLM access or package-directory writes. See [flexible virtualization][virtualization].
- `.pstg` and `.mvr` use `uap3:FileTypeAssociation`, `MultiSelectModel="Single"`
  and quoted `Parameters="\"%1\""` (XML-escaped in the source). They inherit
  the GUI executable, passing a filesystem path to the existing startup parser.
  No URL activation or registry takeover/migration declarations. Both become
  available in Open With; Windows/user consent controls defaults. MSIX has no
  Inno-style optional association checkbox, so MVR registration is present but
  users choose whether to use it. See [FileTypeAssociation schema][association].
- The single GUI Application owns a `uap5:Extension` explicitly targeting
  `perastage-cli.exe` with `packagedClassicApp`/`mediumIL`, containing the
  console `uap5:AppExecutionAlias`. The CLI stays in the payload without a second
  Application entry (including a hidden entry). The Application declares
  `uap10:SupportsMultipleInstances="true"`, required by MakeAppx for a console
  alias; its subsystem remains GUI/default while only the alias is console.
  Windows requires the `.exe`
  suffix in the declaration; normal shells also resolve `perastage-cli` through
  PATHEXT. There is no PATH mutation. Users may disable the alias in Settings,
  and an earlier PATH entry can shadow it. See [alias schema][alias] and
  [desktop packaging extensions][extensions].
- Required package logos (50, 44 and 150 pixels) and file icons are committed
  separately in `assets/`. They derive directly from `resources/Perastage_logo_1024.png`
  and the original project/MVR ICOs. Optional banners/splash assets are omitted.
  CI does not rescale. `generate_assets.py` is a maintainer-only Pillow command;
  review regenerated PNGs. Store listing screenshots/artwork are separate manual
  Partner Center requirements; reuse existing branding. See [listing images][images].

### Explorer file icons

The old association logos were single 32px PNGs. This loses the classic ICO's
native size variants and uses Windows' packaged PNG document presentation;
transparent margins can make the artwork look smaller still.
Each association now declares both a legacy `uap:Logo` PNG and a
[desktop7:Logo][desktop-logo] ICO. On Windows builds supporting desktop7 (19645
and later), the dedicated `StoreAssets/ProjectFile.ico` and `MvrFile.ico` reuse
the original Inno artwork byte-for-byte, including 16/32/48/64/128/256px images.
No PNG target-size/scale variants or resource-index generation are needed for
this native multi-image ICO path. App tile/Store logos are unchanged.

`desktop7` is ignorable so the minimum OS remains 19041. Older Windows builds
retain the PNG fallback and may still show Windows' document backing. The
fallbacks are now 44px, generated once from original ICO artwork with exterior
transparent margins cropped, preserving aspect ratio. This follows Microsoft's
[file icon size/padding guidance][file-icons]. Regenerate with
`python packaging/windows/msix/generate_assets.py` (Pillow required locally).
Packaging checks require the ICOs to match their canonical resource originals.

After installing/upgrading a development-signed package, select Perastage as the
default for both extensions and compare Explorer small/medium/large/extra-large
views at 100%, 150% and 200% DPI on Windows 11 and the minimum Windows 10 build.
Verify double-click opening still works. If Explorer shows cached old icons,
restart Explorer/sign out before comparing; do not modify association registry
entries. Native rendering and older-OS fallback appearance require this visual
check on Windows; schema validation alone cannot confirm their appearance.

## Audit before implementation

Audit base: main `02fbb16f47483a59a0e36e45c1b7a7cfcd83e834` (VERSION 1.6.145).
Inspected Inno script, Windows/minor release workflows, artifact contract, common
install/staging/platform CMake owners, resources, startup/CLI, AppPaths,
ProjectUtils/library bootstrap, runtime/cache storage, GDTF network/credential
services, MVR-xchange settings, local IPC and OSC transports.

| Area | Finding / release verification |
| --- | --- |
| Stage layout | GUI/CLI at root with adjacent DLLs, resources, library, help and licenses; suitable for copying without relocation. Existing release checks validate CLI/localization and exclude symbols. |
| Mutable files | `AppPaths` uses `%LOCALAPPDATA%\Perastage` primarily, with wx user-data and system-temp fallback. Preferences, last project, editable library and cache paths stay outside the package. Installer comments mentioning only `%APPDATA%` do not describe the current primary path. |
| Installed library | `GetDefaultLibraryPath` probes installed-library writability. The sealed package denies it; the existing `GetWritableLibraryPath` fallback bootstraps the per-user library. Confirm this fallback and GDTF editing/downloads on a clean account. No Core change is justified by this handled probe. |
| Legacy roaming data | Catalog/credential legacy migration may read the wx user-data (`%APPDATA%`) directory. Verify existing roaming data and primary LocalAppData independently; do not relocate them. |
| Registry | No runtime association hacks or unrestricted HKLM writes found. `wxConfig("Perastage")` stores MVR-xchange preferences under HKCU with the app's Perasoft vendor. Disabling registry virtualization preserves sharing with the direct installation. |
| Credentials | Existing native wx SecretStore/Credential Manager remains used; user consent/login and credential access need packaged runtime testing. No signing or account secrets are introduced. |
| Cache/temp/import/export | Persistent cache uses `AppPaths`; operation/session workspaces use system temp. MVR/GDTF exports use user-selected destinations. Package resources are read-only inputs; projects must be saved to user-writable folders. |
| IPC/OSC | Local IPC is loopback TCP, OSC is loopback UDP; no named-pipe/AppContainer namespace migration. Retain existing enablement/ports. Validate both packaged alias and direct external CLI clients. |
| Network/helpers | Full-trust libcurl GDTF downloads, MVR-xchange sockets/mDNS and browser/help launches retain Win32 semantics; firewall/network policies still apply. No installed service, driver or elevated helper is required by the audited paths. |
| Virtualization | Default Desktop Bridge redirection could hide newly-created AppData/HKCU data from direct installs/external clients and remove it on uninstall. Manifest opt-outs preserve shared user-owned data. Coexisting installs share preferences/library, so simultaneous edits remain an existing data-conflict risk. |

No Core, GUI, CLI, data-path or framework behavior is changed. These are source
audit conclusions, **not installed-MSIX test results**. See [packaged desktop runtime behavior][runtime].

## Build locally / in CI

First use the normal Windows Release build/staging commands in
[the packaging guide](../../../docs/developer/packaging.md). Install a Windows SDK
with x64 MakeAppx and the retail Desktop VCLibs framework matching the compiler.
The usual VS framework path is under `Microsoft SDKs\Windows Kits\10\ExtensionSDKs\Microsoft.VCLibs.Desktop\14.0\Appx\Retail`.
Then, from the repository root:

```powershell
python tests/check_windows_msix_packaging.py -v
./packaging/windows/msix/build.ps1
# If SDK/framework discovery is unavailable or multiple installations exist:
./packaging/windows/msix/build.ps1 `
  -SdkBinDir 'C:\path\to\Windows Kits\10\bin\SDK-version\x64' `
  -VCLibsPackage 'C:\path\to\matching\retail-x64-Desktop-framework.appx'
```

Output: `out/msix/Perastage_<VERSION>_x64.msix`, for example
`Perastage_1.7.0_x64.msix`. Inno output stays
`out/installer/Perastage_<VERSION>_Setup.exe`. A new output filename is required;
archive previous results or remove a stale output explicitly before retrying.
`MakeAppx pack /h SHA256` runs with semantic/schema validation enabled, followed
by byte/hash checks for the manifest, complete staged payload and assets. Missing
identity, executables, artwork/framework, invalid version, stale layout or SDK
failures terminate packaging. Symbols, keys, certificates and reserved package
files in the stage are rejected; no private symbols are copied. The stage is
never written. Temporary layout is removed even on failure.
Inputs, naming and layout timestamps are deterministic. Bit-identical archives
across different SDK/toolchain versions are not promised; record the logged SDK
and framework version with the release. See [MakeAppx validation][makeappx].

In `.github/workflows/windows-installer.yml`, opt in using `build_msix=true` or
repository variable `PERASTAGE_STORE_MSIX_ENABLED=true` with the committed exact
identity and the runner's matching SDK framework. Missing identity
in an enabled target fails; a disabled target reports its status. There is one
dependency/application build and no Store publication. The
`store-msix-validation.yml` PR workflow first validates a **synthetic test package**
with MakeAppx, then calls this same Windows release lane with `build_msix=true`.
This exercises both packaging targets without an independent application build.
It uploads a Store-only artifact and does not certify or publish the application.

Official `Minor Draft Release` runs explicitly pass `build_msix: true`, producing
the Inno Setup installer and unsigned Store submission MSIX from the same release
source and validated stage, independently of the repository variable. With
`dry_run=true`, only preflight validation runs: no release commit, installers,
MSIX, tags or releases are created. Partner Center submission remains manual.

**1.7.0 policy: Store-only CI artifact (B).**
`Store-Perastage-windows-msix` deliberately does not match the minor release's
`Perastage-*` artifact download glob. The public artifact contract and minor
release assembler stay unchanged. Normal users continue to receive the Inno
EXE; the unsigned MSIX must never enter the public validated release assets.

## Signing and local installation

Upload the **unsigned `.msix` from the Store-only artifact** to Partner Center.
Microsoft signs/re-signs MSIX after certification; no purchased CA certificate
or production PFX secret is needed. Sideloading/local installation requires a
trusted signature. See [Store signing][requirements] and [test certificates][certificate].

Development-only example on a disposable Windows VM: use an exact Publisher
subject, a non-exportable local key and a **copy** of the Store package. Keep the
Store output unchanged; never upload/distribute the development copy or cert.

```powershell
$identity = Get-Content packaging/windows/msix/identity.json -Raw | ConvertFrom-Json
$version = (Get-Content VERSION -Raw).Trim()
$devDir = Join-Path $env:TEMP 'Perastage-store-local-test'
New-Item -ItemType Directory $devDir -Force | Out-Null
$devPackage = Join-Path $devDir "Perastage_${version}_x64.msix"
Copy-Item "out/msix/Perastage_${version}_x64.msix" $devPackage
$cert = New-SelfSignedCertificate -Type Custom -Subject $identity.Publisher `
  -CertStoreLocation Cert:\CurrentUser\My -KeyUsage DigitalSignature `
  -KeyExportPolicy NonExportable -HashAlgorithm SHA256 `
  -TextExtension @('2.5.29.37={text}1.3.6.1.5.5.7.3.3', '2.5.29.19={text}')
$publicCert = Join-Path $devDir 'local-test.cer'
Export-Certificate -Cert $cert -FilePath $publicCert | Out-Null
# Run the following trust import from an elevated shell on this test VM only:
Import-Certificate -FilePath $publicCert -CertStoreLocation Cert:\LocalMachine\TrustedPeople
# Use signtool.exe from the same SDK x64 bin directory used for packaging:
signtool.exe sign /fd SHA256 /s My /sha1 $cert.Thumbprint $devPackage
if ($LASTEXITCODE -ne 0) { throw 'Development signing failed' }
signtool.exe verify /pa $devPackage
if ($LASTEXITCODE -ne 0) { throw 'Development signature verification failed' }
Add-AppxPackage -Path $devPackage -DependencyPath 'C:\path\to\matching\retail-x64-Desktop-framework.appx'
$app = Get-AppxPackage | Where-Object { $_.Name -ceq $identity.Name }
$app | Select-Object Name, Publisher, Version, PackageFamilyName, InstallLocation
explorer.exe "shell:AppsFolder\$($app.PackageFamilyName)!Perastage"
& "$env:LOCALAPPDATA\Microsoft\WindowsApps\perastage-cli.exe" --version
& "$env:LOCALAPPDATA\Microsoft\WindowsApps\perastage-cli.exe" --help
perastage-cli --version
perastage-cli --help
```

Each CLI call must exit 0, display the canonical version/help and resolve the
packaged alias. Confirm with `Get-Command perastage-cli.exe`; inspect Settings >
App execution aliases when disabled/shadowed. Also test output redirection, exit
codes, relative paths, spaces/Unicode and concurrent CLI invocations. Do not
manually edit PATH. A real clean install test must have no existing Perastage
package and no VS/VCRedist masking dependency mistakes.

## Runtime/upgrade checklist (manual release gate)

Record package SHA-256, exact Name/Publisher/PublisherDisplayName/PFN, framework,
SDK/compiler, OS build, account and each result. Back up user data first.

- Clean install; GUI launch from Start; create/save/load a project in Documents.
- Change Preferences, restart, and confirm `%LOCALAPPDATA%\Perastage` files and
  legacy `%APPDATA%\Perastage` persistence. Inspect both from an unpackaged process;
  check package `LocalCache` for unexpected redirected copies.
- Access bundled GDTF library; edit per-user entries; log in to GDTF Share,
  download a fixture and restart with working downloads/catalog/geometry caches.
- Import/export MVR; open `.pstg` and `.mvr` through Open With (also spaces/Unicode).
  Confirm an existing MVR default stays user-controlled; test setting defaults explicitly.
- Execute alias `--version`/`--help`, CLI inspection/export and documented
  [local-live workflow](../../../docs/developer/cli.md) with both the packaged
  alias and external direct CLI. Verify caller working directory and stdout/stderr.
- Enable supported OSC loopback control and MVR-xchange/mDNS/external network
  transfers; test firewall-restricted/offline behavior as well as connectivity.
- Build the next canonical VERSION from its validated stage, retaining identical
  identity/Application IDs; sign a local copy with the same test cert. Close the
  app, `Add-AppxPackage` the newer package and verify version, data, preferences,
  file registrations and alias. Never edit only the manifest to fake an upgrade.
- Uninstall with `Remove-AppxPackage -Package $app.PackageFullName`; confirm
  registrations/alias removed and **user-owned unvirtualized data/HKCU preferences
  retained**, matching direct-install semantics. Reinstall and verify reuse. User
  data removal, if desired, is explicit and separate from package uninstall.
- Remove only this test certificate from CurrentUser\My and LocalMachine\TrustedPeople
  by its recorded thumbprint, and remove the temporary dev copy/public cert.

## WACK and submission readiness

### STORE-110 findings and corrections

Maintainer testing of `LuismaPeramato.Perastage_1.6.145.0_x64__vrw2ms6ps8f0j`
reported overall **WARNING**, with an Application Count failure, optional
Blocked Executables findings and a DPI warning. This is the original report,
not a successful WACK result for the corrected package.

- **Application Count: actual manifest issue.** Previously there were two
  Applications (`Perastage` and hidden `PerastageCLI`). There is now only
  `Perastage`/`Perastage.exe`; its alias extension explicitly launches
  `perastage-cli.exe` as a full-trust console executable. The main GUI AUMID and
  file-association parameters remain unchanged. See [extension executable and
  runtime attributes][alias-extension] and [Desktop Bridge WACK tests][bridge-tests].
  The [Application schema][application] requires explicit multi-instancing for
  console activations. SDK semantic validation also requires that declaration
  on the owning Application, even when the alias targets a different executable.
  This allows repeated packaged GUI activations as well; the native startup has
  no singleton redirection and direct installs already allow separate GUI
  processes. Retest repeated GUI/file opens and the existing local-live endpoint
  ownership alongside concurrent CLI commands. No GUI console subsystem is set.
- **Blocked Executables: classify before correcting.** Microsoft's optional
  test concerns Windows S restrictions; it is not evidence that an imported
  process API or a matching string was actually executed. The audit found:

  | Finding class | Source/meaning | Certification note |
  | --- | --- | --- |
  | Actual process launch | `gui/mainwindow_menu.cpp`, `OnOpenLogsFolder`, uses `wxExecute("explorer ...")` on Windows. | Opens the diagnostics directory; no command interpreter. Retain and verify on the supported OS. |
  | Default handlers | `mainwindow_menu.cpp`, `mainwindow.cpp`, `user_manual_dialog.cpp` and `preferences/cache_preferences_page.cpp` use `wxLaunchDefaultBrowser`/`wxLaunchDefaultApplication`. | User-requested URLs, documentation and directories delegated to Windows handlers; retain functionality. |
  | wxWidgets DLL imports | Generic `wxExecute` uses `CreateProcess`; default handlers can use ShellExecute. | Framework machinery, not proof of launching a forbidden utility. The [pinned wxWidgets source][wx-execute] confirms direct CreateProcess for wxExecute. |
  | Utility names in dependency binaries | The supplied report examples mention cmd/reg/bash/PowerShell strings. | A string alone is not a launch path. Attach the exact DLL/version/offset from the full report and inspect reachability before claiming a specific match is benign. |
  | Documentation and bundled GDTF matches | Non-executable text/resource data. | False positives as process-launch evidence; do not alter user documentation or authored fixture data. |

  Repository production C/C++ searches found no `system`, `popen`, direct
  CreateProcess/ShellExecute call or cmd.exe/PowerShell launch. An unused
  `popen` macro definition is not a call. Build/test PowerShell and other shell
  tooling is not application runtime. No dependency or data was removed to
  suppress the scanner. A real runtime dependency on a blocked command utility,
  if found in the full report or dynamic testing, requires a separate targeted
  correction; no such call was established by this source audit.
- **DPI: missing explicit executable metadata; processing error not diagnosed.**
  The original GUI RC only declared an icon; no application-owned DPI manifest
  was embedded by CMake. Both Windows executables now embed native manifests
  requesting `asInvoker`, without UIAccess. GUI declarations are `dpiAware=true`
  and `dpiAwareness=System`; the headless CLI deliberately has no GUI DPI setting
  because the terminal host owns its display. Native manifest versions derive
  from CMake PROJECT_VERSION/root VERSION and do not alter MSIX identity/version.

  System DPI is the conservative baseline supported by the pinned wxWidgets
  high-DPI guidance. Perastage has some FromDIP sizing but also fixed pixel
  sizes and custom GL rendering; toolkit support alone does not establish safe
  per-monitor behavior. No PerMonitorV2 claim or runtime DPI API override is
  added. Mixed-monitor bitmap scaling and a WACK preference for PerMonitorV2
  may remain. Verify controls, dialogs, GL views and picking at 100/150/200%
  and while moving between monitors. See [Microsoft DPI declarations][dpi] and
  [wxWidgets high-DPI guidance][wx-dpi].

  MSVC uses CMake's normal manifest merge/embedding; non-MSVC Windows uses an
  explicit RC resource. The same declarations apply to direct/Inno and Store
  builds. Packaging extracts resource #1 from both actual staged executables
  using SDK mt.exe and checks the declarations. This addresses missing metadata;
  the original WACK "could not be processed" diagnostic still requires the
  detailed report/tool log, not an assumption that the new declarations fix it.

### Repeat validation locally

Build/stage the new canonical version and generate the unsigned MSIX using the
normal command above. Sign only a development copy and install the matching
VCLibs dependency as described in the local-test section. Run:

```powershell
$alias = Join-Path $env:LOCALAPPDATA 'Microsoft/WindowsApps/perastage-cli.exe'
# This explicit path avoids an unpackaged CLI earlier in PATH.
& $alias --version
& $alias --help
& $alias capabilities --json
python tests/perastage_cli_smoke_test.py $alias (Get-Content VERSION -Raw).Trim()
# Launch the GUI, then verify the local IPC workflow:
& $alias live query scene.summary
```

Also repeat inspect/scene workflows with known GDTF/MVR inputs, stdout/stderr
redirection, exit codes, two concurrent CLI invocations, `.pstg`/`.mvr`
double-clicks, user-library writes, and the existing upgrade/persistence checklist.
Repeat GUI DPI checks for the direct installer as well as MSIX. Record alias
runtime results separately from schema validation: hosted CI does not install a
trusted development certificate or exercise the real WindowsApps alias.

On a Windows test machine with the current Windows App Certification Kit,
an active interactive user session and the trusted development copy/dependency:

```powershell
$wack = "${env:ProgramFiles(x86)}\Windows Kits\10\App Certification Kit\appcert.exe"
& $wack reset
& $wack test -appxpackagepath $devPackage -reportoutputpath (Join-Path $devDir 'wack-report.xml')
# Review every report result, not just the process exit status.
```

Use a fresh report filename for the corrected version and record installed
Name/Publisher/Version/PackageFamilyName, SDK/WACK version and Windows build.
Expect Application Count to have one entry; inspect the native DPI results
separately for GUI and console CLI. Optional string/import findings may remain.
Attach their classification and exact flagged binary/data evidence to Partner
Center certification notes; do not claim that all findings were dynamically
cleared. Required failures, unprocessed-executable diagnostics and any actual
blocked-utility launches remain actionable. **WACK has not been rerun by this
implementation; final Store certification remains manual.**

WACK can activate the GUI and measure hardware-dependent behavior and requires
an active session (not Session0). It is a maintainer readiness command, not an
every-PR hosted-runner test. Review the desktop test workflow/report and record
any actionable failure before submission. **WACK has not been executed as part
of the Linux source implementation.** See [WACK procedure][wack].

Partner Center manual steps: confirm the committed exact identity, reserve the Perastage
display name, enable Store packaging, build the actual 1.7.0 stage, complete the
runtime/upgrade checklist and WACK, explain restricted capabilities, upload the
unsigned MSIX, complete listing screenshots/descriptions/age ratings/privacy and
availability fields as applicable, review validation and submit manually.

Plain `.msix` is the scoped first submission path. `.msixupload` can include
`.appxsym` public PDBs to improve Store crash analytics; existing release PDBs
are collected separately and are not a reviewed public-symbol set. MakeAppx
does not produce an upload container. Follow-up: strip/verify public symbols for
GUI **and CLI**, pair exact binary/PDB identifiers, create/validate the upload
container and inspect its contents independently. Do not ZIP the existing private
symbol artifact into a Store upload. See [upload package/symbol guidance][requirements].

For future releases, update only root VERSION through normal release automation,
stage/validate once, reuse identity, review toolchain/framework/OS compatibility,
rerun packaging/checklist/WACK, and upload the higher-version MSIX manually.

[requirements]: https://learn.microsoft.com/en-us/windows/apps/publish/publish-your-app/msix/app-package-requirements
[identity]: https://learn.microsoft.com/en-us/uwp/schemas/appxpackage/uapmanifestschema/element-identity
[components]: https://learn.microsoft.com/en-us/windows/msix/desktop/desktop-to-uwp-manual-conversion
[application]: https://learn.microsoft.com/en-us/uwp/schemas/appxpackage/uapmanifestschema/element-application
[vclibs]: https://learn.microsoft.com/en-us/troubleshoot/developer/visualstudio/cpp/libraries/c-runtime-packages-desktop-bridge
[virtualization]: https://learn.microsoft.com/en-us/windows/msix/desktop/flexible-virtualization
[runtime]: https://learn.microsoft.com/en-us/windows/msix/desktop/desktop-to-uwp-behind-the-scenes
[association]: https://learn.microsoft.com/en-us/uwp/schemas/appxpackage/uapmanifestschema/element-uap3-filetypeassociation
[desktop-logo]: https://learn.microsoft.com/en-us/uwp/schemas/appxpackage/uapmanifestschema/element-desktop7-logo
[file-icons]: https://learn.microsoft.com/en-us/windows/apps/develop/launch/handle-file-activation#step-2-add-the-proper-icons
[alias]: https://learn.microsoft.com/en-us/uwp/schemas/appxpackage/uapmanifestschema/element-uap5-appexecutionalias
[alias-extension]: https://learn.microsoft.com/en-us/uwp/schemas/appxpackage/uapmanifestschema/element-uap5-extension
[bridge-tests]: https://learn.microsoft.com/en-us/windows/uwp/debug-test-perf/windows-desktop-bridge-app-tests
[dpi]: https://learn.microsoft.com/en-us/windows/win32/hidpi/setting-the-default-dpi-awareness-for-a-process
[wx-dpi]: https://github.com/wxWidgets/wxWidgets/blob/v3.3.3/docs/doxygen/overviews/high_dpi.md
[wx-execute]: https://github.com/wxWidgets/wxWidgets/blob/v3.3.3/src/msw/utilsexc.cpp
[extensions]: https://learn.microsoft.com/en-us/windows/apps/desktop/modernize/desktop-to-uwp-extensions
[images]: https://learn.microsoft.com/en-us/windows/apps/publish/publish-your-app/msix/screenshots-and-images
[makeappx]: https://learn.microsoft.com/en-us/windows/msix/package/create-app-package-with-makeappx-tool
[certificate]: https://learn.microsoft.com/en-us/windows/msix/package/create-certificate-package-signing
[wack]: https://learn.microsoft.com/en-us/windows/uwp/debug-test-perf/windows-app-certification-kit
