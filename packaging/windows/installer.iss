; Inno Setup script for Parvaz Player (Windows x64)

#ifndef AppVersion
#define AppVersion "0.1.0"
#endif

#ifndef SourceDir
#define SourceDir "..\..\build\release"
#endif

[Setup]
AppId={{5E21F58A-2DC7-48DF-9F31-D42299C8D47C}
AppName=Parvaz
AppVersion={#AppVersion}
AppPublisher=Parvaz Player Community
AppPublisherURL=https://github.com/Mrabbas2007/parvaz-player
AppSupportURL=https://github.com/Mrabbas2007/parvaz-player/issues
AppUpdatesURL=https://github.com/Mrabbas2007/parvaz-player/releases
DefaultDirName={autopf}\Parvaz
DefaultGroupName=Parvaz
AllowNoIcons=yes
LicenseFile=..\..\LICENSE
OutputDir=..\..\dist
OutputBaseFilename=parvaz-{#AppVersion}-windows-x64-setup
SetupIconFile=..\..\assets\icons\parvaz.ico
UninstallDisplayIcon={app}\parvaz.exe
Compression=lzma2/ultra64
SolidCompression=yes
WizardStyle=modern
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
PrivilegesRequired=lowest
PrivilegesRequiredOverridesAllowed=dialog

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"

[Files]
Source: "{#SourceDir}\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs

[Icons]
Name: "{autoprograms}\Parvaz"; Filename: "{app}\parvaz.exe"; IconFilename: "{app}\parvaz.exe"
Name: "{autodesktop}\Parvaz"; Filename: "{app}\parvaz.exe"; IconFilename: "{app}\parvaz.exe"; Tasks: desktopicon

[Run]
Filename: "{app}\parvaz.exe"; Description: "{cm:LaunchProgram,Parvaz}"; Flags: nowait postinstall skipifsilent

[Registry]
; Context menu "Play with Parvaz" for video and audio formats
Root: HKCU; Subkey: "Software\Classes\SystemFileAssociations\video\shell\Parvaz"; ValueType: string; ValueData: "Play with Parvaz"; Flags: uninsdeletekey
Root: HKCU; Subkey: "Software\Classes\SystemFileAssociations\video\shell\Parvaz"; ValueType: string; ValueName: "Icon"; ValueData: """{app}\parvaz.exe"""
Root: HKCU; Subkey: "Software\Classes\SystemFileAssociations\video\shell\Parvaz\command"; ValueType: string; ValueData: """{app}\parvaz.exe"" ""%1"""
Root: HKCU; Subkey: "Software\Classes\SystemFileAssociations\audio\shell\Parvaz"; ValueType: string; ValueData: "Play with Parvaz"; Flags: uninsdeletekey
Root: HKCU; Subkey: "Software\Classes\SystemFileAssociations\audio\shell\Parvaz"; ValueType: string; ValueName: "Icon"; ValueData: """{app}\parvaz.exe"""
Root: HKCU; Subkey: "Software\Classes\SystemFileAssociations\audio\shell\Parvaz\command"; ValueType: string; ValueData: """{app}\parvaz.exe"" ""%1"""
