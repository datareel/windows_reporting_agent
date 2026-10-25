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

// Ensure the library is linked
#pragma comment(lib, "Wininet.lib")
#pragma comment(lib, "wbemuuid.lib")
#pragma comment(lib, "advapi32.lib")
#pragma comment(lib, "wevtapi.lib")
#pragma comment(lib, "Ws2_32.lib")
#pragma comment(lib, "iphlpapi.lib")
#pragma comment(lib, "user32.lib")

// Windows Update Agent (WUA) API
#include <wuapi.h>
#include <comutil.h>
#pragma comment(lib, "Ole32.lib")
#pragma comment(lib, "OleAut32.lib")

// Windows Update Agent (WUA) API
#include <wuapi.h>
#include <comutil.h>
#pragma comment(lib, "Ole32.lib")
#pragma comment(lib, "OleAut32.lib")
#pragma comment(lib, "comsuppw.lib")

#include "common.h"

std::string GetLastErrorAsString()
{
  // Get the error message ID, if any.
  DWORD errorMessageID = ::GetLastError();
  if (errorMessageID == 0) {
    return std::string(); // No error message has been recorded
  }
  
  LPSTR messageBuffer = nullptr;
  
  // Ask Win32 to give us the string representation of that message ID.
  // The parameters we pass tell FormatMessage to allocate the buffer.
  size_t size = FormatMessageA(
			       FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
			       NULL, 
			       errorMessageID, 
			       MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), 
			       (LPSTR)&messageBuffer, 
			       0, 
			       NULL
			       );
  
  // Copy the error message into a std::string.
  std::string message(messageBuffer, size);
  
  // Free the Win32 allocated buffer.
  LocalFree(messageBuffer);
  
  return message;
}

int list_dirs(const std::string directory, std::string &output)
{
  output.clear();
  
  WIN32_FIND_DATAA findData;
  HANDLE hFind = INVALID_HANDLE_VALUE;
  // directory = "C:\\Users\\*"; // Note the trailing '*' wildcard
  
  hFind = FindFirstFileA(directory.c_str(), &findData);
  
  if (hFind == INVALID_HANDLE_VALUE) {
    output = "ERROR - " + directory + " DIR not found or access denied.\n";
    return 1;
  }
  
  do {
    // Skip the current (.) and parent (..) directory entries
    if (strcmp(findData.cFileName, ".") != 0 && strcmp(findData.cFileName, "..") != 0) {
      if (findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
	output += "[DIR]  ";
	output += findData.cFileName;
	output += "\n";
      } else {
	output += "[FILE] ";
	output += findData.cFileName;
	output += "\n";
      }
    }
  } while (FindNextFileA(hFind, &findData) != 0);
  
  FindClose(hFind);
  
  return 0;
}

void generate_boundary_seq_number(char *boundary, int length)
{
  srand(time(NULL));
    const char charset[] = "abcdefghijklmnopqrstuvwxyz"
                           "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
    memset(boundary, 0, length);
    for (int i = 0; i < 22; i++) {
        boundary[i] = charset[rand() % (sizeof(charset) - 1)];
    }
}

int UploadFile(const std::string& server, const std::string& path, const std::string& input_fname, const std::string& upload_fname, std::string& output_str)
{
  int error_level = 0;
  
  // 1. Initialize WinINet
  HINTERNET hSession = InternetOpen("WindowsReportAgent/3.2", INTERNET_OPEN_TYPE_DIRECT, NULL, NULL, 0);
  if (!hSession) {
    output_str = "ERROR - Could not initialize WinINet session"; 
    return -1;
  }
  
  // 2. Connect to the server
  HINTERNET hConnect = InternetConnect(hSession, server.c_str(), INTERNET_DEFAULT_HTTPS_PORT, NULL, NULL, INTERNET_SERVICE_HTTP, 0, 0);
  if (!hConnect) {
    InternetCloseHandle(hSession);
    output_str = "ERROR - Could not connect to server " + server; 
    return -1;
  }
  
  // 3. Create the request
  const char* acceptTypes[] = { "*/*", NULL };
  HINTERNET hRequest = HttpOpenRequest(hConnect, "POST", path.c_str(), TEXT("HTTP/1.1"), NULL, acceptTypes, INTERNET_FLAG_SECURE, 0);

  // 06/04/2026: Fix for error code: 12057 cannot validate the SSL/TLS certificate's revocation status.
  // This is usually caused by network security settings or blocked access to a Certificate Revocation List (CRL) server.
  if (hRequest) {
    DWORD dwFlags;
    DWORD dwBuffLen = sizeof(dwFlags);
    
    // 1. Retrieve the current security flags
    InternetQueryOption(hRequest, INTERNET_OPTION_SECURITY_FLAGS, &dwFlags, &dwBuffLen);
    
    // 2. Add the flag to ignore revocation failures
    dwFlags |= SECURITY_FLAG_IGNORE_REVOCATION;
    
    // 3. Set the updated security flags back to the request handle
    InternetSetOption(hRequest, INTERNET_OPTION_SECURITY_FLAGS, &dwFlags, sizeof(dwFlags));
    
    // Now call HttpSendRequest(hRequest, ...);
  }
  
  // 4. Construct Multipart/Form-Data body
  char boundary[32];
  generate_boundary_seq_number(boundary, 31);
  std::string boundary_seq = boundary;
  std::string boundary_sep = "------------------------";
  std::string headers = "Content-Type: multipart/form-data; boundary=" + boundary_sep + boundary_seq;
  
  // Form data headers for the file
  std::string bodyStart = "--" + boundary_sep + boundary_seq + "\r\n"
    "Content-Disposition: form-data; name=\"uploadedFile\"; filename=\"" + upload_fname + "\"\r\n"
    "Content-Type: text/plain\r\n\r\n";
  
  // This is the content of the file being uploaded
  HANDLE hfile;
  std::string fileContent = "No file content\r\n";
  hfile=CreateFile(input_fname.c_str(),GENERIC_READ,FILE_SHARE_READ,NULL,OPEN_EXISTING,0,NULL);
  if(hfile != INVALID_HANDLE_VALUE){
    DWORD filesize, bufsize, bytesread;
    filesize=GetFileSize(hfile,NULL);
    bufsize = filesize + 32;
    LPBYTE pbuf=(LPBYTE)malloc(bufsize);
    memset(pbuf, 0, bufsize); 
    ReadFile(hfile,pbuf,filesize,&bytesread,NULL);
    fileContent = (char *)pbuf;
    CloseHandle(hfile);
    free(pbuf);
  }
  else {
    error_level = -1;
    std::string fileContent = "ERROR - Cannot open " + input_fname + "\r\n";
    output_str = "ERROR - - Cannot open " + input_fname;
  }
  
  std::string submit = "\r\n--" + boundary_sep + boundary_seq + "\r\n"
    "Content-Disposition: form-data; name=\"submit\"\r\n\r\nUpload File";
  
  std::string bodyEnd = "\r\n--" + boundary_sep + boundary_seq + "--\r\n";
  
  // Combine into final data buffer
  std::string fullBody = bodyStart + fileContent + submit + bodyEnd;
  
  // 5. Send the request
  BOOL sent = HttpSendRequest(hRequest, headers.c_str(), headers.length(), (LPVOID)fullBody.c_str(), fullBody.length());
  
  if (sent) {
    output_str = "Upload successful\r\n";
    char buffer[1024];
    DWORD bytesRead;
    while (InternetReadFile(hRequest, buffer, sizeof(buffer) - 1, &bytesRead) && bytesRead > 0) {
      buffer[bytesRead] = '\0';
      output_str += buffer;
    }
    
  } else {
    output_str = "ERROR - Upload failed. Error message: " + GetLastErrorAsString();
    error_level = -1;
  }
  
  // Clean up
  InternetCloseHandle(hRequest);
  InternetCloseHandle(hConnect);
  InternetCloseHandle(hSession);

  return error_level;
}

std::string WStringToString(const std::wstring& wstr)
{
  if (wstr.empty()) return std::string();
  
  // 1. Get required buffer size
  int size_needed = WideCharToMultiByte(CP_UTF8, 0, &wstr[0], (int)wstr.size(), NULL, 0, NULL, NULL);
  
  // 2. Convert
  std::string strTo(size_needed, 0);
  WideCharToMultiByte(CP_UTF8, 0, &wstr[0], (int)wstr.size(), &strTo[0], size_needed, NULL, NULL);
  return strTo;
}

void CheckHResult(HRESULT hr, const char* message, std::string &output)
{
  if (FAILED(hr)) {
    // std::cerr << "ERROR - " << message << " (0x" << std::hex << hr << ")" << std::endl;
    output += "ERROR - ";
    output += message;
    output += "\n";
    throw std::runtime_error("WMI operation failed.");
    output += "ERROR - WMI operation failed\n";
  }
}

int get_hotfixes(std::string &output)
{
  HRESULT hres;
  output.clear();
  
  // 1. Initialize COM
  hres = CoInitializeEx(0, COINIT_MULTITHREADED);
  if (FAILED(hres)) {
    output += "ERROR - Failed to initialize COM library.\n";
    return 1;
  }
  
  // 2. Initialize COM security
  hres = CoInitializeSecurity(
			      NULL,
			      -1,
			      NULL,
			      NULL,
			      RPC_C_AUTHN_LEVEL_PKT_PRIVACY,
			      RPC_C_IMP_LEVEL_IMPERSONATE,
			      NULL,
			      EOAC_NONE,
			      NULL
			      );
    if (FAILED(hres) && hres != RPC_E_TOO_LATE) {
      output += "Failed to initialize COM security.\n";
      CoUninitialize();
      return 1;
    }
    
    IWbemLocator* pLoc = NULL;
    IWbemServices* pSvc = NULL;
    
    try {
      // 3. Obtain the WMI locator
      hres = CoCreateInstance(
			      CLSID_WbemLocator,
			      0,
			      CLSCTX_INPROC_SERVER,
			      IID_IWbemLocator,
			      (LPVOID*)&pLoc
			      );
      CheckHResult(hres, "Failed to create IWbemLocator object.", output);
      
      // 4. Connect to the WMI namespace
      hres = pLoc->ConnectServer(
				 _bstr_t(L"ROOT\\CIMV2"),
				 NULL,
				 NULL,
				 0,
				 NULL,
				 0,
				 0,
				 &pSvc
				 );
      CheckHResult(hres, "Could not connect to WMI server.", output);

      // 5. Execute the WMI query
      IEnumWbemClassObject* pEnumerator = NULL;
      hres = pSvc->ExecQuery(
			     _bstr_t(L"WQL"),
			     _bstr_t(L"SELECT * FROM Win32_QuickFixEngineering"),
			     WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY,
			     NULL,
			     &pEnumerator
			     );
      CheckHResult(hres, "Query for Win32_QuickFixEngineering failed.", output);
      
      // 6. Enumerate the results
      IWbemClassObject* pclsObj = NULL;
      ULONG uReturn = 0;
      std::vector<HotfixInfo> hotfixes;
      
      while (pEnumerator) {
	hres = pEnumerator->Next(WBEM_INFINITE, 1, &pclsObj, &uReturn);
	if (uReturn == 0) break;
	
	HotfixInfo info;
	VARIANT vtProp;
	
	// Get HotFixID
	hres = pclsObj->Get(L"HotFixID", 0, &vtProp, 0, 0);
	if (SUCCEEDED(hres) && vtProp.vt != VT_NULL) {
	  info.hotfixID = vtProp.bstrVal;
	}
	VariantClear(&vtProp);
	
	// Get Description
            hres = pclsObj->Get(L"Description", 0, &vtProp, 0, 0);
            if (SUCCEEDED(hres) && vtProp.vt != VT_NULL) {
	      info.description = vtProp.bstrVal;
            }
            VariantClear(&vtProp);
	    
            // Get InstalledBy
            hres = pclsObj->Get(L"InstalledBy", 0, &vtProp, 0, 0);
            if (SUCCEEDED(hres) && vtProp.vt != VT_NULL) {
	      info.installedBy = vtProp.bstrVal;
            }
            VariantClear(&vtProp);
	    
            // Get InstallDate
            hres = pclsObj->Get(L"InstalledOn", 0, &vtProp, 0, 0);
            if (SUCCEEDED(hres) && vtProp.vt != VT_NULL) {
	      info.installedOn = vtProp.bstrVal;
            }
            VariantClear(&vtProp);
	    
            hotfixes.push_back(info);
            pclsObj->Release();
      }
      
      // 7. Print the list of hotfixes
      output += "Installed Windows Hotfixes:\n";
      output += "--------------------------\n";
        for (const auto& hotfix : hotfixes) {
	  output += "Hotfix ID:      ";
	  output +=  WStringToString(hotfix.hotfixID);
	  output += "\n";
	  
	  output += "Description:    ";
	  output +=  WStringToString(hotfix.description);
	  output += "\n";

	  output += "Installed By:   ";
	  output += WStringToString(hotfix.installedBy);
	  output += "\n";

	  output += "Installed On:   ";
	  output += WStringToString(hotfix.installedOn);
	  output += "\n";
	  
	  output += "--------------------------\n";
        }
	
    } catch (const std::runtime_error& e) {
      output += e.what();
      output += "\n";
    }
    
    // 8. Clean up
    if (pSvc) pSvc->Release();
    if (pLoc) pLoc->Release();
    CoUninitialize();
    
    return 0;
}

std::string GetTime30DaysAgoISO()
{
  // Helper function to convert system time to ISO 8601 string for XPath
  SYSTEMTIME st;
  GetSystemTime(&st);
  
  // Subtract 30 days
  FILETIME ft, localFt;
  SystemTimeToFileTime(&st, &ft);
  ULARGE_INTEGER uli;
  uli.LowPart = ft.dwLowDateTime;
  uli.HighPart = ft.dwHighDateTime;
  
  // 30 days * 24 hours * 60 min * 60 sec * 10^7 (nanoseconds in intervals of 100ns)
  uli.QuadPart -= (ULONGLONG)30 * 24 * 60 * 60 * 10000000;
  
  ft.dwLowDateTime = uli.LowPart;
  ft.dwHighDateTime = uli.HighPart;
  FileTimeToSystemTime(&ft, &st);
  
  char buffer[32];
  snprintf(buffer, sizeof(buffer), "%04d-%02d-%02dT%02d:%02d:%02d.000Z",
	   st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);
  return std::string(buffer);
}

int QueryRecentEvents(const std::wstring &channel, const std::wstring query, std::string &output)
{
  // std::string timeFilter = GetTime30DaysAgoISO();
  // std::wstring channel = L"Microsoft-Windows-Windows Defender/Operational";
  
  // Level 1: Critical
  // Level 2: Error
  // Level 3: Warning
  // Level 4: Information
  // Level 5: Verbose
  // XPath query to get events after the calculated time
  // std::wstring query = L"Event[System[Level=1 or Level=2 or Level=3 and TimeCreated[@SystemTime > '" + 
  // std::wstring(timeFilter.begin(), timeFilter.end()) + L"']]]";

  int error_level = 0;
  output.clear();
  
  // Querying the System log channel
  EVT_HANDLE hResults = EvtQuery(NULL, channel.c_str(), query.c_str(), EvtQueryChannelPath | EvtQueryReverseDirection);
  
  if (hResults == NULL) {
    output += "ERROR - EvtQuery failed with error: ";
    output += std::to_string(GetLastError());
    output += "\n";
    return -1;
  }
  
  EVT_HANDLE hEvents[10];
  DWORD dwReturned = 0;
  bool bStatus = true;
  
  while (bStatus) {
    bStatus = EvtNext(hResults, 10, hEvents, INFINITE, 0, &dwReturned);
    if (!bStatus) {
      if (GetLastError() == ERROR_NO_MORE_ITEMS) break;
      output += "ERROR - EvtNext failed with error: ";
      output += std::to_string(GetLastError());
      output += "\n";
      error_level = -1;
      break;
    }
    
    for (DWORD i = 0; i < dwReturned; i++) {
      // Render the event into XML
      DWORD dwBufferSize = 0;
      DWORD dwBufferUsed = 0;
      DWORD dwPropertyCount = 0;
      
      EvtRender(NULL, hEvents[i], EvtRenderEventXml, dwBufferSize, NULL, &dwBufferUsed, &dwPropertyCount);
      
      if (GetLastError() == ERROR_INSUFFICIENT_BUFFER) {
	std::vector<wchar_t> buffer(dwBufferUsed / sizeof(wchar_t) + 1);
	if (EvtRender(NULL, hEvents[i], EvtRenderEventXml, (DWORD)buffer.size() * sizeof(wchar_t), &buffer[0], &dwBufferUsed, &dwPropertyCount)) {
	  output += WStringToString(&buffer[0]);
	  output += "\n\n";
	}
      }
      EvtClose(hEvents[i]);
    }
  }
  EvtClose(hResults);
  
  return error_level;
}

int get_windows_defender_alerts(std::string &output)
{
  std::string timeFilter = GetTime30DaysAgoISO();
  std::wstring channel = L"Microsoft-Windows-Windows Defender/Operational";
  
  // Level 1: Critical
  // Level 2: Error
  // Level 3: Warning
  // Level 4: Information
  // Level 5: Verbose
  // XPath query to get events after the calculated time
  std::wstring query = L"Event[System[Level=1 or Level=2 or Level=3 and TimeCreated[@SystemTime > '" + 
    std::wstring(timeFilter.begin(), timeFilter.end()) + L"']]]";

  int error_level = 0;
  output.clear();

  error_level = QueryRecentEvents(channel, query, output);
  
  return error_level;
}

int DumpUninstallKey(HKEY hRootKey, const wchar_t* subKeyPath, REGSAM samDesired, std::string &output)
{
  int ERROR_LEVEL = 0;
  
  HKEY hKey = NULL;
  if (RegOpenKeyExW(hRootKey, subKeyPath, 0, KEY_READ | samDesired, &hKey) != ERROR_SUCCESS) {
    output += "ERROR - Error opening registry key for app listing";
    output += "\n";
    return -1;
  }
  
  WCHAR subKeyName[256];
  DWORD subKeyNameSize;
  DWORD index = 0;
  
  while (true) {
    subKeyNameSize = ARRAYSIZE(subKeyName);
    LONG result = RegEnumKeyExW(hKey, index, subKeyName, &subKeyNameSize, NULL, NULL, NULL, NULL);
    if (result == ERROR_NO_MORE_ITEMS) {
      break;
    }
    if (result != ERROR_SUCCESS) {
      index++;
      continue;
    }
    
    HKEY hSubKey = NULL;
    WCHAR fullPath[512];
    wsprintfW(fullPath, L"%s\\%s", subKeyPath, subKeyName);
    
    if (RegOpenKeyExW(hRootKey, fullPath, 0, KEY_READ | samDesired, &hSubKey) == ERROR_SUCCESS) {
      WCHAR displayName[512] = { 0 };
      WCHAR displayVersion[128] = { 0 };
      DWORD dataSize = sizeof(displayName);
      
      // Get Display Name
      if (RegQueryValueExW(hSubKey, L"DisplayName", NULL, NULL, (LPBYTE)displayName, &dataSize) == ERROR_SUCCESS) {
	dataSize = sizeof(displayVersion);
	RegQueryValueExW(hSubKey, L"DisplayVersion", NULL, NULL, (LPBYTE)displayVersion, &dataSize);
	output += "Name: ";
	output += WStringToString(displayName);
	if (displayVersion[0] != L'\0') {
	  output += " | ";
	  output += "Version: ";
	  output += WStringToString(displayVersion);
	}
	output += "\n";
      }
      RegCloseKey(hSubKey);
    }
    index++;
  }
  RegCloseKey(hKey);
  
  return ERROR_LEVEL;
}

int get_all_apps(std::string &output)
{
  int error_level = 0;
  
  output.clear();

  output += "--- 64-bit / Default Hive ---\n";
  error_level = DumpUninstallKey(HKEY_LOCAL_MACHINE, L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Uninstall", KEY_WOW64_64KEY, output);
  
  output += "\n--- 32-bit Wow6432Node Hive ---\n";
  error_level += DumpUninstallKey(HKEY_LOCAL_MACHINE, L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Uninstall", KEY_WOW64_32KEY, output);
  
  // output += "\n--- Per-User Applications ---\n";
  // error_level += DumpUninstallKey(HKEY_CURRENT_USER, L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Uninstall", KEY_WOW64_64KEY, output);
  
  // output += "\n--- Per-User Applications 32-bit Applications on 64-bit Windows (WOW64) ---\n";
  // error_level += DumpUninstallKey(HKEY_CURRENT_USER, L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Uninstall", KEY_WOW64_32KEY, output);
  
  return error_level;
}

int get_dell_service_tag(std::string &output)
{
  int ERROR_LEVEL = 0;
  int rv = 0;
  
  output.clear();  

  rv = GetDellServiceTag(output);
  ERROR_LEVEL += rv;

  rv = GetDellAssetTag(output);
  ERROR_LEVEL += rv;
  
  return ERROR_LEVEL;
}

int GetDellServiceTag(std::string &output)
{
  int ERROR_LEVEL = 0;
  output.clear();
  
  HRESULT hres;
  
  // 1. Initialize COM
  hres = CoInitializeEx(0, COINIT_MULTITHREADED);
  if (FAILED(hres)) {
    output += "ERROR - Failed to initialize COM. Error: ";
    output += std::to_string(hres);
    output += "\n";
    return -1;
  }
  
  // 2. Set general COM security levels
  hres = CoInitializeSecurity(
			      NULL, -1, NULL, NULL,
			      RPC_C_AUTHN_LEVEL_DEFAULT,
			      RPC_C_IMP_LEVEL_IMPERSONATE,
			      NULL, EOAC_NONE, NULL
			      );
  
  // 3. Obtain the initial locator to WMI
  IWbemLocator* pLoc = NULL;
  hres = CoCreateInstance(
			  CLSID_WbemLocator, NULL,
			  CLSCTX_INPROC_SERVER,
			  IID_IWbemLocator, (LPVOID*)&pLoc
			  );
  if (FAILED(hres)) {
    output += "ERROR - Failed to create IWbemLocator. Error: ";
    output += std::to_string(hres);
    output += "\n";
    CoUninitialize();
    return -1;
  }
  
  // 4. Connect to root\cimv2
  IWbemServices* pSvc = NULL;
  hres = pLoc->ConnectServer(
			     _bstr_t(L"ROOT\\CIMV2"),
			     NULL, NULL, NULL, 0, NULL, NULL, &pSvc
			     );
  if (FAILED(hres)) {
    output += "ERROR - Could not connect to WMI. Error: ";
    output += std::to_string(hres);
    output += "\n";
    pLoc->Release();
    CoUninitialize();
    return -1;
  }
  
  // 5. Set security levels on the proxy
  hres = CoSetProxyBlanket(
			   pSvc, RPC_C_AUTHN_WINNT, RPC_C_AUTHZ_NONE, NULL,
			   RPC_C_AUTHN_LEVEL_CALL, RPC_C_IMP_LEVEL_IMPERSONATE,
			   NULL, EOAC_NONE
			   );
  
  // 6. Use the pointer to make a WMI query
  IEnumWbemClassObject* pEnumerator = NULL;
  hres = pSvc->ExecQuery(
			 bstr_t("WQL"),
			 bstr_t("SELECT SerialNumber FROM Win32_BIOS"),
			 WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY,
			 NULL, &pEnumerator
			 );
  
  if (FAILED(hres)) {
    output += "ERROR - Query failed.. Error: ";
    output += std::to_string(hres);
    output += "\n";
    pSvc->Release();
    pLoc->Release();
    CoUninitialize();
    return -1;
  }
  
  // 7. Retrieve the data from the query
  IWbemClassObject* pclsObj = NULL;
  ULONG uReturn = 0;
  while (pEnumerator) {
    hres = pEnumerator->Next(WBEM_INFINITE, 1, &pclsObj, &uReturn);
    if (0 == uReturn) {
      break;
    }
    VARIANT vtProp;
    VariantInit(&vtProp);
    hres = pclsObj->Get(L"SerialNumber", 0, &vtProp, 0, 0);
    if (SUCCEEDED(hres) && vtProp.vt == VT_BSTR) {
      output += "Dell Service Tag (Serial Number): ";
      output += (const char*)_bstr_t(vtProp.bstrVal); 
      output += "\n";
    }
    VariantClear(&vtProp);
    pclsObj->Release();
  }
  
  // 8. Cleanup
  pSvc->Release();
  pLoc->Release();
  pEnumerator->Release();
  CoUninitialize();
  
  return ERROR_LEVEL;
}

int GetDellAssetTag(std::string &output)
{
  int ERROR_LEVEL = 0;
  
  HRESULT hres;
  
  // 1. Initialize COM
  hres = CoInitializeEx(0, COINIT_MULTITHREADED);
  if (FAILED(hres)) {
    output += "ERROR - Failed to initialize COM. Error: ";
    output += std::to_string(hres);
    output += "\n";
    return -1;
  }
  
  // 2. Set general COM security levels
  hres = CoInitializeSecurity(
			      NULL, -1, NULL, NULL,
			      RPC_C_AUTHN_LEVEL_DEFAULT,
			      RPC_C_IMP_LEVEL_IMPERSONATE,
			      NULL, EOAC_NONE, NULL
			      );
  
  // 3. Obtain the initial locator to WMI
  IWbemLocator* pLoc = NULL;
  hres = CoCreateInstance(
			  CLSID_WbemLocator, NULL,
			  CLSCTX_INPROC_SERVER,
			  IID_IWbemLocator, (LPVOID*)&pLoc
			  );
  if (FAILED(hres)) {
    output += "ERROR - Failed to create IWbemLocator. Error: ";
    output += std::to_string(hres);
    output += "\n";
    CoUninitialize();
    return -1;
  }
  
  // 4. Connect to root\cimv2
  IWbemServices* pSvc = NULL;
  hres = pLoc->ConnectServer(
			     _bstr_t(L"ROOT\\CIMV2"),
			     NULL, NULL, NULL, 0, NULL, NULL, &pSvc
			     );
  if (FAILED(hres)) {
    output += "ERROR - Could not connect to WMI. Error: ";
    output += std::to_string(hres);
    output += "\n";
    pLoc->Release();
    CoUninitialize();
    return -1;
  }
  
  // 5. Set security levels on the proxy
  hres = CoSetProxyBlanket(
			   pSvc, RPC_C_AUTHN_WINNT, RPC_C_AUTHZ_NONE, NULL,
			   RPC_C_AUTHN_LEVEL_CALL, RPC_C_IMP_LEVEL_IMPERSONATE,
			   NULL, EOAC_NONE
			   );
  
  // 6. Use the pointer to make a WMI query
  IEnumWbemClassObject* pEnumerator = NULL;
  hres = pSvc->ExecQuery(
			 bstr_t("WQL"),
			 bstr_t("SELECT SMBIOSAssetTag FROM Win32_SystemEnclosure"),
			 WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY,
			 NULL, &pEnumerator
			 );
  
  if (FAILED(hres)) { 
    output += "ERROR - Query failed.. Error: ";
    output += std::to_string(hres);
    output += "\n";
    pSvc->Release();
    pLoc->Release();
    CoUninitialize();
    return -1;
  }
  
  // 7. Retrieve the data from the query
  IWbemClassObject* pclsObj = NULL;
  ULONG uReturn = 0;
  while (pEnumerator) {
    hres = pEnumerator->Next(WBEM_INFINITE, 1, &pclsObj, &uReturn);
    if (0 == uReturn) {
      break;
    }
    VARIANT vtProp;
    VariantInit(&vtProp);
    hres = pclsObj->Get(L"SMBIOSAssetTag", 0, &vtProp, 0, 0);
    if (SUCCEEDED(hres) && vtProp.vt == VT_BSTR) {
      
      output += "Dell Asset Tag (Barcode): ";
      output += (const char*)_bstr_t(vtProp.bstrVal); 
      output += "\n";
    }
    VariantClear(&vtProp);
    pclsObj->Release();
  }
  
  // 8. Cleanup
  pSvc->Release();
  pLoc->Release();
  pEnumerator->Release();
  CoUninitialize();
  
  return ERROR_LEVEL;
}

int GetIPAddress(std::string &output)
{
  int ERROR_LEVEL = 0;
  output.clear();
  
  // 1. Initialize Winsock
  WSADATA wsaData;
  if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
    output += "Winsock initialization failed.\n";
    return -11;
  }

  // 2. Allocate buffer for adapter information
  ULONG bufferSize = 15000; // Recommended default size
  std::vector<BYTE> buffer(bufferSize);
  PIP_ADAPTER_ADDRESSES adapters = reinterpret_cast<PIP_ADAPTER_ADDRESSES>(buffer.data());
  
  // 3. Query network adapters for IPv4 addresses
  ULONG flags = GAA_FLAG_INCLUDE_PREFIX;
  DWORD result = GetAdaptersAddresses(AF_INET, flags, nullptr, adapters, &bufferSize);
  
  // Handle buffer overflow by resizing and re-retrieving if necessary
  if (result == ERROR_BUFFER_OVERFLOW) {
    buffer.resize(bufferSize);
    adapters = reinterpret_cast<PIP_ADAPTER_ADDRESSES>(buffer.data());
    result = GetAdaptersAddresses(AF_INET, flags, nullptr, adapters, &bufferSize);
  }
  
  if (result != NO_ERROR) {
    output += "ERROR - Failed to retrieve network adapters. Error code: ";
    output += std::to_string(result);
    output += "\n";
    WSACleanup();
    return -1;
  }

    // 4. Iterate through the linked list of network adapters
    for (PIP_ADAPTER_ADDRESSES adapter = adapters; adapter != nullptr; adapter = adapter->Next) {
        // Skip adapters that are down or inactive
        if (adapter->OperStatus != IfOperStatusUp) {
            continue;
        }

        // Iterate through unicast IP addresses assigned to this adapter
        for (PIP_ADAPTER_UNICAST_ADDRESS unicast = adapter->FirstUnicastAddress; unicast != nullptr; unicast = unicast->Next) {
            sockaddr* socketAddress = unicast->Address.lpSockaddr;

            // Confirm it is an IPv4 address
            if (socketAddress->sa_family == AF_INET) {
                sockaddr_in* ipv4 = reinterpret_cast<sockaddr_in*>(socketAddress);
                char ipString[INET_ADDRSTRLEN];

                // Convert the binary IP structure into a human-readable string
                if (inet_ntop(AF_INET, &(ipv4->sin_addr), ipString, sizeof(ipString))) {
                    std::string ipAddr(ipString);

                    // Skip the local loopback address
                    if (ipAddr != "127.0.0.1") {
		      output += "Adapter: ";
		      output += PWCHAR_to_string(adapter->FriendlyName);
		      output += "\n";

		      output += "IPv4 Address: ";
		      output += ipAddr;
		      output += "\n\n";
                    }
                }
            }
        }
    }
    
    // 5. Clean up the Winsock environment
    WSACleanup();
    
    return ERROR_LEVEL;
}

std::string PWCHAR_to_string(PWCHAR pwchar) {
  if (!pwchar) return std::string();
  
  // 1. Calculate the required buffer size
  int size_needed = WideCharToMultiByte(CP_UTF8, 0, pwchar, -1, NULL, 0, NULL, NULL);
  
  // 2. Allocate the string buffer
  std::string result(size_needed - 1, 0); // -1 to exclude the null terminator
  
  // 3. Perform the actual conversion
  WideCharToMultiByte(CP_UTF8, 0, pwchar, -1, &result[0], size_needed, NULL, NULL);
  
  return result;
}

