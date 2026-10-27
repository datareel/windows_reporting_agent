; DataReel Reporting Agent for Windows installer (Inno Setup 6)
;
; Build the programs first (nmake), then:
;   "C:\Program Files (x86)\Inno Setup 6\ISCC.exe" drra_installer.iss
; or: nmake installer
;
; The setup asks for the report server on a wizard page. For unattended
; installs (GPO, PDQ, a login script) pass the settings on the command line:
;
;   drra_setup_3.3.exe /VERYSILENT /SUPPRESSMSGBOXES /NORESTART
;       /SERVER=reports.example.com /PORT=443 /URLPATH=/windows/upload
;       /TOKEN=<enrollment token> [/HTTP] [/INTERVAL=<minutes>]
;
; Delivery to a file share (CIFS) instead of, or next to, HTTPS:
;
;       /METHODS=cifs /CIFSSHARE=\\fileserver\reports
;       [/CIFSUSER=<share user> /CIFSPASS=<password>]
;
; Setup logs record the command line: do not combine /CIFSPASS with /LOG,
; or delete the log afterwards.
; /CIFSUSER and /CIFSPASS are for shares outside a Windows domain. The
; password is stored encrypted (DPAPI) in C:\ProgramData\DataReel\ReportingAgent.
;
; /URLPATH is the web path of the folder that holds upload.php and
; register.php. /HTTP turns off HTTPS (test servers only). /INTERVAL sets
; the minutes between reports (default 15). Settings not
; given keep the value from an existing install, or the default.
;
; Test builds before 3.3 were named "Windows Reporting Agent" (service
; WindowsReportAgent, C:\Program Files\WindowsReportAgent). This setup
; moves such an install over: settings, agent data, encrypted share
; password and machine key are kept, the old service and folders are
; removed.

#define AppVersion "3.3"
#define SvcName "DataReelReportAgent"
#define AppTitle "DataReel Reporting Agent"
#define DataDir "{commonappdata}\DataReel\ReportingAgent"
; Names used by the test builds before 3.3
#define OldSvcName "WindowsReportAgent"
#define OldAppId "{7AC8D5E4-C9B2-4216-B192-7844742EC7CC}"

[Setup]
AppId={{834749A3-54B1-4719-B5C6-A553FCDBAA9E}
AppName={#AppTitle}
AppVersion={#AppVersion}
AppVerName={#AppTitle} for Windows {#AppVersion}
AppPublisher=DataReel Software Development
AppPublisherURL=https://datareel.com
DefaultDirName={autopf}\DataReel\ReportingAgent
DisableProgramGroupPage=yes
; Administrative privileges are required to manage services
PrivilegesRequired=admin
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
OutputBaseFilename=drra_setup_{#AppVersion}
OutputDir=output
LicenseFile=LICENSE
Compression=lzma2
SolidCompression=yes
SetupLogging=yes
UninstallDisplayName={#AppTitle} for Windows
UninstallDisplayIcon={app}\drra_svc.exe

[Files]
Source: "drra_svc.exe"; DestDir: "{app}"; Flags: ignoreversion
Source: "drra_messages.dll"; DestDir: "{app}"; Flags: ignoreversion
Source: "send_test_report.exe"; DestDir: "{app}"; Flags: ignoreversion
Source: "svcconfig.exe"; DestDir: "{app}"; Flags: ignoreversion
Source: "svccontrol.exe"; DestDir: "{app}"; Flags: ignoreversion
Source: "list_all_apps.exe"; DestDir: "{app}"; Flags: ignoreversion
Source: "list_all_patches.exe"; DestDir: "{app}"; Flags: ignoreversion
Source: "list_defender_alerts.exe"; DestDir: "{app}"; Flags: ignoreversion
Source: "get_ip_address.exe"; DestDir: "{app}"; Flags: ignoreversion
Source: "get_service_tag.exe"; DestDir: "{app}"; Flags: ignoreversion
Source: "README.md"; DestDir: "{app}"; Flags: ignoreversion
Source: "LICENSE"; DestDir: "{app}"; Flags: ignoreversion
; Keep an edited INI file on upgrade and uninstall
Source: "drra.ini"; DestDir: "{app}"; Flags: onlyifdoesntexist uninsneveruninstall

[Dirs]
; Agent certificate, registration state, report and log file
Name: "{#DataDir}"; Flags: uninsneveruninstall

[Registry]
Root: HKLM; Subkey: "SYSTEM\CurrentControlSet\Services\EventLog\Application\{#SvcName}"; ValueType: string; ValueName: "EventMessageFile"; ValueData: "{app}\drra_messages.dll"; Flags: uninsdeletekey
Root: HKLM; Subkey: "SYSTEM\CurrentControlSet\Services\EventLog\Application\{#SvcName}"; ValueType: dword; ValueName: "TypesSupported"; ValueData: "0x00000007"; Flags: uninsdeletekey

[Run]
; Only SYSTEM and Administrators may read or change the agent data (S-1-5-18, S-1-5-32-544)
Filename: "{sys}\icacls.exe"; Parameters: """{#DataDir}"" /inheritance:r /grant:r *S-1-5-18:(OI)(CI)F *S-1-5-32-544:(OI)(CI)F"; Flags: runhidden; StatusMsg: "Securing the agent data folder..."
; Create the service, or update it on an upgrade
Filename: "{sys}\sc.exe"; Parameters: "create {#SvcName} start= auto binPath= ""\""{app}\drra_svc.exe\"""" DisplayName= ""{#AppTitle}"""; Flags: runhidden; Check: not ServiceExists; StatusMsg: "Installing the service..."
Filename: "{sys}\sc.exe"; Parameters: "config {#SvcName} start= auto binPath= ""\""{app}\drra_svc.exe\"""" DisplayName= ""{#AppTitle}"""; Flags: runhidden; Check: ServiceExists
Filename: "{sys}\sc.exe"; Parameters: "description {#SvcName} ""DataReel Reporting Agent for Windows: sends hardware, software, patch and Windows Defender reports to a report server"""; Flags: runhidden
; Restart the service if it stops unexpectedly
Filename: "{sys}\sc.exe"; Parameters: "failure {#SvcName} reset= 86400 actions= restart/60000/restart/60000/restart/300000"; Flags: runhidden
; The service is started in CurStepChanged(ssPostInstall), after the server
; settings are written

[UninstallRun]
Filename: "{sys}\net.exe"; Parameters: "stop {#SvcName}"; Flags: runhidden; RunOnceId: "StopService"
Filename: "{sys}\sc.exe"; Parameters: "delete {#SvcName}"; Flags: runhidden; RunOnceId: "DeleteService"

[Code]
var
  ServerPage: TInputQueryWizardPage;
  HttpsPage: TInputOptionWizardPage;
  PagesLoaded: Boolean;

function IniPath(Dir: String): String;
begin
  Result := AddBackslash(Dir) + 'drra.ini';
end;

// ------------------------------------- test builds before 3.3 (old names)

const
  MOVEFILE_DELAY_UNTIL_REBOOT = 4;

// MoveFileEx with no new name: delete the file or empty folder at reboot
function MoveFileExDelete(lpExistingFileName: String; lpNewFileName: Cardinal; dwFlags: Cardinal): Boolean;
  external 'MoveFileExW@kernel32.dll stdcall';

function OldAppDir: String;
begin
  Result := ExpandConstant('{commonpf64}\WindowsReportAgent');
end;

function OldDataDir: String;
begin
  Result := ExpandConstant('{commonappdata}\WindowsReportAgent');
end;

function OldIniPath: String;
begin
  Result := OldAppDir + '\windows_reporting_agent.ini';
end;

// Settings to show on the wizard pages: this install's, or the old one's
function SettingsIni(Dir: String): String;
begin
  Result := IniPath(Dir);
  if not FileExists(Result) and FileExists(OldIniPath) then
    Result := OldIniPath;
end;

function OldServiceExists: Boolean;
var
  RC: Integer;
begin
  Result := Exec(ExpandConstant('{sys}\sc.exe'), 'query {#OldSvcName}', '', SW_HIDE, ewWaitUntilTerminated, RC) and (RC = 0);
end;

// Copy an old INI file to the new folder with the old data paths changed
procedure MigrateIniFile(Src, Dest: String);
var
  Text: AnsiString;
  S: String;
begin
  if FileExists(Dest) or not LoadStringFromFile(Src, Text) then
    exit;
  S := String(Text);
  StringChangeEx(S, OldDataDir + '\windows_report_agent.log', ExpandConstant('{#DataDir}') + '\drra.log', True);
  StringChangeEx(S, OldDataDir, ExpandConstant('{#DataDir}'), True);
  StringChangeEx(S, '; Windows Reporting Agent settings', '; DataReel Reporting Agent for Windows: settings', True);
  // 45 minutes was the test builds' default, 3.3 reports every 15
  StringChangeEx(S, 'report_interval_minutes=45', 'report_interval_minutes=15', True);
  if SaveStringToFile(Dest, AnsiString(S), False) then
    Log('Moved settings ' + Src + ' to ' + Dest)
  else
    Log('Could not write ' + Dest);
end;

// Move a test build install: settings, agent data, then remove the old
// service, uninstall entry and folders. The machine key keeps its old
// name; the agent finds it.
procedure MigrateOldInstall;
var
  FindRec: TFindRec;
  AppDir, NewData, UninstallString: String;
  RC, I: Integer;
begin
  if not OldServiceExists and not DirExists(OldAppDir) and not DirExists(OldDataDir) then
    exit;
  Log('Found a Windows Reporting Agent test build install, moving it');
  AppDir := ExpandConstant('{app}');
  NewData := ExpandConstant('{#DataDir}');

  if OldServiceExists then begin
    Exec(ExpandConstant('{sys}\net.exe'), 'stop {#OldSvcName}', '', SW_HIDE, ewWaitUntilTerminated, RC);
    Exec(ExpandConstant('{sys}\sc.exe'), 'delete {#OldSvcName}', '', SW_HIDE, ewWaitUntilTerminated, RC);
    Log('Removed the {#OldSvcName} service');
  end;

  // Settings: windows_reporting_agent.ini and any copies of it
  ForceDirectories(AppDir);
  if FindFirst(OldAppDir + '\windows_reporting_agent.ini*', FindRec) then begin
    try
      repeat
        MigrateIniFile(OldAppDir + '\' + FindRec.Name,
                       AppDir + '\drra.ini' + Copy(FindRec.Name, Length('windows_reporting_agent.ini') + 1, MaxInt));
      until not FindNext(FindRec);
    finally
      FindClose(FindRec);
    end;
  end;

  // Agent data: certificate, registration state, share password, keys.
  // The old report and log are not needed.
  ForceDirectories(NewData);
  if FindFirst(OldDataDir + '\*', FindRec) then begin
    try
      repeat
        if (FindRec.Attributes and FILE_ATTRIBUTE_DIRECTORY = 0) and
           (Pos('_windows_report_agent.txt', FindRec.Name) = 0) and
           (CompareText(FindRec.Name, 'windows_report_agent.log') <> 0) and
           not FileExists(NewData + '\' + FindRec.Name) then begin
          if FileCopy(OldDataDir + '\' + FindRec.Name, NewData + '\' + FindRec.Name, True) then
            Log('Moved ' + FindRec.Name + ' to ' + NewData);
        end;
      until not FindNext(FindRec);
    finally
      FindClose(FindRec);
    end;
  end;

  // Run the old uninstaller; it keeps its INI and data folder, which are
  // deleted below now that they are copied
  if RegQueryStringValue(HKLM, 'Software\Microsoft\Windows\CurrentVersion\Uninstall\{#OldAppId}_is1',
                         'UninstallString', UninstallString) then begin
    Exec(RemoveQuotes(UninstallString), '/VERYSILENT /SUPPRESSMSGBOXES /NORESTART', '', SW_HIDE, ewWaitUntilTerminated, RC);
    // The uninstaller runs a copy of itself, wait for it to finish
    for I := 1 to 60 do begin
      if not FileExists(OldAppDir + '\unins000.exe') then
        break;
      Sleep(500);
    end;
    Log('Old uninstaller returned ' + IntToStr(RC));
  end;
  RegDeleteKeyIncludingSubkeys(HKLM, 'SYSTEM\CurrentControlSet\Services\EventLog\Application\{#OldSvcName}');
  if DelTree(OldAppDir, True, True, True) then
    Log('Removed ' + OldAppDir);
  if DelTree(OldDataDir, True, True, True) then
    Log('Removed ' + OldDataDir);
end;

function ServiceExists: Boolean;
var
  RC: Integer;
begin
  Result := Exec(ExpandConstant('{sys}\sc.exe'), 'query {#SvcName}', '', SW_HIDE, ewWaitUntilTerminated, RC) and (RC = 0);
end;

// Web path of the folder that holds upload.php, from an existing INI file
function UrlPathFromIni(Ini: String): String;
var
  UploadPath: String;
  P: Integer;
begin
  UploadPath := GetIniString('https', 'upload_path', '/windows/upload/upload.php', Ini);
  P := Pos('/upload.php', UploadPath);
  if P > 0 then
    Result := Copy(UploadPath, 1, P - 1)
  else
    Result := UploadPath;
end;

// True if a switch such as /HTTP is on the command line
function HasSwitch(Name: String): Boolean;
var
  I: Integer;
begin
  Result := False;
  for I := 1 to ParamCount do
    if CompareText(ParamStr(I), Name) = 0 then
      Result := True;
end;

function ParamOrDefault(Name, Default: String): String;
begin
  Result := ExpandConstant('{param:' + Name + '|' + Default + '}');
end;

procedure InitializeWizard;
begin
  ServerPage := CreateInputQueryPage(wpSelectDir,
    'Report server', 'Where should this computer send its reports?',
    'Enter the web server that runs the DataReel Reporting Agent PHP pages ' +
    '(register.php and upload.php). You can change these later in ' +
    'drra.ini in the install folder.');
  ServerPage.Add('Server host name:', False);
  ServerPage.Add('Port:', False);
  ServerPage.Add('Web path of the upload pages folder:', False);
  ServerPage.Add('Enrollment token (blank = an admin approves this computer):', True);

  HttpsPage := CreateInputOptionPage(ServerPage.ID,
    'Connection security', 'How should the agent connect to the server?',
    'Use HTTPS for any real deployment. Plain HTTP is only for testing.',
    True, False);
  HttpsPage.Add('HTTPS (recommended)');
  HttpsPage.Add('HTTP, no encryption (test servers only)');
end;

// Fill the pages from the command line, an existing INI file, or the defaults
procedure LoadPages;
var
  Ini: String;
  UseHttps: Boolean;
begin
  Ini := SettingsIni(WizardDirValue);
  // A new /SERVER= on the command line means HTTPS unless /HTTP is given;
  // otherwise keep the setting of an existing install
  if ExpandConstant('{param:SERVER|}') <> '' then
    UseHttps := not HasSwitch('/HTTP')
  else
    UseHttps := (GetIniInt('https', 'use_https', 1, 0, 1, Ini) = 1) and not HasSwitch('/HTTP');
  ServerPage.Values[0] := ParamOrDefault('SERVER', GetIniString('https', 'host', 'reports.example.com', Ini));
  ServerPage.Values[1] := ParamOrDefault('PORT', GetIniString('https', 'port', '443', Ini));
  ServerPage.Values[2] := ParamOrDefault('URLPATH', UrlPathFromIni(Ini));
  ServerPage.Values[3] := ParamOrDefault('TOKEN', GetIniString('https', 'enrollment_token', '', Ini));
  if UseHttps then
    HttpsPage.SelectedValueIndex := 0
  else
    HttpsPage.SelectedValueIndex := 1;
  PagesLoaded := True;
end;

procedure CurPageChanged(CurPageID: Integer);
begin
  if (CurPageID = ServerPage.ID) and not PagesLoaded then
    LoadPages;
end;

function NextButtonClick(CurPageID: Integer): Boolean;
var
  Port: Integer;
begin
  Result := True;
  if CurPageID = ServerPage.ID then begin
    if Trim(ServerPage.Values[0]) = '' then begin
      MsgBox('Enter the server host name.', mbError, MB_OK);
      Result := False;
    end else begin
      Port := StrToIntDef(Trim(ServerPage.Values[1]), 0);
      if (Port < 1) or (Port > 65535) then begin
        MsgBox('Enter a port number from 1 to 65535.', mbError, MB_OK);
        Result := False;
      end;
    end;
  end;
end;

// Stop the running service so its files can be replaced on an upgrade
function PrepareToInstall(var NeedsRestart: Boolean): String;
var
  RC: Integer;
begin
  if ServiceExists then
    Exec(ExpandConstant('{sys}\net.exe'), 'stop {#SvcName}', '', SW_HIDE, ewWaitUntilTerminated, RC);
  MigrateOldInstall;
  Result := '';
end;

procedure CurStepChanged(CurStep: TSetupStep);
var
  Ini, UrlPath: String;
  RC: Integer;
begin
  if CurStep = ssPostInstall then begin
    // Silent installs never show the pages, so load them here
    if not PagesLoaded then
      LoadPages;
    Ini := IniPath(ExpandConstant('{app}'));
    UrlPath := Trim(ServerPage.Values[2]);
    while (Length(UrlPath) > 0) and (UrlPath[Length(UrlPath)] = '/') do
      UrlPath := Copy(UrlPath, 1, Length(UrlPath) - 1);
    if (Length(UrlPath) = 0) or (UrlPath[1] <> '/') then
      UrlPath := '/' + UrlPath;
    SetIniString('https', 'host', Trim(ServerPage.Values[0]), Ini);
    SetIniString('https', 'port', Trim(ServerPage.Values[1]), Ini);
    SetIniString('https', 'upload_path', UrlPath + '/upload.php', Ini);
    SetIniString('https', 'register_path', UrlPath + '/register.php', Ini);
    SetIniString('https', 'enrollment_token', Trim(ServerPage.Values[3]), Ini);
    if HttpsPage.SelectedValueIndex = 0 then
      SetIniString('https', 'use_https', '1', Ini)
    else
      SetIniString('https', 'use_https', '0', Ini);
    Log('Wrote server settings to ' + Ini);

    // The old uninstaller may still have been deleting itself when the
    // old folder was removed before the install, try again. If something
    // still has it open (an Explorer window), delete it at the next reboot.
    if DirExists(OldAppDir) then begin
      if DelTree(OldAppDir, True, True, True) then
        Log('Removed ' + OldAppDir)
      else if MoveFileExDelete(OldAppDir, 0, MOVEFILE_DELAY_UNTIL_REBOOT) then
        Log(OldAppDir + ' is in use, it will be deleted at the next reboot')
      else
        Log('Could not remove ' + OldAppDir);
    end;

    // Delivery methods and the file share, from the command line only
    if ExpandConstant('{param:INTERVAL|}') <> '' then
      SetIniString('agent', 'report_interval_minutes', ExpandConstant('{param:INTERVAL|}'), Ini);
    if ExpandConstant('{param:METHODS|}') <> '' then
      SetIniString('delivery', 'methods', ExpandConstant('{param:METHODS|}'), Ini);
    if ExpandConstant('{param:CIFSSHARE|}') <> '' then
      SetIniString('cifs', 'share', ExpandConstant('{param:CIFSSHARE|}'), Ini);
    if ExpandConstant('{param:CIFSUSER|}') <> '' then
      SetIniString('cifs', 'user', ExpandConstant('{param:CIFSUSER|}'), Ini);
    if ExpandConstant('{param:CIFSPASS|}') <> '' then begin
      Exec(ExpandConstant('{app}\drra_svc.exe'),
           'set-cifs-password "' + ExpandConstant('{param:CIFSPASS|}') + '"', '', SW_HIDE, ewWaitUntilTerminated, RC);
      Log('set-cifs-password returned ' + IntToStr(RC));
    end;

    // The [Run] entries that create or update the service have already run
    WizardForm.StatusLabel.Caption := 'Starting the service...';
    Exec(ExpandConstant('{sys}\net.exe'), 'start {#SvcName}', '', SW_HIDE, ewWaitUntilTerminated, RC);
    Log('net start {#SvcName} returned ' + IntToStr(RC));
  end;
end;
