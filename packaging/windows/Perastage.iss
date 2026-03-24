; Perastage Windows installer (official) - Inno Setup
; This script keeps file-type registration in HKCU/HKA Software\Classes and
; exposes .mvr association as an optional installer task.

#define MyAppName "Perastage"
#define MyAppVersion "0.1.0"
#define MyAppPublisher "Perasoft"
#define MyAppURL "https://github.com/PeramatoG/Perastage"
#define MyAppExeName "Perastage.exe"

#define AssocProjectExt ".pstg"
#define AssocProjectProgId "Perastage.Project"
#define AssocProjectName "Perastage Project"

#define AssocMvrExt ".mvr"
#define AssocMvrProgId "Perastage.MVR"
#define AssocMvrName "MVR Scene"

[Setup]
AppId={{1A0AC65C-067C-41ED-9A73-5B324ADDD0D8}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppPublisher={#MyAppPublisher}
AppPublisherURL={#MyAppURL}
AppSupportURL={#MyAppURL}
AppUpdatesURL={#MyAppURL}
DefaultDirName={autopf}\{#MyAppName}
UninstallDisplayIcon={app}\{#MyAppExeName}
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
ChangesAssociations=yes
DisableProgramGroupPage=yes
; Replace these paths in CI/local packaging jobs as needed.
LicenseFile=out\install\Release\LICENSE.txt
OutputDir=out\installer
OutputBaseFilename=Perastage_{#MyAppVersion}_Setup
SetupIconFile=out\install\Release\resources\Perastage.ico
SolidCompression=yes
WizardStyle=modern

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"
Name: "spanish"; MessagesFile: "compiler:Languages\Spanish.isl"

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"; Flags: unchecked
Name: "assoc_pstg"; Description: "Associate .pstg project files with {#MyAppName}"; GroupDescription: "File associations"; Flags: checkedonce
Name: "assoc_mvr"; Description: "Associate .mvr files with {#MyAppName} (optional import workflow)"; GroupDescription: "File associations"; Flags: unchecked

[Files]
Source: "out\install\Release\{#MyAppExeName}"; DestDir: "{app}"; Flags: ignoreversion
Source: "out\install\Release\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs

[Registry]
; Register both ProgIDs in Classes (HKA -> HKLM/HKCU based on install mode).
Root: HKA; Subkey: "Software\Classes\{#AssocProjectProgId}"; ValueType: string; ValueName: ""; ValueData: "{#AssocProjectName}"; Flags: uninsdeletekey
Root: HKA; Subkey: "Software\Classes\{#AssocProjectProgId}\DefaultIcon"; ValueType: string; ValueName: ""; ValueData: "{app}\{#MyAppExeName},0"; Flags: uninsdeletekey
Root: HKA; Subkey: "Software\Classes\{#AssocProjectProgId}\shell\open\command"; ValueType: string; ValueName: ""; ValueData: "\"{app}\\{#MyAppExeName}\" \"%1\""; Flags: uninsdeletekey

Root: HKA; Subkey: "Software\Classes\{#AssocMvrProgId}"; ValueType: string; ValueName: ""; ValueData: "{#AssocMvrName}"; Flags: uninsdeletekey
Root: HKA; Subkey: "Software\Classes\{#AssocMvrProgId}\DefaultIcon"; ValueType: string; ValueName: ""; ValueData: "{app}\{#MyAppExeName},0"; Flags: uninsdeletekey
Root: HKA; Subkey: "Software\Classes\{#AssocMvrProgId}\shell\open\command"; ValueType: string; ValueName: ""; ValueData: "\"{app}\\{#MyAppExeName}\" \"%1\""; Flags: uninsdeletekey

; Safe uninstall policy: only remove our OpenWith entries and keep extension owner untouched.
Root: HKA; Subkey: "Software\Classes\{#AssocProjectExt}\OpenWithProgids"; ValueType: string; ValueName: "{#AssocProjectProgId}"; ValueData: ""; Tasks: assoc_pstg; Flags: uninsdeletevalue
Root: HKA; Subkey: "Software\Classes\{#AssocMvrExt}\OpenWithProgids"; ValueType: string; ValueName: "{#AssocMvrProgId}"; ValueData: ""; Tasks: assoc_mvr; Flags: uninsdeletevalue

; Optional default association tasks (user-selected in installer UI).
Root: HKA; Subkey: "Software\Classes\{#AssocProjectExt}"; ValueType: string; ValueName: ""; ValueData: "{#AssocProjectProgId}"; Tasks: assoc_pstg
Root: HKA; Subkey: "Software\Classes\{#AssocMvrExt}"; ValueType: string; ValueName: ""; ValueData: "{#AssocMvrProgId}"; Tasks: assoc_mvr

; Register executable under OpenWith so users can switch default app in Windows UI.
Root: HKA; Subkey: "Software\Classes\Applications\{#MyAppExeName}\shell\open\command"; ValueType: string; ValueName: ""; ValueData: "\"{app}\\{#MyAppExeName}\" \"%1\""; Flags: uninsdeletekey

[Icons]
Name: "{autoprograms}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"
Name: "{autodesktop}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"; Tasks: desktopicon

[Run]
Filename: "{app}\{#MyAppExeName}"; Description: "{cm:LaunchProgram,{#StringChange(MyAppName, '&', '&&')}}"; Flags: nowait postinstall skipifsilent
