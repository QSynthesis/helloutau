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
; The identifier of the installation, which names the uninstall entry and the mutex that the
; program holds while it runs. share/helloutau/UpdateConfig.json records the same value.
#define AppId "org.openvpi.helloutau"
#define AppPublisher "OpenVPI"
#define AppURL "https://github.com/QSynthesis/helloutau"
#define AppExeName "helloutau.exe"
#define AppExe "bin\" + AppExeName
#define RepositoryDir SourcePath + "..\..\.."

[Setup]
; Windows names the uninstall entry after the identifier, and an installer of the same identifier
; upgrades the installation, so the identifier never changes after the first release.
AppId={#AppId}
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
; The program started again after an update that it started with /RELAUNCH, as the user who
; started the update rather than as the administrator of an installation for all users
Filename: "{app}\{#AppExe}"; Flags: nowait runasoriginaluser; Check: IsRelaunch

#include "messages.iss"
#include "uninstall.iss"

[Code]
// The program updates itself by starting this installer with the switches of
// UpdateChecker::installerArguments(): /UPDATE, and /RELAUNCH if the program starts again after
// the update. See the update of the installed application in docs/Distribution.md.

const
  // The longest wait for the program to end, in milliseconds
  ExitTimeout = 60000;

// Returns whether the command line has the switch Name, in any letter case.
function HasSwitch(const Name: String): Boolean;
var
  I: Integer;
begin
  Result := False;
  for I := 1 to ParamCount do
    if CompareText(ParamStr(I), Name) = 0 then
    begin
      Result := True;
      Exit;
    end;
end;

function IsRelaunch(): Boolean;
begin
  Result := HasSwitch('/UPDATE') and HasSwitch('/RELAUNCH');
end;

// The program starts this installer before it ends, and holds its mutex until its process has
// ended. The installer waits for the mutex to disappear, so that no file of the program is in use,
// and fails if the program does not end in time.
function InitializeSetup(): Boolean;
var
  Waited: Integer;
begin
  Result := True;
  if not HasSwitch('/UPDATE') then
    Exit;
  Waited := 0;
  while CheckForMutexes('{#AppId}') do
  begin
    if Waited >= ExitTimeout then
    begin
      Log('The program did not end in time.');
      Result := False;
      Exit;
    end;
    Sleep(250);
    Waited := Waited + 250;
  end;
end;
