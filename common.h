#ifndef COMMON_H
#define COMMON_H

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <string>

struct HotfixInfo {
    std::wstring hotfixID;
    std::wstring installedBy;
    std::wstring installedOn;
    std::wstring description;
};

std::string GetLastErrorAsString();
int list_dirs(const std::string directory, std::string &output);
void generate_boundary_seq_number(char *boundary, int length);
int UploadFile(const std::string& server, const std::string& path, const std::string& input_fname, const std::string& upload_fname, std::string& output_str);
std::string WStringToString(const std::wstring& wstr);
void CheckHResult(HRESULT hr, const char* message, std::string &output);
int get_hotfixes(std::string &output);
std::string GetTime30DaysAgoISO();
int QueryRecentEvents(const std::wstring &channel, const std::wstring query, std::string &output);
int get_windows_defender_alerts(std::string &output);
int DumpUninstallKey(HKEY hRootKey, const wchar_t* subKeyPath, REGSAM samDesired, std::string &output);
int get_all_apps(std::string &output);
int get_dell_service_tag(std::string &output);
int GetDellServiceTag(std::string &output);
int GetDellAssetTag(std::string &output);
int GetIPAddress(std::string &output);
std::string PWCHAR_to_string(PWCHAR pwchar);

#endif
