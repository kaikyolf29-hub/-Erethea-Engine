; Erethea Engine installer (Inno Setup 6).
;
; Build from the repository root, with the editor executable already in
; erethea/installer/ under the name erethea.exe:
;
;     ISCC.exe /DMyAppVersion=4.8 erethea\installer\erethea.iss
;
; The installer runs without administrator rights and can add Erethea to the
; user's PATH. It installs next to Godot without touching it: different
; folder, different AppId, different user data folder (%APPDATA%\Erethea).

#ifndef MyAppVersion
  #define MyAppVersion "4.8"
#endif
#define MyAppName "Erethea Engine"
#define MyAppPublisher "Erethea Engine contributors"
#define MyAppURL "https://github.com/kaikyolf29-hub/-Erethea-Engine"
#define MyAppExeName "erethea.exe"

[Setup]
AppId={{B7E2C9A4-5F1D-4E8B-9C3A-2D6F0E1A7B54}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
; Keep the Add/Remove Programs name without the version, it has its own column.
AppVerName={#MyAppName}
AppPublisher={#MyAppPublisher}
AppPublisherURL={#MyAppURL}
AppSupportURL={#MyAppURL}
AppUpdatesURL={#MyAppURL}
AppComments=Erethea Engine editor, based on Godot Engine
ChangesEnvironment=yes
DefaultDirName={localappdata}\Erethea
DefaultGroupName=Erethea Engine
AllowNoIcons=yes
UninstallDisplayIcon={app}\{#MyAppExeName}
SetupIconFile=..\branding\erethea.ico
LicenseFile=..\..\LICENSE.txt
OutputDir=.
OutputBaseFilename=erethea-setup-x86_64
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
Compression=lzma2
SolidCompression=yes
PrivilegesRequired=lowest
WizardStyle=modern

[Languages]
Name: "brazilianportuguese"; MessagesFile: "compiler:Languages\BrazilianPortuguese.isl"
Name: "english"; MessagesFile: "compiler:Default.isl"

[CustomMessages]
brazilianportuguese.AddToPath=Adicionar a Erethea à variável PATH
english.AddToPath=Add Erethea to the PATH environment variable

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"; Flags: unchecked
Name: "modifypath"; Description: "{cm:AddToPath}"; Flags: unchecked

[Files]
Source: "{#MyAppExeName}"; DestDir: "{app}"; Flags: ignoreversion
Source: "erethea.console.exe"; DestDir: "{app}"; Flags: ignoreversion skipifsourcedoesntexist
Source: "..\..\LICENSE.txt"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\..\COPYRIGHT.txt"; DestDir: "{app}"; Flags: ignoreversion

[Icons]
Name: "{group}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"
Name: "{autodesktop}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"; Tasks: desktopicon

[Run]
Filename: "{app}\{#MyAppExeName}"; Description: "{cm:LaunchProgram,{#StringChange(MyAppName, '&', '&&')}}"; Flags: nowait postinstall skipifsilent

[Code]
const
    ModPathName = 'modifypath';
    ModPathType = 'user';

function ModPathDir(): TArrayOfString;
begin
    setArrayLength(Result, 1)
    Result[0] := ExpandConstant('{app}');
end;

#include "..\..\misc\dist\windows\modpath.pas"
