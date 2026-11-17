; Quickshot — Inno Setup 6 script
; Build Release|x64 first, then compile this with ISCC.exe
; Installs to %LOCALAPPDATA%\Programs\Quickshot so settings.ini next to EXE is writable.

#define MyAppName "Quickshot"
#define MyAppVersion "1.2.1"
#define MyAppPublisher "Ivan Stojmenovic"
#define MyAppURL "mailto:office.stojmenovic@gmail.com"
#define MyAppExeName "Quickshot.exe"

[Setup]
AppId={{B0D6D422-215A-4A97-8FA2-A277CA9991E8}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppPublisher={#MyAppPublisher}
AppPublisherURL={#MyAppURL}
DefaultDirName={localappdata}\Programs\{#MyAppName}
DefaultGroupName={#MyAppName}
DisableProgramGroupPage=yes
OutputDir=..\dist
OutputBaseFilename=Quickshot-Setup-{#MyAppVersion}
SetupIconFile=..\assets\icons\Screenshot_App.ico
Compression=lzma
SolidCompression=yes
WizardStyle=modern
PrivilegesRequired=lowest
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
UninstallDisplayIcon={app}\{#MyAppExeName}

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"; Flags: unchecked
Name: "autostart"; Description: "Start with Windows"; Flags: unchecked

[Files]
Source: "..\build\x64\Release\Quickshot.exe"; DestDir: "{app}"; Flags: ignoreversion
Source: "settings.ini.default"; DestDir: "{app}"; DestName: "settings.ini"; Flags: onlyifdoesntexist

[Icons]
Name: "{group}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"
Name: "{group}\{cm:UninstallProgram,{#MyAppName}}"; Filename: "{uninstallexe}"
Name: "{autodesktop}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"; Tasks: desktopicon

[Registry]
Root: HKCU; Subkey: "Software\Microsoft\Windows\CurrentVersion\Run"; ValueType: string; ValueName: "Quickshot"; ValueData: """{app}\{#MyAppExeName}"""; Flags: uninsdeletevalue; Tasks: autostart

[Run]
Filename: "{app}\{#MyAppExeName}"; Description: "{cm:LaunchProgram,{#StringChange(MyAppName, '&', '&&')}}"; Flags: nowait postinstall skipifsilent
