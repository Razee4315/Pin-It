; PinIt installer script (Inno Setup 6)
; Builds a single PinIt_x.y.z_x64-setup.exe with the PinIt logo, Start Menu
; shortcut, optional desktop icon and run-at-startup, and an uninstaller.

#define MyAppName "PinIt"
#define MyAppVersion "1.0.0"
#define MyAppPublisher "Saqlain Abbas"
#define MyAppURL "https://github.com/Razee4315/Pin-It"
#define MyAppExeName "PinIt.exe"

[Setup]
; Stable AppId — keep this constant across versions so upgrades replace cleanly.
AppId={{8F3A2C1E-5B4D-4E9A-9C7F-1A2B3C4D5E6F}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppVerName={#MyAppName} {#MyAppVersion}
AppPublisher={#MyAppPublisher}
AppPublisherURL={#MyAppURL}
AppSupportURL={#MyAppURL}
AppUpdatesURL={#MyAppURL}

; Per-user install (no admin prompt), matching the original PinIt installer.
PrivilegesRequired=lowest
DefaultDirName={autopf}\PinIt
DisableProgramGroupPage=yes
DefaultGroupName=PinIt

; Branding / icons
SetupIconFile=..\resources\icon.ico
UninstallDisplayIcon={app}\{#MyAppExeName}
UninstallDisplayName={#MyAppName}
WizardStyle=modern

; Output
OutputDir=..\release
OutputBaseFilename=PinIt_{#MyAppVersion}_x64-setup

; Compression + architecture
Compression=lzma2
SolidCompression=yes
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible

; Gracefully close a running PinIt before installing/uninstalling.
CloseApplications=yes
RestartApplications=no

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"; Flags: unchecked
Name: "startupicon"; Description: "Start {#MyAppName} automatically when Windows starts"; GroupDescription: "Startup:"; Flags: unchecked

[Files]
; The full self-contained bundle produced by windeployqt.
Source: "..\dist\PinIt\*"; DestDir: "{app}"; Flags: recursesubdirs ignoreversion

[Icons]
Name: "{group}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"
Name: "{group}\Uninstall {#MyAppName}"; Filename: "{uninstallexe}"
Name: "{autodesktop}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"; Tasks: desktopicon

[Registry]
; Optional autostart (the app's own setting writes the same key, so this stays in sync).
Root: HKCU; Subkey: "Software\Microsoft\Windows\CurrentVersion\Run"; ValueType: string; \
    ValueName: "PinIt"; ValueData: """{app}\{#MyAppExeName}"""; Tasks: startupicon; Flags: uninsdeletevalue

[Run]
Filename: "{app}\{#MyAppExeName}"; Description: "{cm:LaunchProgram,{#MyAppName}}"; Flags: nowait postinstall skipifsilent
