// Vulnerability scan with the Windows Update Agent API, see scan.h

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

// C STD lib includes
#include <stdio.h>
#include <time.h>

// C++ STD lib includes
#include <algorithm>
#include <map>
#include <set>
#include <string>
#include <vector>

// Windows Update Agent (WUA) API
#include <wuapi.h>
#include <comutil.h>
#pragma comment(lib, "Ole32.lib")
#pragma comment(lib, "OleAut32.lib")

// APP includes
#include "common.h"
#include "agent_auth.h"
#include "scan.h"

// Updates not installed yet. Hidden updates were declined by an admin.
#define WUA_CRITERIA L"IsInstalled=0 and Type='Software' and IsHidden=0"

struct WuaUpdate {
  std::string id;              // KB number, or the update ID without one
  std::string severity;        // MSRC severity, Unknown for none
  std::string issued;          // YYYY-MM-DD
  std::string title;
  std::string classification;  // Security Updates, Critical Updates, Drivers ...
  std::set<std::string> cves;
  std::set<std::string> products;
  bool security;
};

static std::string Bstr(BSTR b)
{
  if(b == NULL) return std::string();
  return WStringToString(std::wstring(b, SysStringLen(b)));
}

static std::string QuoteValue(const std::string &s)
{
  std::string q = "\"";
  for(char c : s) {
    if(c == '"' || c == '\\') q += '\\';
    if(c == '\n' || c == '\r' || c == '\t') c = ' ';
    q += c;
  }
  return q + "\"";
}

static std::string JoinSet(const std::set<std::string> &s)
{
  std::string out;
  for(const std::string &v : s) {
    if(!out.empty()) out += ",";
    out += v;
  }
  return out;
}

static std::string HexResult(HRESULT hr)
{
  char buf[16];
  snprintf(buf, sizeof(buf), "0x%08lX", (unsigned long)hr);
  return buf;
}

static int SeverityRank(const std::string &s)
{
  if(s == "Critical") return 0;
  if(s == "Important") return 1;
  if(s == "Moderate") return 2;
  if(s == "Low") return 3;
  return 4;
}

static std::vector<std::string> StringCollection(IStringCollection *c)
{
  std::vector<std::string> out;
  if(c == NULL) return out;
  LONG n = 0;
  c->get_Count(&n);
  for(LONG i = 0; i < n; i++) {
    BSTR b = NULL;
    if(SUCCEEDED(c->get_Item(i, &b)) && b != NULL) {
      out.push_back(Bstr(b));
      SysFreeString(b);
    }
  }
  return out;
}

static std::string FileText(const std::string &fname)
{
  std::string data;
  HANDLE h = CreateFileA(fname.c_str(), GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
  if(h == INVALID_HANDLE_VALUE) return data;
  char buffer[65536];
  DWORD n = 0;
  while(ReadFile(h, buffer, sizeof(buffer), &n, NULL) && n > 0) data.append(buffer, n);
  CloseHandle(h);
  return data;
}

static bool WriteFileText(const std::string &fname, const std::string &data)
{
  HANDLE h = CreateFileA(fname.c_str(), GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
  if(h == INVALID_HANDLE_VALUE) return false;
  DWORD n = 0;
  BOOL ok = data.empty() || WriteFile(h, data.data(), (DWORD)data.length(), &n, NULL);
  CloseHandle(h);
  return ok && n == data.length();
}

bool ScanDue(const AgentConfig &cfg)
{
  if(!cfg.scan_enabled) return false;
  WIN32_FILE_ATTRIBUTE_DATA fa;
  if(!GetFileAttributesExA(JoinPath(cfg.data_dir, SCAN_VULN_FILE).c_str(), GetFileExInfoStandard, &fa)) return true;
  FILETIME now;
  GetSystemTimeAsFileTime(&now);
  ULARGE_INTEGER a, b;
  a.LowPart = fa.ftLastWriteTime.dwLowDateTime; a.HighPart = fa.ftLastWriteTime.dwHighDateTime;
  b.LowPart = now.dwLowDateTime; b.HighPart = now.dwHighDateTime;
  if(b.QuadPart < a.QuadPart) return false;
  return (b.QuadPart - a.QuadPart) / 10000000ULL >= (ULONGLONG)cfg.scan_interval_hours * 3600ULL;
}

//
// Purpose:
//   Reads one update from the search result
//
static void ReadUpdate(IUpdate *u, WuaUpdate &w)
{
  BSTR b = NULL;
  w.security = false;
  if(SUCCEEDED(u->get_Title(&b))) { w.title = Bstr(b); SysFreeString(b); b = NULL; }
  if(SUCCEEDED(u->get_MsrcSeverity(&b))) { w.severity = Bstr(b); SysFreeString(b); b = NULL; }

  IStringCollection *kbs = NULL;
  if(SUCCEEDED(u->get_KBArticleIDs(&kbs)) && kbs != NULL) {
    std::vector<std::string> v = StringCollection(kbs);
    if(!v.empty()) w.id = "KB" + v[0];
    kbs->Release();
  }
  if(w.id.empty()) {
    IUpdateIdentity *ident = NULL;
    if(SUCCEEDED(u->get_Identity(&ident)) && ident != NULL) {
      if(SUCCEEDED(ident->get_UpdateID(&b))) { w.id = Bstr(b); SysFreeString(b); b = NULL; }
      ident->Release();
    }
  }

  DATE d = 0;
  if(SUCCEEDED(u->get_LastDeploymentChangeTime(&d)) && d > 0) {
    SYSTEMTIME st;
    if(VariantTimeToSystemTime(d, &st)) {
      char buf[16];
      snprintf(buf, sizeof(buf), "%04d-%02d-%02d", st.wYear, st.wMonth, st.wDay);
      w.issued = buf;
    }
  }

  // CVEs are on IUpdate2
  IUpdate2 *u2 = NULL;
  if(SUCCEEDED(u->QueryInterface(IID_IUpdate2, (void **)&u2)) && u2 != NULL) {
    IStringCollection *cves = NULL;
    if(SUCCEEDED(u2->get_CveIDs(&cves)) && cves != NULL) {
      for(const std::string &c : StringCollection(cves)) w.cves.insert(c);
      cves->Release();
    }
    u2->Release();
  }

  // Categories: the products it updates and its classification
  ICategoryCollection *cats = NULL;
  if(SUCCEEDED(u->get_Categories(&cats)) && cats != NULL) {
    LONG n = 0;
    cats->get_Count(&n);
    for(LONG i = 0; i < n; i++) {
      ICategory *c = NULL;
      if(FAILED(cats->get_Item(i, &c)) || c == NULL) continue;
      BSTR name = NULL, type = NULL;
      c->get_Name(&name);
      c->get_Type(&type);
      std::string sname = Bstr(name), stype = Bstr(type);
      if(stype == "Product") {
	std::replace(sname.begin(), sname.end(), ',', ' ');
	w.products.insert(sname);
      }
      else if(stype == "UpdateClassification") {
	w.classification = sname;
      }
      SysFreeString(name);
      SysFreeString(type);
      c->Release();
    }
    cats->Release();
  }

  // MSRC rates security updates only. Windows Update leaves some
  // security updates unrated, the classification still tells.
  w.security = !w.severity.empty() || !w.cves.empty() || w.classification == "Security Updates";
  if(w.severity != "Critical" && w.severity != "Important" && w.severity != "Moderate" && w.severity != "Low") {
    w.severity = "Unknown";
  }
}

//
// Purpose:
//   Searches Windows Update (or the WSUS server set by policy) for the
//   updates not installed yet
//
// Return value:
//   0 on success
//
static int SearchUpdates(std::vector<WuaUpdate> &updates, std::string &feed, std::string &output)
{
  int rv = 0;
  HRESULT hinit = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
  bool uninit = SUCCEEDED(hinit);

  IUpdateSession *session = NULL;
  HRESULT hr = CoCreateInstance(CLSID_UpdateSession, NULL, CLSCTX_INPROC_SERVER, IID_IUpdateSession, (LPVOID *)&session);
  if(FAILED(hr)) {
    output += "ERROR - Could not start a Windows Update session: " + HexResult(hr) + "\n";
    if(uninit) CoUninitialize();
    return 1;
  }
  BSTR app = SysAllocString(L"DataReel Reporting Agent");
  session->put_ClientApplicationID(app);
  SysFreeString(app);

  IUpdateSearcher *searcher = NULL;
  hr = session->CreateUpdateSearcher(&searcher);
  if(FAILED(hr)) {
    output += "ERROR - Could not create a Windows Update searcher: " + HexResult(hr) + "\n";
    session->Release();
    if(uninit) CoUninitialize();
    return 1;
  }

  // Default is WSUS when a policy sets one, Windows Update otherwise
  feed = "Windows Update";
  HKEY key;
  if(RegOpenKeyExA(HKEY_LOCAL_MACHINE, "SOFTWARE\\Policies\\Microsoft\\Windows\\WindowsUpdate\\AU", 0, KEY_READ, &key) == ERROR_SUCCESS) {
    DWORD use = 0, size = sizeof(use);
    if(RegQueryValueExA(key, "UseWUServer", NULL, NULL, (LPBYTE)&use, &size) == ERROR_SUCCESS && use == 1) feed = "WSUS";
    RegCloseKey(key);
  }
  if(feed == "WSUS") {
    char server[512];
    DWORD size = sizeof(server);
    if(RegGetValueA(HKEY_LOCAL_MACHINE, "SOFTWARE\\Policies\\Microsoft\\Windows\\WindowsUpdate", "WUServer",
		    RRF_RT_REG_SZ, NULL, server, &size) == ERROR_SUCCESS) {
      feed += " " + std::string(server);
    }
  }

  ISearchResult *result = NULL;
  BSTR criteria = SysAllocString(WUA_CRITERIA);
  hr = searcher->Search(criteria, &result);
  SysFreeString(criteria);
  if(FAILED(hr) || result == NULL) {
    output += "ERROR - Windows Update search failed: " + HexResult(hr) +
      " (see https://learn.microsoft.com/windows/deployment/update/windows-update-error-reference)\n";
    rv = 1;
  }
  else {
    OperationResultCode code = orcNotStarted;
    result->get_ResultCode(&code);
    if(code != orcSucceeded && code != orcSucceededWithErrors) {
      output += "ERROR - Windows Update search did not finish, result code " + std::to_string((int)code) + "\n";
      rv = 1;
    }
    else {
      IUpdateCollection *list = NULL;
      if(SUCCEEDED(result->get_Updates(&list)) && list != NULL) {
	LONG n = 0;
	list->get_Count(&n);
	for(LONG i = 0; i < n; i++) {
	  IUpdate *u = NULL;
	  if(FAILED(list->get_Item(i, &u)) || u == NULL) continue;
	  WuaUpdate w;
	  ReadUpdate(u, w);
	  updates.push_back(w);
	  u->Release();
	}
	list->Release();
      }
      if(code == orcSucceededWithErrors) output += "Windows Update search finished with errors, the list may be incomplete\n";
    }
    result->Release();
  }
  searcher->Release();
  session->Release();
  if(uninit) CoUninitialize();
  return rv;
}

//
// Purpose:
//   Scans and saves the VULNERABILITIES section data in data_dir
//
// Return value:
//   0 on success
//
int RunScan(const AgentConfig &cfg, std::string &output)
{
  time_t start = time(NULL);
  std::vector<WuaUpdate> updates;
  std::string feed;
  int rv = SearchUpdates(updates, feed, output);

  std::string section = "source=wua\n";
  section += "feed=" + feed + "\n";
  section += "scan_etime=" + std::to_string((long long)start) + "\n";
  section += "scan_seconds=" + std::to_string((long long)(time(NULL) - start)) + "\n";
  size_t nsec = 0;
  if(rv != 0) {
    section += "status=failed\n";
    // The error line goes in the report so the dashboard can show it
    std::string last;
    size_t pos = 0;
    while(pos < output.length()) {
      size_t e = output.find('\n', pos);
      if(e == std::string::npos) e = output.length();
      std::string l = output.substr(pos, e - pos);
      if(l.compare(0, 8, "ERROR - ") == 0) last = l;
      pos = e + 1;
    }
    section += last + "\n";
  }
  else {
    std::vector<WuaUpdate> sec, other;
    for(const WuaUpdate &w : updates) (w.security ? sec : other).push_back(w);
    std::sort(sec.begin(), sec.end(), [](const WuaUpdate &a, const WuaUpdate &b) {
	if(SeverityRank(a.severity) != SeverityRank(b.severity)) return SeverityRank(a.severity) < SeverityRank(b.severity);
	return a.id > b.id;
      });
    std::map<std::string, int> counts;
    std::set<std::string> cves;
    for(const WuaUpdate &w : sec) {
      counts[w.severity]++;
      cves.insert(w.cves.begin(), w.cves.end());
    }
    nsec = sec.size();
    section += "status=ok\n";
    section += "critical=" + std::to_string(counts["Critical"]) + " important=" + std::to_string(counts["Important"]) +
      " moderate=" + std::to_string(counts["Moderate"]) + " low=" + std::to_string(counts["Low"]) +
      " unknown=" + std::to_string(counts["Unknown"]) + " total=" + std::to_string(sec.size()) +
      " cves=" + std::to_string(cves.size()) + " other=" + std::to_string(other.size()) + "\n";
    for(const WuaUpdate &w : sec) {
      section += "advisory id=" + w.id + " severity=" + w.severity;
      if(!w.issued.empty()) section += " issued=" + w.issued;
      section += " cves=" + JoinSet(w.cves);
      section += " packages=" + QuoteValue(JoinSet(w.products));
      section += " title=" + QuoteValue(w.title) + "\n";
    }
    for(const WuaUpdate &w : other) {
      section += "other id=" + w.id + " classification=" + QuoteValue(w.classification);
      if(!w.issued.empty()) section += " issued=" + w.issued;
      section += " title=" + QuoteValue(w.title) + "\n";
    }
  }

  if(!CreateDirTree(cfg.data_dir) || !WriteFileText(JoinPath(cfg.data_dir, SCAN_VULN_FILE), section)) {
    output += "ERROR - Could not write " + JoinPath(cfg.data_dir, SCAN_VULN_FILE) + "\n";
    rv = 1;
  }
  output += "Vulnerability scan (wua, " + feed + ") finished in " + std::to_string((long long)(time(NULL) - start)) +
    " seconds" + (rv == 0 ? ", " + std::to_string(nsec) + " security updates and " +
		  std::to_string(updates.size() - nsec) + " other updates to install" : ", failed") + "\n";
  return rv;
}

std::string ScanReportSection(const AgentConfig &cfg)
{
  // The last scan goes in every report, from the schedule (enabled=1)
  // or from "drra_svc.exe scan" run by hand (enabled=0, mode=manual)
  std::string mode = std::string("mode=") + (cfg.scan_enabled ? "auto" : "manual") + "\n";
  std::string data = FileText(JoinPath(cfg.data_dir, SCAN_VULN_FILE));
  if(!data.empty()) return "VULNERABILITIES:\n" + mode + data + "END VULNERABILITIES\n\n";
  if(!cfg.scan_enabled) return "VULNERABILITIES:\n" + mode + "status=disabled\nEND VULNERABILITIES\n\n";
  return "VULNERABILITIES:\n" + mode + "status=pending\nEND VULNERABILITIES\n\n";
}
