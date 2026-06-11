; Inno Setup script for WinTea -- builds a per-user installer (no admin needed).
;   "C:\Program Files (x86)\Inno Setup 6\ISCC.exe" /DMyAppVersion=0.9.0 installer\wintea.iss
; Source exe + version can be overridden with /DSourceExe=... /DMyAppVersion=...

#ifndef MyAppVersion
  #define MyAppVersion "0.9.0"
#endif
#ifndef SourceExe
  #define SourceExe "..\build\release\WinTea.exe"
#endif

#define MyAppName "WinTea"
#define MyAppPublisher "SpookySandwich"
#define MyAppURL "https://github.com/SpookySandwich/WinTea"
#define MyAppExeName "WinTea.exe"

[Setup]
; Stable AppId -- never change this across releases (ties upgrades together).
AppId={{8F2A6B14-3C7D-4E59-AB12-6D9E0F1A2B3C}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppVerName={#MyAppName} {#MyAppVersion}
AppPublisher={#MyAppPublisher}
AppPublisherURL={#MyAppURL}
AppSupportURL={#MyAppURL}
AppUpdatesURL={#MyAppURL}/releases
VersionInfoVersion={#MyAppVersion}
; Per-user install: no UAC prompt for the installer itself.
PrivilegesRequired=lowest
DefaultDirName={localappdata}\Programs\{#MyAppName}
DefaultGroupName={#MyAppName}
DisableProgramGroupPage=yes
DisableReadyPage=no
OutputDir=.
OutputBaseFilename=WinTea-Setup-{#MyAppVersion}
SetupIconFile=..\assets\icon\wintea.ico
UninstallDisplayIcon={app}\{#MyAppExeName}
Compression=lzma2/max
SolidCompression=yes
WizardStyle=modern
; Use Restart Manager to close a running WinTea before replacing the exe.
CloseApplications=yes
RestartApplications=no

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

[Tasks]
Name: "autostart"; Description: "Start {#MyAppName} automatically when I sign in"; GroupDescription: "Startup:"
Name: "desktopicon"; Description: "Create a desktop shortcut"; GroupDescription: "Shortcuts:"; Flags: unchecked

[Files]
Source: "{#SourceExe}"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\README.md"; DestDir: "{app}"; Flags: ignoreversion isreadme
Source: "..\LICENSE"; DestDir: "{app}"; Flags: ignoreversion

[Icons]
; Start Menu entry (uses the exe's embedded teacup icon).
Name: "{group}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"; Comment: "Win+T terminal launcher"
Name: "{group}\{#MyAppName} on GitHub"; Filename: "{#MyAppURL}"
Name: "{userdesktop}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"; Tasks: desktopicon

[Registry]
; Run at sign-in (HKCU, matches the app's own "Start with Windows" toggle).
Root: HKCU; Subkey: "Software\Microsoft\Windows\CurrentVersion\Run"; ValueType: string; \
    ValueName: "WinTea"; ValueData: """{app}\{#MyAppExeName}"""; Tasks: autostart; \
    Flags: uninsdeletevalue

[Run]
Filename: "{app}\{#MyAppExeName}"; Description: "Launch {#MyAppName} now"; \
    Flags: nowait postinstall skipifsilent

[UninstallRun]
; Close the running tray app before files are removed.
Filename: "{app}\{#MyAppExeName}"; Parameters: "--exit"; \
    Flags: skipifdoesntexist runhidden; RunOnceId: "ExitWinTea"

[Code]
// Belt-and-suspenders: ask any running instance to exit before we overwrite it.
function PrepareToInstall(var NeedsRestart: Boolean): String;
var
  ResultCode: Integer;
  ExePath: String;
begin
  ExePath := ExpandConstant('{app}\' + '{#MyAppExeName}');
  if FileExists(ExePath) then
    Exec(ExePath, '--exit', '', SW_HIDE, ewWaitUntilTerminated, ResultCode);
  Result := '';
end;
