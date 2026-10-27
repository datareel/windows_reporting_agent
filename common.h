#ifndef COMMON_H
#define COMMON_H

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <string>
#include <vector>

#define AGENT_VERSION "3.3-beta1"
#define AGENT_INI_FILE "drra.ini"

// Agent settings, loaded from AGENT_INI_FILE in the same directory as the
// executable. Any setting missing from the INI file keeps its default.
struct AgentConfig {
    // [delivery]
    std::vector<std::string> delivery_methods; // methods: https, cifs, scp
    // [https]
    std::string server_host;        // host
    int server_port;                // port
    std::string upload_path;        // upload_path
    std::string register_path;      // register_path
    std::string enrollment_token;   // enrollment_token
    bool use_https;                 // use_https
    bool ignore_cert_revocation;    // ignore_cert_revocation
    // [cifs]
    std::string cifs_share;         // share
    std::string cifs_user;          // user: share login for sites without a domain,
                                    // the password is in data_dir (set-cifs-password)
    // [scp]
    std::string scp_host;           // host
    int scp_port;                   // port
    std::string scp_user;           // user
    std::string scp_key_file;       // key_file
    std::string scp_remote_dir;     // remote_dir
    std::string scp_path;           // scp_path
    std::string scp_known_hosts;    // known_hosts_file
    std::string scp_host_key_check; // strict_host_key_checking
    int scp_timeout_seconds;        // timeout_seconds
    // [agent]
    int report_interval_minutes;    // report_interval_minutes
    std::string report_dir;         // report_dir
    std::string log_file;           // log_file
    std::string users_dir;          // users_dir
    int event_lookback_days;        // event_lookback_days
    std::string data_dir;           // data_dir: agent certificate and registration state
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
int DeliverReport(const AgentConfig &cfg, const std::string& input_fname, const std::string& upload_fname, std::string& output_str);
int SetCifsPassword(const AgentConfig &cfg, const std::string &password, std::string &output);
int ReadCifsPassword(const AgentConfig &cfg, std::string &password, std::string &output);
int CopyToShare(const AgentConfig &cfg, const std::string& input_fname, const std::string& upload_fname, std::string& output_str);
int ScpFile(const AgentConfig &cfg, const std::string& input_fname, const std::string& upload_fname, std::string& output_str);
int RunCommand(const std::string &cmdline, DWORD timeout_ms, std::string &output);
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
int get_performance(std::string &output, DWORD cpu_sample_ms);
int get_disk_usage(std::string &output);
int get_network_stats(std::string &output);
std::string PWCHAR_to_string(PWCHAR pwchar);

#endif
