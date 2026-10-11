; The installer of HelloUtau for Windows, compiled with Inno Setup 7 by package.ps1 beside this
; file. See docs/Distribution.md. The command line defines the following values:
;
;   AppVersion           HELLO_VERSION, such as 0.1.6.0
;   SourceDir            the deployed installation, which holds bin and lib
;   OutputDir            the directory of the installer
;   OutputBaseFilename   the file name of the installer without .exe

#ifndef AppVersion
  #error AppVersion is not defined
#endif
#ifndef SourceDir
  #error SourceDir is not defined
#endif
#ifndef OutputDir
  #error OutputDir is not defined
#endif
#ifndef OutputBaseFilename
  #error OutputBaseFilename is not defined
#endif

#define AppName "HelloUtau"
#define AppPublisher "OpenVPI"
#define AppURL "https://github.com/QSynthesis/helloutau"
#define AppExeName "helloutau.exe"
#define AppExe "bin\" + AppExeName
#define RepositoryDir SourcePath + "..\..\.."

[Setup]
; Windows names the uninstall entry after the identifier, and an installer of the same identifier
; upgrades the installation, so the identifier never changes after the first release.
AppId=org.openvpi.helloutau
AppName={#AppName}
AppVersion={#AppVersion}
AppPublisher={#AppPublisher}
AppPublisherURL={#AppURL}
AppSupportURL={#AppURL}/issues
AppUpdatesURL={#AppURL}/releases
VersionInfoVersion={#AppVersion}
DefaultDirName={autopf}\OpenVPI\{#AppName}
DisableProgramGroupPage=yes
; Installed for the current user without administrative privileges unless the user chooses all
; users in the dialog
PrivilegesRequired=lowest
PrivilegesRequiredOverridesAllowed=dialog
SetupArchitecture=x64
ChangesAssociations=yes
LicenseFile={#RepositoryDir}\LICENSE
SetupIconFile={#RepositoryDir}\helloutau\tools\driver\helloutau.ico
UninstallDisplayIcon={app}\{#AppExe}
OutputDir={#OutputDir}
OutputBaseFilename={#OutputBaseFilename}
Compression=lzma2/max
SolidCompression=yes
WizardStyle=modern

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"
Name: "chinesesimplified"; MessagesFile: "compiler:Languages\ChineseSimplified.isl"
Name: "chinesetraditional"; MessagesFile: "compiler:Languages\ChineseTraditional.isl"
Name: "japanese"; MessagesFile: "compiler:Languages\Japanese.isl"

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"; Flags: unchecked
Name: "associateusth"; Description: "{cm:AssocFileExtension,{#AppName},.usth}"; GroupDescription: "{cm:FileAssociations}"
Name: "associateust"; Description: "{cm:AssocFileExtension,{#AppName},.ust}"; GroupDescription: "{cm:FileAssociations}"

[Files]
Source: "{#SourceDir}\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs

[Registry]
; The program is offered in Open With of each file type, without changing the default program of
; the type, because .ust may be associated with UTAU.
Root: HKA; Subkey: "Software\Classes\Applications\{#AppExeName}"; Flags: uninsdeletekey
Root: HKA; Subkey: "Software\Classes\Applications\{#AppExeName}\SupportedTypes"; ValueType: string; ValueName: ".usth"; ValueData: ""; Tasks: associateusth
Root: HKA; Subkey: "Software\Classes\Applications\{#AppExeName}\SupportedTypes"; ValueType: string; ValueName: ".ust"; ValueData: ""; Tasks: associateust

Root: HKA; Subkey: "Software\Classes\.usth\OpenWithProgids"; ValueType: string; ValueName: "HelloUtau.usth"; ValueData: ""; Flags: uninsdeletevalue; Tasks: associateusth
Root: HKA; Subkey: "Software\Classes\HelloUtau.usth"; ValueType: string; ValueName: ""; ValueData: "{cm:UsthFile}"; Flags: uninsdeletekey; Tasks: associateusth
Root: HKA; Subkey: "Software\Classes\HelloUtau.usth\DefaultIcon"; ValueType: string; ValueName: ""; ValueData: "{app}\{#AppExe},0"; Tasks: associateusth
Root: HKA; Subkey: "Software\Classes\HelloUtau.usth\shell\open\command"; ValueType: string; ValueName: ""; ValueData: """{app}\{#AppExe}"" ""%1"""; Tasks: associateusth

Root: HKA; Subkey: "Software\Classes\.ust\OpenWithProgids"; ValueType: string; ValueName: "HelloUtau.ust"; ValueData: ""; Flags: uninsdeletevalue; Tasks: associateust
Root: HKA; Subkey: "Software\Classes\HelloUtau.ust"; ValueType: string; ValueName: ""; ValueData: "{cm:UstFile}"; Flags: uninsdeletekey; Tasks: associateust
Root: HKA; Subkey: "Software\Classes\HelloUtau.ust\DefaultIcon"; ValueType: string; ValueName: ""; ValueData: "{app}\{#AppExe},0"; Tasks: associateust
Root: HKA; Subkey: "Software\Classes\HelloUtau.ust\shell\open\command"; ValueType: string; ValueName: ""; ValueData: """{app}\{#AppExe}"" ""%1"""; Tasks: associateust

[Icons]
Name: "{autoprograms}\{#AppName}"; Filename: "{app}\{#AppExe}"
Name: "{autodesktop}\{#AppName}"; Filename: "{app}\{#AppExe}"; Tasks: desktopicon

[Run]
Filename: "{app}\{#AppExe}"; Description: "{cm:LaunchProgram,{#StringChange(AppName, '&', '&&')}}"; Flags: nowait postinstall skipifsilent

#include "messages.iss"
#include "uninstall.iss"
