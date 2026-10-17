#ifndef MyAppVersion
  #define MyAppVersion "1.1.2"
#endif

[Setup]
AppName=OpenPreamp
AppVersion={#MyAppVersion}
AppPublisher="OpenGrid / Marcos Deida"
AppPublisherURL="https://github.com/MistaMin/OpenPreamp"
DefaultDirName={commonpf64}\OpenPreamp
DefaultGroupName=OpenPreamp
OutputBaseFilename=OpenPreamp-{#MyAppVersion}-win
LicenseFile=..\LICENSE
PrivilegesRequired=admin
ArchitecturesAllowed=x64
ArchitecturesInstallIn64BitMode=x64

[Files]
Source: "..\build\OpenPreamp_artefacts\Release\VST3\OpenPreamp.vst3\*"; DestDir: "{commoncf64}\VST3\OpenPreamp.vst3"; Flags: ignoreversion recursesubdirs
Source: "..\build\OpenPreamp_artefacts\Release\CLAP\OpenPreamp.clap"; DestDir: "{commoncf64}\CLAP"; Flags: ignoreversion
Source: "..\build\OpenPreamp_artefacts\Release\LV2\OpenPreamp.lv2\*"; DestDir: "{commoncf64}\LV2\OpenPreamp.lv2"; Flags: ignoreversion recursesubdirs
Source: "..\build\OpenPreamp_artefacts\Release\Standalone\OpenPreamp.exe"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\Licenses\*"; DestDir: "{app}\Licenses"; Flags: ignoreversion recursesubdirs
Source: "..\Docs\UserManual.md"; DestDir: "{app}"; DestName: "OpenPreamp-User-Manual.md"; Flags: ignoreversion

[Icons]
Name: "{autoprograms}\OpenPreamp"; Filename: "{app}\OpenPreamp.exe"

[Run]
Filename: "{app}\OpenPreamp.exe"; Description: "Launch OpenPreamp Standalone"; Flags: postinstall skipifsilent nowait
