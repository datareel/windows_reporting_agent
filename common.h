#ifndef COMMON_H
#define COMMON_H

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <string>

#define AGENT_VERSION "3.2"
#define AGENT_INI_FILE "windows_reporting_agent.ini"

// Agent settings, loaded from AGENT_INI_FILE in the same directory as the
// executable. Any setting missing from the INI file keeps its default.
struct AgentConfig {
    // [server]
    std::string server_host;        // host
    int server_port;                // port
    std::string upload_path;        // upload_path
    bool use_https;                 // use_https
    bool ignore_cert_revocation;    // ignore_cert_revocation
    // [agent]
    int report_interval_minutes;    // report_interval_minutes
    std::string report_dir;         // report_dir
    std::string log_file;           // log_file
    std::string users_dir;          // users_dir
    int event_lookback_days;        // event_lookback_days
    // Full path of the INI file that was read
    std::string ini_path;
};

struct HotfixInfo {
    std::wstring hotfixID;
    std::wstring installedBy;
    std::wstring installedOn;
    std::wstring description;
};

std::string GetLastErrorAsString();
int list_dirs(const std::string directory, std::string &output);
void generate_boundary_seq_number(char *boundary, int length);
void SetDefaultAgentConfig(AgentConfig &cfg);
int LoadAgentConfig(AgentConfig &cfg, std::string &output);
std::string JoinPath(const std::string &dir, const std::string &name);
int UploadFile(const AgentConfig &cfg, const std::string& input_fname, const std::string& upload_fname, std::string& output_str);
std::string WStringToString(const std::wstring& wstr);
void CheckHResult(HRESULT hr, const char* message, std::string &output);
int get_hotfixes(std::string &output);
std::string GetTimeDaysAgoISO(int days);
int QueryRecentEvents(const std::wstring &channel, const std::wstring query, std::string &output);
int get_windows_defender_alerts(std::string &output, int days = 30);
int DumpUninstallKey(HKEY hRootKey, const wchar_t* subKeyPath, REGSAM samDesired, std::string &output);
int get_all_apps(std::string &output);
int get_dell_service_tag(std::string &output);
int GetDellServiceTag(std::string &output);
int GetDellAssetTag(std::string &output);
int GetIPAddress(std::string &output);
std::string PWCHAR_to_string(PWCHAR pwchar);

#endif
