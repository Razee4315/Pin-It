; PinIt installer script (Inno Setup 6)
; Builds a single PinIt_x.y.z_x64-setup.exe with the PinIt logo, Start Menu
; shortcut, optional desktop icon and run-at-startup, and an uninstaller.
;
; Expects the release build at ..\rust\target\release\pinit.exe:
;   cargo build --release --manifest-path rust\Cargo.toml

#define MyAppName "PinIt"
; The version is read from [package] in rust/Cargo.toml, its single source.
; Windows' INI reader handles that section of a TOML file and drops the quotes.
#define MyAppVersion ReadIni(SourcePath + "..\rust\Cargo.toml", "package", "version", "")
#if MyAppVersion == ""
  #error Could not read the version from rust\Cargo.toml
#endif
; File version resources need plain numbers: "3.0.0-alpha.1" -> "3.0.0".
#if Pos("-", MyAppVersion) > 0
  #define MyAppNumericVersion Copy(MyAppVersion, 1, Pos("-", MyAppVersion) - 1)
#else
  #define MyAppNumericVersion MyAppVersion
#endif
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
VersionInfoVersion={#MyAppNumericVersion}
VersionInfoProductTextVersion={#MyAppVersion}

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
; The mutex PinIt holds while it runs (see hand_off_to_running_instance in
; rust/src/host.rs). Lets setup and the uninstaller notice a running PinIt
; and ask to close it, instead of failing to replace or delete PinIt.exe.
AppMutex=PinIt_SingleInstance_v2

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"; Flags: unchecked
Name: "startupicon"; Description: "Start {#MyAppName} automatically when Windows starts"; GroupDescription: "Startup:"; Flags: unchecked

[InstallDelete]
; Up to 2.2.0 PinIt was a Qt app shipped with its runtime next to the exe.
; This build is one exe, so clear that runtime out on upgrade.
Type: files; Name: "{app}\*.dll"
Type: filesandordirs; Name: "{app}\platforms"
Type: filesandordirs; Name: "{app}\styles"
Type: filesandordirs; Name: "{app}\imageformats"
Type: filesandordirs; Name: "{app}\iconengines"
Type: filesandordirs; Name: "{app}\generic"
Type: filesandordirs; Name: "{app}\networkinformation"
Type: filesandordirs; Name: "{app}\tls"
Type: filesandordirs; Name: "{app}\translations"

[Files]
; The whole app: one self-contained exe from the Rust build.
Source: "..\rust\target\release\pinit.exe"; DestDir: "{app}"; DestName: "{#MyAppExeName}"; Flags: ignoreversion

[Icons]
Name: "{group}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"
Name: "{group}\Uninstall {#MyAppName}"; Filename: "{uninstallexe}"
Name: "{autodesktop}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"; Tasks: desktopicon

[Registry]
; Optional autostart. PinIt's "Start PinIt with Windows" checkbox reads and
; writes this same value, so the two always agree.
Root: HKCU; Subkey: "Software\Microsoft\Windows\CurrentVersion\Run"; ValueType: string; \
    ValueName: "PinIt"; ValueData: """{app}\{#MyAppExeName}"" --minimized"; Tasks: startupicon; Flags: uninsdeletevalue
; Autostart may also have been switched on from inside the app, without the
; task above. Remove the value on uninstall either way, so no Run entry is
; left pointing at a deleted PinIt.exe. (ValueType none: nothing is written at
; install time.)
Root: HKCU; Subkey: "Software\Microsoft\Windows\CurrentVersion\Run"; ValueType: none; \
    ValueName: "PinIt"; Flags: dontcreatekey uninsdeletevalue

[Run]
Filename: "{app}\{#MyAppExeName}"; Description: "{cm:LaunchProgram,{#MyAppName}}"; Flags: nowait postinstall skipifsilent
