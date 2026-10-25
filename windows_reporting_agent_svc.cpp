// Windows includes that must come first
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <WinSock2.h> 

// C STD lib includes
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <tchar.h>

// Windows includes
#include <wininet.h>
#include <comdef.h>
#include <Wbemidl.h>
#include <strsafe.h>
#include <winevt.h>
#include <iphlpapi.h>
#include <ws2tcpip.h>

// C++ STD lib includes
#include <iostream>
#include <string>
#include <cctype>
#include <ctime>
#include <vector>

// APP includes
#include "windows_reporting_agent.h"

// Ensure the library is linked
#pragma comment(lib, "Wininet.lib")
#pragma comment(lib, "wbemuuid.lib")
#pragma comment(lib, "advapi32.lib")
#pragma comment(lib, "wevtapi.lib")
#pragma comment(lib, "Ws2_32.lib")
#pragma comment(lib, "iphlpapi.lib")
#pragma comment(lib, "user32.lib")

#include "common.h"
// Windows Update Agent (WUA) API
#include <wuapi.h>
#include <comutil.h>
#pragma comment(lib, "Ole32.lib")
#pragma comment(lib, "OleAut32.lib")
#pragma comment(lib, "comsuppw.lib")

#define SVCNAME TEXT("WindowsReportAgent")

SERVICE_STATUS          gSvcStatus; 
SERVICE_STATUS_HANDLE   gSvcStatusHandle; 
HANDLE                  ghSvcStopEvent = NULL;


void generate_boundary_seq_number(char *boundary, int length);
int upload_agent_report(const AgentConfig &cfg, std::string &output);
int get_all_patches(std::string &output);

VOID SvcInstall(void);
VOID WINAPI SvcCtrlHandler( DWORD ); 
VOID WINAPI SvcMain( DWORD, LPTSTR * ); 
VOID ReportSvcStatus( DWORD, DWORD, DWORD );
VOID SvcInit( DWORD, LPTSTR * ); 
VOID SvcReportEvent( LPTSTR );

//
// Purpose: 
//   Entry point for the process
//
// Parameters:
//   None
// 
// Return value:
//   None, defaults to 0 (zero)
//
int __cdecl _tmain(int argc, TCHAR *argv[]) 
{ 
  // If command-line parameter is "install", install the service. 
  // Otherwise, the service is probably being started by the SCM.
  if( lstrcmpi( argv[1], TEXT("install")) == 0 )
    {
      SvcInstall();
      return 0;
    }
  
  // TO_DO: Add any additional services for the process to this table.
  SERVICE_TABLE_ENTRY DispatchTable[] = 
    { 
      { SVCNAME, (LPSERVICE_MAIN_FUNCTION) SvcMain }, 
      { NULL, NULL } 
    }; 
  
  // This call returns when the service has stopped. 
  // The process should simply terminate when the call returns.
  
  if (!StartServiceCtrlDispatcher( DispatchTable )) 
    { 
      SvcReportEvent(TEXT("StartServiceCtrlDispatcher")); 
    } 
} 

//
// Purpose: 
//   Installs a service in the SCM database
//
// Parameters:
//   None
// 
// Return value:
//   None
//
VOID SvcInstall()
{
  SC_HANDLE schSCManager;
  SC_HANDLE schService;
  TCHAR szUnquotedPath[MAX_PATH];
  
  if( !GetModuleFileName( NULL, szUnquotedPath, MAX_PATH ) )
    {
      printf("Cannot install service (%d)\n", GetLastError());
      return;
    }
  
  // In case the path contains a space, it must be quoted so that
  // it is correctly interpreted. For example,
  // "d:\my share\myservice.exe" should be specified as
  // ""d:\my share\myservice.exe"".
  TCHAR szPath[MAX_PATH];
  StringCbPrintf(szPath, MAX_PATH, TEXT("\"%s\""), szUnquotedPath);
  
  // Get a handle to the SCM database. 
  
  schSCManager = OpenSCManager( 
			       NULL,                    // local computer
			       NULL,                    // ServicesActive database 
			       SC_MANAGER_ALL_ACCESS);  // full access rights 
  
  if (NULL == schSCManager) 
    {
      printf("OpenSCManager failed (%d)\n", GetLastError());
      return;
    }
  
  // Create the service
  
  schService = CreateService( 
			     schSCManager,              // SCM database 
			     SVCNAME,                   // name of service 
			     SVCNAME,                   // service name to display 
			     SERVICE_ALL_ACCESS,        // desired access 
			     SERVICE_WIN32_OWN_PROCESS, // service type 
			     SERVICE_DEMAND_START,      // start type 
			     SERVICE_ERROR_NORMAL,      // error control type 
			     szPath,                    // path to service's binary 
			     NULL,                      // no load ordering group 
			     NULL,                      // no tag identifier 
			     NULL,                      // no dependencies 
			     NULL,                      // LocalSystem account 
			     NULL);                     // no password 
  
  if (schService == NULL) 
    {
      printf("CreateService failed (%d)\n", GetLastError()); 
      CloseServiceHandle(schSCManager);
      return;
    }
  else printf("Service installed successfully\n"); 
  
  CloseServiceHandle(schService); 
  CloseServiceHandle(schSCManager);
}

//
// Purpose: 
//   Entry point for the service
//
// Parameters:
//   dwArgc - Number of arguments in the lpszArgv array
//   lpszArgv - Array of strings. The first string is the name of
//     the service and subsequent strings are passed by the process
//     that called the StartService function to start the service.
// 
// Return value:
//   None.
//
VOID WINAPI SvcMain( DWORD dwArgc, LPTSTR *lpszArgv )
{
  // Register the handler function for the service
  
  gSvcStatusHandle = RegisterServiceCtrlHandler( 
						SVCNAME, 
						SvcCtrlHandler);
  
  if( !gSvcStatusHandle )
    { 
      SvcReportEvent(TEXT("RegisterServiceCtrlHandler")); 
      return; 
    } 
  
  // These SERVICE_STATUS members remain as set here
  
  gSvcStatus.dwServiceType = SERVICE_WIN32_OWN_PROCESS; 
  gSvcStatus.dwServiceSpecificExitCode = 0;    
  
  // Report initial status to the SCM
  
  ReportSvcStatus( SERVICE_START_PENDING, NO_ERROR, 3000 );
  
  // Perform service-specific initialization and work.
  
  SvcInit( dwArgc, lpszArgv );
}

//
// Purpose: 
//   The service code
//
// Parameters:
//   dwArgc - Number of arguments in the lpszArgv array
//   lpszArgv - Array of strings. The first string is the name of
//     the service and subsequent strings are passed by the process
//     that called the StartService function to start the service.
// 
// Return value:
//   None
//
VOID SvcInit( DWORD dwArgc, LPTSTR *lpszArgv)
{
  // TO_DO: Declare and set any required variables.
  //   Be sure to periodically call ReportSvcStatus() with 
  //   SERVICE_START_PENDING. If initialization fails, call
  //   ReportSvcStatus with SERVICE_STOPPED.
  
  // Create an event. The control handler function, SvcCtrlHandler,
  // signals this event when it receives the stop control code.
  
  ghSvcStopEvent = CreateEvent(
			       NULL,    // default security attributes
			       TRUE,    // manual reset event
			       FALSE,   // not signaled
			       NULL);   // no name
  
  if ( ghSvcStopEvent == NULL)
    {
      ReportSvcStatus( SERVICE_STOPPED, GetLastError(), 0 );
      return;
    }
  
  // Report running status when initialization is complete.
  
  ReportSvcStatus( SERVICE_RUNNING, NO_ERROR, 0 );
  int i = 0;
    //  Periodically check if the service has been requested to stop
    while (WaitForSingleObject(ghSvcStopEvent, 0) != WAIT_OBJECT_0) {        
      /* 
       * Perform main service function here
       */

      // Reload the settings each cycle so INI file edits take effect
      // without restarting the service
      std::string output;
      AgentConfig cfg;
      LoadAgentConfig(cfg, output);

      // Upload the report every report_interval_minutes
      std::string report_output;
      upload_agent_report(cfg, report_output);
      output += report_output;
      std::string logfile_name = cfg.log_file;
      
      HANDLE hFile = CreateFile(
				logfile_name.c_str(),
				GENERIC_WRITE,           // Desired access
				0,                       // Share mode
				NULL,                    // Security attributes
				CREATE_ALWAYS,           // Creation disposition
				FILE_ATTRIBUTE_NORMAL,   // Flags and attributes
				NULL                     // Template file handle
				);
      
      if (hFile == INVALID_HANDLE_VALUE) {
	// Could not create log file
      }
      else {
	  DWORD bytesWritten;
	  BOOL result = WriteFile(
				  hFile,                       // Handle to file
				  output.c_str(),         // Data to write
				  (DWORD)output.length(), // Number of bytes to write
				  &bytesWritten,               // Stores number of bytes written
				  NULL                         // Overlapped buffer
				  );

	  if(!result) {
	    // could not write to log file
	  }
	  
	  
	  CloseHandle(hFile);
      }

      // Sleep for report_interval_minutes, checking every 5 seconds to see if the serivce recieved a request to stop
      // 1 minute = 12 checks of 5000 ms
      int wait_checks = cfg.report_interval_minutes * 12;
      for(i = 0; i < wait_checks; i++) {
	// Use a timed wait instead of Sleep(5000)
        // This ensures the thread stops IMMEDIATELY when requested
        if (WaitForSingleObject(ghSvcStopEvent, 5000) == WAIT_OBJECT_0) {
	  ReportSvcStatus( SERVICE_STOPPED, NO_ERROR, 0 );
	  break; 
        }
      }
    }
  
    ReportSvcStatus( SERVICE_STOPPED, NO_ERROR, 0 );
}

//
// Purpose: 
//   Sets the current service status and reports it to the SCM.
//
// Parameters:
//   dwCurrentState - The current state (see SERVICE_STATUS)
//   dwWin32ExitCode - The system error code
//   dwWaitHint - Estimated time for pending operation, 
//     in milliseconds
// 
// Return value:
//   None
//
VOID ReportSvcStatus( DWORD dwCurrentState,
                      DWORD dwWin32ExitCode,
                      DWORD dwWaitHint)
{
  static DWORD dwCheckPoint = 1;
  
  // Fill in the SERVICE_STATUS structure.
  
  gSvcStatus.dwCurrentState = dwCurrentState;
  gSvcStatus.dwWin32ExitCode = dwWin32ExitCode;
  gSvcStatus.dwWaitHint = dwWaitHint;
  
  if (dwCurrentState == SERVICE_START_PENDING)
    gSvcStatus.dwControlsAccepted = 0;
  else gSvcStatus.dwControlsAccepted = SERVICE_ACCEPT_STOP;
  
  if ( (dwCurrentState == SERVICE_RUNNING) ||
       (dwCurrentState == SERVICE_STOPPED) )
    gSvcStatus.dwCheckPoint = 0;
  else gSvcStatus.dwCheckPoint = dwCheckPoint++;
  
  // Report the status of the service to the SCM.
  SetServiceStatus( gSvcStatusHandle, &gSvcStatus );
}

//
// Purpose: 
//   Called by SCM whenever a control code is sent to the service
//   using the ControlService function.
//
// Parameters:
//   dwCtrl - control code
// 
// Return value:
//   None
//
VOID WINAPI SvcCtrlHandler( DWORD dwCtrl )
{
  // Handle the requested control code. 
  
  switch(dwCtrl) 
    {  
      case SERVICE_CONTROL_STOP: 
	ReportSvcStatus(SERVICE_STOP_PENDING, NO_ERROR, 0);
	
	// Signal the service to stop.
	
	SetEvent(ghSvcStopEvent);
	ReportSvcStatus(gSvcStatus.dwCurrentState, NO_ERROR, 0);
        
	return;
	
      case SERVICE_CONTROL_INTERROGATE: 
	break; 
	
      default: 
	break;
    } 
  
}

//
// Purpose: 
//   Logs messages to the event log
//
// Parameters:
//   szFunction - name of function that failed
// 
// Return value:
//   None
//
// Remarks:
//   The service must have an entry in the Application event log.
//
VOID SvcReportEvent(LPTSTR szFunction) 
{ 
  HANDLE hEventSource;
  LPCTSTR lpszStrings[2];
  TCHAR Buffer[80];
  
  hEventSource = RegisterEventSource(NULL, SVCNAME);
  
  if( NULL != hEventSource )
    {
      StringCchPrintf(Buffer, 80, TEXT("%s failed with %d"), szFunction, GetLastError());
      
      lpszStrings[0] = SVCNAME;
      lpszStrings[1] = Buffer;
      
      ReportEvent(hEventSource,        // event log handle
		  EVENTLOG_ERROR_TYPE, // event type
		  0,                   // event category
		  SVC_ERROR,           // event identifier
		  NULL,                // no security identifier
		  2,                   // size of lpszStrings array
		  0,                   // no binary data
		  lpszStrings,         // array of strings
		  NULL);               // no binary data
      
      DeregisterEventSource(hEventSource);
    }
}



// Function to generate a random boundary



// Error-handling helper function


int upload_agent_report(const AgentConfig &cfg, std::string &output) {

  std::string input_fname, upload_fname, outbuf;
  std::string output_str;
  std::string HOSTNAME;
  std::string DIR;
    
  TCHAR buffer[MAX_COMPUTERNAME_LENGTH + 1];
  DWORD size = MAX_COMPUTERNAME_LENGTH + 1;
  
  if(!GetComputerName(buffer, &size)) {
    output += "ERROR - Error getting computer name\n";
    return 1;
  }
  HOSTNAME = buffer;
  for (char &c : HOSTNAME) {
    c = std::tolower((unsigned char)c);
  }

  upload_fname = HOSTNAME + "_windows_report_agent.txt";
  input_fname = JoinPath(cfg.report_dir, upload_fname);
  
  HANDLE hFile = CreateFile(
			    input_fname.c_str(),
			    GENERIC_WRITE,           // Desired access
			    0,                       // Share mode
			    NULL,                    // Security attributes
			    CREATE_ALWAYS,           // Creation disposition
			    FILE_ATTRIBUTE_NORMAL,   // Flags and attributes
			    NULL                     // Template file handle
			    );

  if (hFile == INVALID_HANDLE_VALUE) {
    output += "ERROR - Could not open file " + input_fname;
    output += "\n";
    return 1;
  }

  std::string report_data = "Windows report agent from host " + HOSTNAME + "\n";
  report_data += "VERSION: " AGENT_VERSION "\n";
  report_data += "HOSTNAME: " + HOSTNAME + "\n";
  std::time_t time_result = std::time(nullptr);
  std::string EPTIME = std::to_string(time_result);
  report_data += "ETIME: " + EPTIME + "\n";
  
  time_t now = time(0);
  char tbuf[26];
  ctime_s(tbuf, sizeof(tbuf), &now);
  std::string DATETIME = tbuf;
  report_data += "DATETIME: " +  DATETIME + "\n";

  std::string sbuf;

  report_data += "\n";
  report_data += "SERVICE TAG AND BARCODE:\n";
  get_dell_service_tag(sbuf);
  report_data += sbuf;
  report_data += "END SERVICE TAG AND BARCODE\n";
  report_data += "\n";

  report_data += "\n";
  report_data += "IP ADDRESS:\n";
  GetIPAddress(sbuf);
  report_data += sbuf;
  report_data += "END IP ADDRESS\n";
  report_data += "\n";
  
  DIR.clear();
  DIR = JoinPath(cfg.users_dir, "*"); // Note the trailing '*' wildcard
  list_dirs(DIR, sbuf);
  report_data += "USERS:\n";
  report_data += sbuf;
  report_data += "END USERS\n";
  report_data += "\n";

  report_data += "ALL INSTALLED PROGRAMS:\n";
  get_all_apps(sbuf);
  report_data += sbuf;
  report_data += "END ALL INSTALLED PROGRAMS\n";
  report_data += "\n";
  
  // Windows defender alerts
  report_data += "WINDOWS DEFENDER ALERTS:\n";
  get_windows_defender_alerts(sbuf, cfg.event_lookback_days);
  report_data += sbuf;
  report_data += "END WINDOWS DEFENDER ALERTS\n";
  report_data += "\n";

  report_data += "\n";

  report_data += "INSTALLED WINDOWS PATCHES:\n";
  get_all_patches(sbuf);
  report_data += sbuf;
  report_data += "END INSTALLED WINDOWS PATCHES\n";

  report_data += "\n";
  
  // Hotfixes
  report_data += "HOT FIXES:\n";
  get_hotfixes(sbuf);
  report_data += sbuf;
  report_data += "END HOT FIXES\n";

  report_data += "\n";
  
  DWORD bytesWritten;
  BOOL result = WriteFile(
			  hFile,                       // Handle to file
			  report_data.c_str(),         // Data to write
			  (DWORD)report_data.length(), // Number of bytes to write
			  &bytesWritten,               // Stores number of bytes written
			  NULL                         // Overlapped buffer
			  );
  
  if(!result) {
    output += "ERROR - could not write to file " + input_fname;
    output += "\n";
    CloseHandle(hFile);
    return 1;
  }

  CloseHandle(hFile);
  
  int error_level = DeliverReport(cfg, input_fname, upload_fname, output_str);

  if(error_level != 0) {
    output += "ERROR - One or more report delivery methods failed\n";
  }
  output += output_str += "\n";
  
  return 0;
}

				




int get_all_patches(std::string &output)
{
  int error_level = 0;
  output.clear();

  // Initialize the COM library
  HRESULT hr = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
  if (FAILED(hr)) {
    output += "ERROR - Failed to initialize COM library. Error code: ";
    output += std::to_string(hr);
    output += "\n";
    return 1;
  }
  
  // Create an instance of the UpdateSession
  IUpdateSession* pSession = nullptr;
  hr = CoCreateInstance(CLSID_UpdateSession, NULL, CLSCTX_INPROC_SERVER, IID_IUpdateSession, (LPVOID*)&pSession);
  
  if (SUCCEEDED(hr)) {
    // Create an UpdateSearcher
    IUpdateSearcher* pSearcher = nullptr;
    hr = pSession->CreateUpdateSearcher(&pSearcher);
    
    if (SUCCEEDED(hr)) {
      // Query the total number of history events available
      LONG totalHistoryCount = 0;
      hr = pSearcher->GetTotalHistoryCount(&totalHistoryCount);
      
      if (SUCCEEDED(hr) && totalHistoryCount > 0) {
	// Retrieve the history entries (from index 0 to totalHistoryCount)
	IUpdateHistoryEntryCollection* pHistoryCollection = nullptr;
	hr = pSearcher->QueryHistory(0, totalHistoryCount, &pHistoryCollection);
	
	if (SUCCEEDED(hr)) {
	  LONG historyCount = 0;
	  pHistoryCollection->get_Count(&historyCount);
	  
	  for (LONG i = 0; i < historyCount; ++i) {
	    IUpdateHistoryEntry* pEntry = nullptr;
	    hr = pHistoryCollection->get_Item(i, &pEntry);
	    
	    if (SUCCEEDED(hr)) {
	      BSTR title;
	      BSTR supportUrl;
	      DATE date;
	      UpdateOperation operation;
	      
	      // Fetch patch details
	      pEntry->get_Title(&title);
	      pEntry->get_Date(&date);
	      pEntry->get_Operation(&operation);
	      
	      // We only want to display actual installation events
	      if (operation == uoInstallation) {
		// Convert COM DATE variant to readable system time
		SYSTEMTIME st;
		VariantTimeToSystemTime(date, &st);

		output += "Date:  ";
		output += std::to_string(st.wYear);
		output += "-";
		output += std::to_string(st.wMonth);
		output += "-";
		output += std::to_string(st.wDay);
		output += "\n";

		output += " Patch: ";
		char* charPtr = _com_util::ConvertBSTRToString(title);
		output += charPtr;
		delete[] charPtr; 
		output += "\n";

		output +=  "\n";
	      }
	      
	      // Free COM strings and objects
	      SysFreeString(title);
	      pEntry->Release();
	    }
	  }
	  pHistoryCollection->Release();
	}
      } else {
	output += "No patch history found.\n";
      }
      pSearcher->Release();
    }
    pSession->Release();
  } else {
    output += "ERROR - Failed to create IUpdateSession. Error code: ";
    output += std::to_string(hr);
    output += "\n";
  }
  
  // Uninitialize COM library
  CoUninitialize();
  
  return error_level;
}








