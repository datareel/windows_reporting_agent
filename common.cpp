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
  
  // WinINet error messages (12001-12999) live in wininet.dll, not the system table
  DWORD flags = FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS;
  HMODULE hWininet = NULL;
  if (errorMessageID >= INTERNET_ERROR_BASE && errorMessageID <= INTERNET_ERROR_LAST) {
    hWininet = GetModuleHandleA("wininet.dll");
    if (hWininet) flags |= FORMAT_MESSAGE_FROM_HMODULE;
  }

  // Ask Win32 to give us the string representation of that message ID.
  // The parameters we pass tell FormatMessage to allocate the buffer.
  size_t size = FormatMessageA(
			       flags,
			       hWininet, 
			       errorMessageID, 
			       MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), 
			       (LPSTR)&messageBuffer, 
			       0, 
			       NULL
			       );
  
  if (size == 0 || messageBuffer == nullptr) {
    return "error code " + std::to_string(errorMessageID);
  }

  // Copy the error message into a std::string.
  std::string message(messageBuffer, size);
  while (!message.empty() && (message.back() == '\r' || message.back() == '\n' || message.back() == ' ')) {
    message.pop_back();
  }
  message += " (error code " + std::to_string(errorMessageID) + ")";
  
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

void SetDefaultAgentConfig(AgentConfig &cfg)
{
  cfg.delivery_methods.clear();
  cfg.delivery_methods.push_back("https");
  cfg.server_host = "reports.example.com";
  cfg.server_port = INTERNET_DEFAULT_HTTPS_PORT;
  cfg.upload_path = "/windows/upload/upload.php";
  cfg.use_https = true;
  cfg.ignore_cert_revocation = true;
  cfg.cifs_share.clear();
  cfg.scp_host.clear();
  cfg.scp_port = 22;
  cfg.scp_user.clear();
  cfg.scp_key_file.clear();
  cfg.scp_remote_dir.clear();
  cfg.scp_path = "%SystemRoot%\\System32\\OpenSSH\\scp.exe";
  cfg.scp_known_hosts.clear();
  cfg.scp_host_key_check = "yes";
  cfg.scp_timeout_seconds = 120;
  cfg.report_interval_minutes = 45;
  cfg.report_dir = "C:\\";
  cfg.log_file = "C:\\windows_report_agent.log";
  cfg.users_dir = "C:\\Users";
  cfg.event_lookback_days = 30;
  cfg.ini_path.clear();
}

static std::string ReadIniString(const char *section, const char *key, const std::string &default_value, const std::string &ini_path)
{
  char buffer[1024];
  GetPrivateProfileStringA(section, key, default_value.c_str(), buffer, sizeof(buffer), ini_path.c_str());
  return std::string(buffer);
}

static std::string Trim(const std::string &str)
{
  size_t start = str.find_first_not_of(" \t");
  if(start == std::string::npos) return std::string();
  size_t end = str.find_last_not_of(" \t");
  return str.substr(start, end - start + 1);
}

static std::string ToLower(std::string str)
{
  for (char &c : str) {
    c = std::tolower((unsigned char)c);
  }
  return str;
}

// Split a comma separated list such as "https, cifs" into lower case items
static std::vector<std::string> SplitList(const std::string &list)
{
  std::vector<std::string> items;
  size_t start = 0;
  while(start <= list.length()) {
    size_t end = list.find(',', start);
    if(end == std::string::npos) end = list.length();
    std::string item = ToLower(Trim(list.substr(start, end - start)));
    if(!item.empty()) items.push_back(item);
    start = end + 1;
  }
  return items;
}

static int ReadIniInt(const char *section, const char *key, int default_value, const std::string &ini_path)
{
  return (int)GetPrivateProfileIntA(section, key, default_value, ini_path.c_str());
}

int LoadAgentConfig(AgentConfig &cfg, std::string &output)
{
  SetDefaultAgentConfig(cfg);

  // Look for the INI file in the same directory as the executable
  char exe_path[MAX_PATH];
  DWORD len = GetModuleFileNameA(NULL, exe_path, MAX_PATH);
  if(len == 0 || len >= MAX_PATH) {
    output += "WARNING - Could not get executable path, using default settings\n";
    return 1;
  }
  std::string ini_path = exe_path;
  size_t pos = ini_path.find_last_of("\\/");
  ini_path = (pos == std::string::npos) ? AGENT_INI_FILE : ini_path.substr(0, pos + 1) + AGENT_INI_FILE;
  cfg.ini_path = ini_path;

  if(GetFileAttributesA(ini_path.c_str()) == INVALID_FILE_ATTRIBUTES) {
    output += "WARNING - Config file " + ini_path + " not found, using default settings\n";
    return 1;
  }

  std::string methods = ReadIniString("delivery", "methods", "https", ini_path);
  cfg.delivery_methods = SplitList(methods);
  if(cfg.delivery_methods.empty()) {
    output += "WARNING - No delivery methods in " + ini_path + ", using https\n";
    cfg.delivery_methods.push_back("https");
  }

  cfg.server_host = ReadIniString("https", "host", cfg.server_host, ini_path);
  cfg.use_https = ReadIniInt("https", "use_https", cfg.use_https ? 1 : 0, ini_path) != 0;
  if(!cfg.use_https) cfg.server_port = INTERNET_DEFAULT_HTTP_PORT;
  cfg.server_port = ReadIniInt("https", "port", cfg.server_port, ini_path);
  cfg.upload_path = ReadIniString("https", "upload_path", cfg.upload_path, ini_path);
  cfg.ignore_cert_revocation = ReadIniInt("https", "ignore_cert_revocation", cfg.ignore_cert_revocation ? 1 : 0, ini_path) != 0;

  cfg.cifs_share = ReadIniString("cifs", "share", cfg.cifs_share, ini_path);

  cfg.scp_host = ReadIniString("scp", "host", cfg.scp_host, ini_path);
  cfg.scp_port = ReadIniInt("scp", "port", cfg.scp_port, ini_path);
  cfg.scp_user = ReadIniString("scp", "user", cfg.scp_user, ini_path);
  cfg.scp_key_file = ReadIniString("scp", "key_file", cfg.scp_key_file, ini_path);
  cfg.scp_remote_dir = ReadIniString("scp", "remote_dir", cfg.scp_remote_dir, ini_path);
  cfg.scp_path = ReadIniString("scp", "scp_path", cfg.scp_path, ini_path);
  cfg.scp_known_hosts = ReadIniString("scp", "known_hosts_file", cfg.scp_known_hosts, ini_path);
  cfg.scp_host_key_check = ToLower(ReadIniString("scp", "strict_host_key_checking", cfg.scp_host_key_check, ini_path));
  cfg.scp_timeout_seconds = ReadIniInt("scp", "timeout_seconds", cfg.scp_timeout_seconds, ini_path);

  cfg.report_interval_minutes = ReadIniInt("agent", "report_interval_minutes", cfg.report_interval_minutes, ini_path);
  cfg.report_dir = ReadIniString("agent", "report_dir", cfg.report_dir, ini_path);
  cfg.log_file = ReadIniString("agent", "log_file", cfg.log_file, ini_path);
  cfg.users_dir = ReadIniString("agent", "users_dir", cfg.users_dir, ini_path);
  cfg.event_lookback_days = ReadIniInt("agent", "event_lookback_days", cfg.event_lookback_days, ini_path);

  // Keep values in a usable range
  if(cfg.server_port <= 0 || cfg.server_port > 65535) {
    output += "WARNING - Invalid port in " + ini_path + ", using default\n";
    cfg.server_port = cfg.use_https ? INTERNET_DEFAULT_HTTPS_PORT : INTERNET_DEFAULT_HTTP_PORT;
  }
  if(cfg.scp_port <= 0 || cfg.scp_port > 65535) cfg.scp_port = 22;
  if(cfg.scp_timeout_seconds < 10) cfg.scp_timeout_seconds = 10;
  if(cfg.scp_host_key_check != "yes" && cfg.scp_host_key_check != "accept-new") {
    output += "WARNING - strict_host_key_checking must be yes or accept-new, using yes\n";
    cfg.scp_host_key_check = "yes";
  }
  if(cfg.report_interval_minutes < 1) cfg.report_interval_minutes = 1;
  if(cfg.event_lookback_days < 1) cfg.event_lookback_days = 1;
  if(cfg.upload_path.empty() || cfg.upload_path[0] != '/') cfg.upload_path = "/" + cfg.upload_path;

  output += "Loaded settings from " + ini_path + "\n";
  return 0;
}

std::string JoinPath(const std::string &dir, const std::string &name)
{
  if(dir.empty()) return name;
  char last = dir[dir.length() - 1];
  if(last == '\\' || last == '/') return dir + name;
  return dir + "\\" + name;
}

int DeliverReport(const AgentConfig &cfg, const std::string& input_fname, const std::string& upload_fname, std::string& output_str)
{
  // Run every method listed in [delivery] methods, so one failing
  // method does not stop the others
  int error_level = 0;
  output_str.clear();

  for (const std::string &method : cfg.delivery_methods) {
    std::string method_output;
    int rv = 0;
    if(method == "https") {
      rv = UploadFile(cfg, input_fname, upload_fname, method_output);
    }
    else if(method == "cifs") {
      rv = CopyToShare(cfg, input_fname, upload_fname, method_output);
    }
    else if(method == "scp") {
      rv = ScpFile(cfg, input_fname, upload_fname, method_output);
    }
    else {
      rv = -1;
      method_output = "ERROR - Unknown delivery method " + method;
    }
    output_str += "[" + method + "] " + method_output + "\n";
    if(rv != 0) error_level = -1;
  }

  return error_level;
}

int CopyToShare(const AgentConfig &cfg, const std::string& input_fname, const std::string& upload_fname, std::string& output_str)
{
  // The service runs as LocalSystem, so on a domain it reaches the share
  // as the computer account (DOMAIN\HOSTNAME$). Grant that account, or
  // Domain Computers, write access to the share.
  if(cfg.cifs_share.empty()) {
    output_str = "ERROR - No share set in the [cifs] section";
    return -1;
  }

  // Copy to a temporary name first, then rename, so a reader on the
  // share never sees a partly written report
  std::string dest_fname = JoinPath(cfg.cifs_share, upload_fname);
  std::string tmp_fname = dest_fname + ".tmp";

  if(!CopyFileA(input_fname.c_str(), tmp_fname.c_str(), FALSE)) {
    output_str = "ERROR - Could not copy report to " + tmp_fname + ": " + GetLastErrorAsString();
    return -1;
  }
  if(!MoveFileExA(tmp_fname.c_str(), dest_fname.c_str(), MOVEFILE_REPLACE_EXISTING)) {
    output_str = "ERROR - Could not rename " + tmp_fname + " to " + dest_fname + ": " + GetLastErrorAsString();
    DeleteFileA(tmp_fname.c_str());
    return -1;
  }

  output_str = "Copied report to " + dest_fname;
  return 0;
}

static std::string ExpandEnv(const std::string &str)
{
  char buffer[MAX_PATH * 2];
  DWORD len = ExpandEnvironmentStringsA(str.c_str(), buffer, sizeof(buffer));
  if(len == 0 || len > sizeof(buffer)) return str;
  return std::string(buffer);
}

int RunCommand(const std::string &cmdline, DWORD timeout_ms, std::string &output)
{
  output.clear();

  // Capture the program's stdout and stderr for the log file
  SECURITY_ATTRIBUTES sa = { sizeof(SECURITY_ATTRIBUTES), NULL, TRUE };
  HANDLE hRead = NULL, hWrite = NULL;
  if(!CreatePipe(&hRead, &hWrite, &sa, 0)) {
    output = "ERROR - Could not create pipe: " + GetLastErrorAsString();
    return -1;
  }
  SetHandleInformation(hRead, HANDLE_FLAG_INHERIT, 0);

  STARTUPINFOA si;
  PROCESS_INFORMATION pi;
  memset(&si, 0, sizeof(si));
  memset(&pi, 0, sizeof(pi));
  si.cb = sizeof(si);
  si.dwFlags = STARTF_USESTDHANDLES;
  si.hStdInput = NULL;
  si.hStdOutput = hWrite;
  si.hStdError = hWrite;

  std::vector<char> cmd(cmdline.begin(), cmdline.end());
  cmd.push_back('\0');

  if(!CreateProcessA(NULL, cmd.data(), NULL, NULL, TRUE, CREATE_NO_WINDOW, NULL, NULL, &si, &pi)) {
    output = "ERROR - Could not run " + cmdline + ": " + GetLastErrorAsString();
    CloseHandle(hRead);
    CloseHandle(hWrite);
    return -1;
  }
  CloseHandle(hWrite);

  // Read output until the program exits or the timeout runs out
  DWORD start = GetTickCount();
  char buffer[1024];
  for(;;) {
    DWORD avail = 0;
    if(PeekNamedPipe(hRead, NULL, 0, NULL, &avail, NULL) && avail > 0) {
      DWORD bytesRead = 0;
      if(ReadFile(hRead, buffer, sizeof(buffer) - 1, &bytesRead, NULL) && bytesRead > 0) {
	buffer[bytesRead] = '\0';
	output += buffer;
      }
      continue;
    }
    if(WaitForSingleObject(pi.hProcess, 100) == WAIT_OBJECT_0) {
      // Drain anything left in the pipe
      DWORD bytesRead = 0;
      while(PeekNamedPipe(hRead, NULL, 0, NULL, &avail, NULL) && avail > 0 &&
	    ReadFile(hRead, buffer, sizeof(buffer) - 1, &bytesRead, NULL) && bytesRead > 0) {
	buffer[bytesRead] = '\0';
	output += buffer;
      }
      break;
    }
    if(GetTickCount() - start > timeout_ms) {
      TerminateProcess(pi.hProcess, 1);
      output += "ERROR - Command timed out\n";
      CloseHandle(pi.hProcess);
      CloseHandle(pi.hThread);
      CloseHandle(hRead);
      return -1;
    }
  }

  DWORD exit_code = 1;
  GetExitCodeProcess(pi.hProcess, &exit_code);
  CloseHandle(pi.hProcess);
  CloseHandle(pi.hThread);
  CloseHandle(hRead);

  return (exit_code == 0) ? 0 : -1;
}

int ScpFile(const AgentConfig &cfg, const std::string& input_fname, const std::string& upload_fname, std::string& output_str)
{
  // Uses the OpenSSH client that ships with Windows 10/11 and Server 2019+.
  // The key must have no passphrase, since the service runs unattended.
  if(cfg.scp_host.empty() || cfg.scp_user.empty() || cfg.scp_key_file.empty() || cfg.scp_remote_dir.empty()) {
    output_str = "ERROR - The [scp] section needs host, user, key_file and remote_dir";
    return -1;
  }

  std::string scp_exe = ExpandEnv(cfg.scp_path);
  if(GetFileAttributesA(scp_exe.c_str()) == INVALID_FILE_ATTRIBUTES) {
    // A 32-bit build sees the 32-bit System32, which has no OpenSSH,
    // so try the 64-bit System32 through Sysnative
    std::string native_exe = ExpandEnv("%SystemRoot%\\Sysnative\\OpenSSH\\scp.exe");
    if(GetFileAttributesA(native_exe.c_str()) == INVALID_FILE_ATTRIBUTES) {
      output_str = "ERROR - scp not found at " + scp_exe + ". Install the Windows OpenSSH client or set scp_path";
      return -1;
    }
    scp_exe = native_exe;
  }

  std::string remote_dir = cfg.scp_remote_dir;
  if(remote_dir[remote_dir.length() - 1] != '/') remote_dir += "/";

  std::string cmdline = "\"" + scp_exe + "\" -q -B";
  cmdline += " -P " + std::to_string(cfg.scp_port);
  cmdline += " -i \"" + ExpandEnv(cfg.scp_key_file) + "\"";
  cmdline += " -o StrictHostKeyChecking=" + cfg.scp_host_key_check;
  if(!cfg.scp_known_hosts.empty()) {
    cmdline += " -o UserKnownHostsFile=\"" + ExpandEnv(cfg.scp_known_hosts) + "\"";
  }
  cmdline += " -o ConnectTimeout=30";
  cmdline += " \"" + input_fname + "\"";
  cmdline += " \"" + cfg.scp_user + "@" + cfg.scp_host + ":" + remote_dir + upload_fname + "\"";

  std::string cmd_output;
  int rv = RunCommand(cmdline, (DWORD)cfg.scp_timeout_seconds * 1000, cmd_output);
  if(rv != 0) {
    output_str = "ERROR - scp to " + cfg.scp_user + "@" + cfg.scp_host + ":" + remote_dir + " failed: " + cmd_output;
    return -1;
  }

  output_str = "Copied report to " + cfg.scp_user + "@" + cfg.scp_host + ":" + remote_dir + upload_fname;
  return 0;
}

int UploadFile(const AgentConfig &cfg, const std::string& input_fname, const std::string& upload_fname, std::string& output_str)
{
  int error_level = 0;
  
  // 1. Initialize WinINet
  std::string user_agent = "WindowsReportAgent/" AGENT_VERSION;
  HINTERNET hSession = InternetOpen(user_agent.c_str(), INTERNET_OPEN_TYPE_DIRECT, NULL, NULL, 0);
  if (!hSession) {
    output_str = "ERROR - Could not initialize WinINet session"; 
    return -1;
  }
  
  // 2. Connect to the server
  HINTERNET hConnect = InternetConnect(hSession, cfg.server_host.c_str(), (INTERNET_PORT)cfg.server_port, NULL, NULL, INTERNET_SERVICE_HTTP, 0, 0);
  if (!hConnect) {
    InternetCloseHandle(hSession);
    output_str = "ERROR - Could not connect to server " + cfg.server_host; 
    return -1;
  }
  
  // 3. Create the request
  const char* acceptTypes[] = { "*/*", NULL };
  DWORD request_flags = cfg.use_https ? INTERNET_FLAG_SECURE : 0;
  HINTERNET hRequest = HttpOpenRequest(hConnect, "POST", cfg.upload_path.c_str(), TEXT("HTTP/1.1"), NULL, acceptTypes, request_flags, 0);

  // 06/04/2026: Fix for error code: 12057 cannot validate the SSL/TLS certificate's revocation status.
  // This is usually caused by network security settings or blocked access to a Certificate Revocation List (CRL) server.
  // Set ignore_cert_revocation = 0 in the INI file to enforce revocation checks.
  if (hRequest && cfg.use_https && cfg.ignore_cert_revocation) {
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

std::string GetTimeDaysAgoISO(int days)
{
  // Helper function to convert system time to ISO 8601 string for XPath
  SYSTEMTIME st;
  GetSystemTime(&st);
  
  // Subtract the number of days
  FILETIME ft, localFt;
  SystemTimeToFileTime(&st, &ft);
  ULARGE_INTEGER uli;
  uli.LowPart = ft.dwLowDateTime;
  uli.HighPart = ft.dwHighDateTime;
  
  // days * 24 hours * 60 min * 60 sec * 10^7 (nanoseconds in intervals of 100ns)
  uli.QuadPart -= (ULONGLONG)days * 24 * 60 * 60 * 10000000;
  
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
  // std::string timeFilter = GetTimeDaysAgoISO(30);
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

int get_windows_defender_alerts(std::string &output, int days)
{
  std::string timeFilter = GetTimeDaysAgoISO(days);
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

