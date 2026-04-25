; Perastage Windows installer (official) - Inno Setup
; Portable repository-relative script for packaging\windows\Perastage.iss
; Uses the staged install output under out\install\Release and writes the
; generated installer next to the repository folder, not inside it.

#define MyAppName "Perastage"
#ifndef MyAppVersion
  #error "MyAppVersion is not defined. Pass /DMyAppVersion=<version> to ISCC using PROJECT_VERSION from CMakeLists.txt."
#endif
#define MyAppPublisher "Perasoft"
#define MyAppURL "https://github.com/PeramatoG/Perastage"
#define MyAppExeName "Perastage.exe"

#define AssocProjectExt ".pstg"
#define AssocProjectProgId "Perastage.Project"
#define AssocProjectName "Perastage Project"

#define AssocMvrExt ".mvr"
#define AssocMvrProgId "Perastage.MVR"
#define AssocMvrName "MVR Scene"

#define ProjectFileIconName "PerastageProject.ico"
#define MvrFileIconName "PerastageMVR.ico"

#define RepoRoot "..\\.."
#define StageDir RepoRoot + "\\out\\install\\Release"
#define RepoResourcesDir RepoRoot + "\\resources"
#define OutputParentDir RepoRoot + "\\.."

[Setup]
AppId={{1A0AC65C-067C-41ED-9A73-5B324ADDD0D8}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppPublisher={#MyAppPublisher}
AppPublisherURL={#MyAppURL}
AppSupportURL={#MyAppURL}
AppUpdatesURL={#MyAppURL}
DefaultDirName={autopf}\{#MyAppName}
; Editable library/configuration data is stored per-user under %APPDATA%\Perastage
; by the application runtime (not inside {app}).
UninstallDisplayIcon={app}\{#MyAppExeName}
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
ChangesAssociations=yes
DisableProgramGroupPage=yes
LicenseFile={#StageDir}\LICENSE.txt
OutputDir={#OutputParentDir}
OutputBaseFilename=Perastage_{#MyAppVersion}_Setup
SetupIconFile={#RepoResourcesDir}\Perastage.ico
SolidCompression=yes
WizardStyle=modern dark

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"
Name: "spanish"; MessagesFile: "compiler:Languages\Spanish.isl"

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"
Name: "assoc_mvr"; Description: "Associate .mvr files with {#MyAppName} (recommended for direct MVR viewing)"; GroupDescription: "Optional integrations"

[Files]
Source: "{#StageDir}\{#MyAppExeName}"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#StageDir}\*"; DestDir: "{app}"; Excludes: "{#MyAppExeName}"; Flags: ignoreversion recursesubdirs createallsubdirs

; Dedicated file-type icons stored in the repository resources folder.
Source: "{#RepoResourcesDir}\{#ProjectFileIconName}"; DestDir: "{app}\resources"; Flags: ignoreversion
Source: "{#RepoResourcesDir}\{#MvrFileIconName}"; DestDir: "{app}\resources"; Flags: ignoreversion

[Registry]
; Mandatory .pstg registration
Root: HKA; Subkey: "Software\Classes\{#AssocProjectProgId}"; ValueType: string; ValueName: ""; ValueData: "{#AssocProjectName}"; Flags: uninsdeletekey
Root: HKA; Subkey: "Software\Classes\{#AssocProjectProgId}\DefaultIcon"; ValueType: string; ValueName: ""; ValueData: "{app}\resources\{#ProjectFileIconName}"; Flags: uninsdeletekey
Root: HKA; Subkey: "Software\Classes\{#AssocProjectProgId}\shell\open\command"; ValueType: string; ValueName: ""; ValueData: """{app}\{#MyAppExeName}"" ""%1"""; Flags: uninsdeletekey

Root: HKA; Subkey: "Software\Classes\{#AssocProjectExt}\OpenWithProgids"; ValueType: string; ValueName: "{#AssocProjectProgId}"; ValueData: ""; Flags: uninsdeletevalue
Root: HKA; Subkey: "Software\Classes\{#AssocProjectExt}"; ValueType: string; ValueName: ""; ValueData: "{#AssocProjectProgId}"; Flags: uninsdeletevalue

; Optional .mvr registration
Root: HKA; Subkey: "Software\Classes\{#AssocMvrProgId}"; ValueType: string; ValueName: ""; ValueData: "{#AssocMvrName}"; Flags: uninsdeletekey
Root: HKA; Subkey: "Software\Classes\{#AssocMvrProgId}\DefaultIcon"; ValueType: string; ValueName: ""; ValueData: "{app}\resources\{#MvrFileIconName}"; Flags: uninsdeletekey
Root: HKA; Subkey: "Software\Classes\{#AssocMvrProgId}\shell\open\command"; ValueType: string; ValueName: ""; ValueData: """{app}\{#MyAppExeName}"" ""%1"""; Flags: uninsdeletekey

Root: HKA; Subkey: "Software\Classes\{#AssocMvrExt}\OpenWithProgids"; ValueType: string; ValueName: "{#AssocMvrProgId}"; ValueData: ""; Tasks: assoc_mvr; Flags: uninsdeletevalue
Root: HKA; Subkey: "Software\Classes\{#AssocMvrExt}"; ValueType: string; ValueName: ""; ValueData: "{#AssocMvrProgId}"; Tasks: assoc_mvr; Flags: uninsdeletevalue

; Register executable in Open With
Root: HKA; Subkey: "Software\Classes\Applications\{#MyAppExeName}\shell\open\command"; ValueType: string; ValueName: ""; ValueData: """{app}\{#MyAppExeName}"" ""%1"""; Flags: uninsdeletekey
Root: HKA; Subkey: "Software\Classes\Applications\{#MyAppExeName}\SupportedTypes"; ValueType: string; ValueName: "{#AssocProjectExt}"; ValueData: ""; Flags: uninsdeletekey
Root: HKA; Subkey: "Software\Classes\Applications\{#MyAppExeName}\SupportedTypes"; ValueType: string; ValueName: "{#AssocMvrExt}"; ValueData: ""; Flags: uninsdeletekey

[Icons]
Name: "{autoprograms}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"
Name: "{autodesktop}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"; Tasks: desktopicon

[Run]
Filename: "{app}\{#MyAppExeName}"; Description: "{cm:LaunchProgram,{#StringChange(MyAppName, '&', '&&')}}"; Flags: nowait postinstall skipifsilent
