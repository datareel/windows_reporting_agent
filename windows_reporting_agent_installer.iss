[Setup]
AppName=Windows Reporting Agent
AppVersion=3.2
DefaultDirName={autopf}\WindowsReportAgent
DefaultGroupName=Windows Reporting Agent
; Administrative privileges are required to manage services
PrivilegesRequired=admin
OutputBaseFilename=windows_reporting_agent_setup
Compression=lzma2
SolidCompression=yes
OutputDir=.

[Files]
Source: "svcconfig.exe"; DestDir: "{app}"; Flags: ignoreversion
Source: "send_test_report.exe"; DestDir: "{app}"; Flags: ignoreversion
Source: "svccontrol.exe"; DestDir: "{app}"; Flags: ignoreversion
Source: "windows_reporting_agent_svc.exe"; DestDir: "{app}"; Flags: ignoreversion
Source: "windows_reporting_agent.dll"; DestDir: "{app}"; Flags: ignoreversion
Source: "list_all_apps.exe"; DestDir: "{app}"; Flags: ignoreversion
Source: "list_all_patches.exe"; DestDir: "{app}"; Flags: ignoreversion
Source: "list_defender_alerts.exe"; DestDir: "{app}"; Flags: ignoreversion
Source: "get_ip_address.exe"; DestDir: "{app}"; Flags: ignoreversion
Source: "get_service_tag.exe"; DestDir: "{app}"; Flags: ignoreversion

[Run]
; Install the service using sc.exe
Filename: "{sys}\sc.exe"; Parameters: "create WindowsReportAgent start= auto binPath= ""{app}\windows_reporting_agent_svc.exe"" DisplayName= ""Windows Report Agent"""; Flags: runhidden
; Start the service immediately after installation
Filename: "{sys}\sc.exe"; Parameters: "start WindowsReportAgent"; Flags: runhidden

[UninstallRun]
; Stop the service before removing it
Filename: "{sys}\sc.exe"; Parameters: "stop WindowsReportAgent"; Flags: runhidden
; Delete the service registration
Filename: "{sys}\sc.exe"; Parameters: "delete WindowsReportAgent"; Flags: runhidden

[Registry]
Root: HKLM; Subkey: "SYSTEM\CurrentControlSet\Services\EventLog\Application\WindowsReportAgent"; ValueType: string; ValueName: "EventMessageFile"; ValueData: "{app}\windows_reporting_agent.dll"; Flags: uninsdeletekey
Root: HKLM; Subkey: "SYSTEM\CurrentControlSet\Services\EventLog\Application\WindowsReportAgent"; ValueType: dword; ValueName: "TypesSupported"; ValueData: "0x00000007"; Flags: uninsdeletekey
