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

// Ensure the library is linked
#pragma comment(lib, "Wininet.lib")
#pragma comment(lib, "wbemuuid.lib")
#pragma comment(lib, "advapi32.lib")
#pragma comment(lib, "wevtapi.lib")
#pragma comment(lib, "Ws2_32.lib")
#pragma comment(lib, "iphlpapi.lib")
#pragma comment(lib, "user32.lib")

#include "common.h"
#define SVCNAME TEXT("DataReelReportAgent")

SERVICE_STATUS          gSvcStatus; 
SERVICE_STATUS_HANDLE   gSvcStatusHandle; 
HANDLE                  ghSvcStopEvent = NULL;


void generate_boundary_seq_number(char *boundary, int length);
int upload_agent_report(const AgentConfig &cfg, std::string &output);
  
int main()
{
  std::string output;
  AgentConfig cfg;
  LoadAgentConfig(cfg, output);

  std::cout << "Sending single report\n";
  
  // Upload a single report 
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

  std::cout << output.c_str() << "\n";
  
  return 0;
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

  std::string report_data = "DataReel Reporting Agent report from host " + HOSTNAME + "\n";
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

  // Usage for trends (DRSM): CPU sampled over 2 seconds
  report_data += "PERFORMANCE:\n";
  get_performance(sbuf, 2000);
  report_data += sbuf;
  report_data += "END PERFORMANCE\n";
  report_data += "\n";

  report_data += "DISK USAGE:\n";
  get_disk_usage(sbuf);
  report_data += sbuf;
  report_data += "END DISK USAGE\n";
  report_data += "\n";

  report_data += "NETWORK:\n";
  get_network_stats(sbuf);
  report_data += sbuf;
  report_data += "END NETWORK\n";
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

				








