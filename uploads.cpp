// Hook uploads: reports from other sources, see uploads.h

// C STD lib includes
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

// C++ STD lib includes
#include <algorithm>
#include <string>
#include <vector>

// APP includes
#include "common.h"
#include "agent_auth.h"
#include "uploads.h"

bool ValidUploadId(const std::string &id)
{
  if(id.empty() || id.length() > 32) return false;
  for(char c : id) {
    if(!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_')) return false;
  }
  return true;
}

std::string UploadSpoolDir(const AgentConfig &cfg, const std::string &id)
{
  return JoinPath(cfg.uploads_spool_dir, id);
}

static bool EndsWith(const std::string &s, const std::string &suffix)
{
  return s.length() >= suffix.length() && _stricmp(s.c_str() + s.length() - suffix.length(), suffix.c_str()) == 0;
}

static std::string Lower(std::string s)
{
  for(char &c : s) c = (char)tolower((unsigned char)c);
  return s;
}

static std::string TrimValue(const std::string &s)
{
  size_t b = s.find_first_not_of(" \t\r\n");
  if(b == std::string::npos) return std::string();
  size_t e = s.find_last_not_of(" \t\r\n");
  return s.substr(b, e - b + 1);
}

static bool ReadFileData(const std::string &fname, std::string &data)
{
  data.clear();
  HANDLE h = CreateFileA(fname.c_str(), GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
  if(h == INVALID_HANDLE_VALUE) return false;
  char buffer[65536];
  DWORD n = 0;
  while(ReadFile(h, buffer, sizeof(buffer), &n, NULL) && n > 0) data.append(buffer, n);
  CloseHandle(h);
  return true;
}

static bool WriteFileData(const std::string &fname, const std::string &data)
{
  HANDLE h = CreateFileA(fname.c_str(), GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
  if(h == INVALID_HANDLE_VALUE) return false;
  DWORD n = 0;
  BOOL ok = data.empty() || WriteFile(h, data.data(), (DWORD)data.length(), &n, NULL);
  CloseHandle(h);
  return ok && n == data.length();
}

static bool FileExistsA(const std::string &fname)
{
  DWORD a = GetFileAttributesA(fname.c_str());
  return a != INVALID_FILE_ATTRIBUTES && !(a & FILE_ATTRIBUTE_DIRECTORY);
}

static long long FileTimeToEpoch(const FILETIME &ft)
{
  ULARGE_INTEGER u;
  u.LowPart = ft.dwLowDateTime;
  u.HighPart = ft.dwHighDateTime;
  return (long long)((u.QuadPart - 116444736000000000ULL) / 10000000ULL);
}

struct SpoolFile {
  std::string name;
  long long mtime;
  long long size;
};

// Regular files in dir, sorted by name
static std::vector<SpoolFile> ListFiles(const std::string &dir)
{
  std::vector<SpoolFile> files;
  WIN32_FIND_DATAA fd;
  HANDLE h = FindFirstFileA(JoinPath(dir, "*").c_str(), &fd);
  if(h == INVALID_HANDLE_VALUE) return files;
  do {
    if(fd.dwFileAttributes & (FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_REPARSE_POINT)) continue;
    SpoolFile f;
    f.name = fd.cFileName;
    f.mtime = FileTimeToEpoch(fd.ftLastWriteTime);
    f.size = ((long long)fd.nFileSizeHigh << 32) | fd.nFileSizeLow;
    files.push_back(f);
  } while(FindNextFileA(h, &fd));
  FindClose(h);
  std::sort(files.begin(), files.end(), [](const SpoolFile &a, const SpoolFile &b) { return a.name < b.name; });
  return files;
}

// File names sent to the server: letters, digits, . _ - only
static std::string SafeFileName(const std::string &name)
{
  std::string out;
  for(char c : name) {
    bool ok = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '.' || c == '_' || c == '-';
    out += ok ? c : '_';
  }
  if(out.length() > 100) out = out.substr(out.length() - 100);
  if(out.empty() || out[0] == '.') out = "payload" + out;
  return out;
}

static std::string ContentTypeFor(const std::string &name)
{
  if(EndsWith(name, ".xml") || EndsWith(name, ".nessus")) return "application/xml";
  if(EndsWith(name, ".json")) return "application/json";
  if(EndsWith(name, ".csv")) return "text/csv";
  if(EndsWith(name, ".html") || EndsWith(name, ".htm")) return "text/html";
  return "text/plain";
}

static std::string IsoTime(long long t)
{
  char buf[32];
  struct tm tmv;
  time_t tt = (time_t)t;
  gmtime_s(&tmv, &tt);
  strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%SZ", &tmv);
  return buf;
}

static std::string IniString(const std::string &fname, const char *key)
{
  char buffer[1024];
  GetPrivateProfileStringA("upload", key, "", buffer, sizeof(buffer), fname.c_str());
  std::string val = TrimValue(buffer);
  // Comments may follow a value
  size_t sc = val.find(" ;");
  if(sc != std::string::npos) val = TrimValue(val.substr(0, sc));
  return val;
}

static bool ReadUploadFile(const AgentConfig &cfg, const std::string &fname, UploadHookConfig &hook, std::string &output)
{
  hook.file = fname;
  hook.id = IniString(fname, "id");
  hook.title = IniString(fname, "title");
  hook.format = Lower(IniString(fname, "format"));
  hook.max_bytes = cfg.uploads_max_bytes;
  hook.keep_sent = 3;
  long long n = _atoi64(IniString(fname, "max_bytes").c_str());
  if(n > 0 && n < hook.max_bytes) hook.max_bytes = n;
  std::string keep = IniString(fname, "keep_sent");
  if(!keep.empty() && atoi(keep.c_str()) >= 0) hook.keep_sent = atoi(keep.c_str());
  if(!ValidUploadId(hook.id)) {
    output += "ERROR - Upload hook file " + fname + ": id must be 1 to 32 of a-z, 0-9 and _\n";
    return false;
  }
  if(hook.title.empty()) hook.title = hook.id;
  for(char &c : hook.format) if(!(isalnum((unsigned char)c) || c == '-')) c = '-';
  for(char &c : hook.title) if(c == '\r' || c == '\n') c = ' ';
  return true;
}

std::vector<UploadHookConfig> LoadUploadHooks(const AgentConfig &cfg, std::string &output)
{
  std::vector<UploadHookConfig> hooks;
  if(!cfg.uploads_enabled) return hooks;
  for(const SpoolFile &f : ListFiles(cfg.uploads_dir)) {
    if(!EndsWith(f.name, ".ini") || f.name[0] == '.') continue;
    UploadHookConfig hook;
    if(!ReadUploadFile(cfg, JoinPath(cfg.uploads_dir, f.name), hook, output)) continue;
    bool dup = false;
    for(const UploadHookConfig &h : hooks) if(h.id == hook.id) dup = true;
    if(dup) {
      output += "ERROR - Upload hook " + hook.id + " is defined twice, " + hook.file + " skipped\n";
      continue;
    }
    hooks.push_back(hook);
  }
  return hooks;
}

static bool FindUploadHook(const AgentConfig &cfg, const std::string &id, UploadHookConfig &hook, std::string &output)
{
  for(const UploadHookConfig &h : LoadUploadHooks(cfg, output)) {
    if(h.id == id) {
      hook = h;
      return true;
    }
  }
  return false;
}

int SubmitUploadFile(const AgentConfig &cfg, const std::string &hook_id, const std::string &file, bool send_now,
		     std::string &output)
{
  UploadHookConfig hook;
  if(!ValidUploadId(hook_id) || !FindUploadHook(cfg, hook_id, hook, output)) {
    output += "ERROR - No upload hook " + hook_id + " in " + cfg.uploads_dir + "\n";
    return -1;
  }
  std::string data;
  if(!ReadFileData(file, data)) {
    output += "ERROR - Cannot read " + file + ": " + GetLastErrorAsString() + "\n";
    return -1;
  }
  if((long long)data.length() > hook.max_bytes) {
    output += "ERROR - " + file + " is " + std::to_string(data.length()) + " bytes, upload hook " + hook_id +
      " takes at most " + std::to_string(hook.max_bytes) + "\n";
    return -1;
  }
  std::string dir = UploadSpoolDir(cfg, hook_id);
  if(!CreateDirTree(dir)) {
    output += "ERROR - Cannot create " + dir + "\n";
    return -1;
  }
  size_t slash = file.find_last_of("\\/");
  std::string base = SafeFileName(slash == std::string::npos ? file : file.substr(slash + 1));
  std::string name = std::to_string((long long)time(NULL)) + "_" + std::to_string((long long)GetCurrentProcessId()) + "_" + base;
  std::string meta = "SOURCE: submit\nFILE_NAME: " + base + "\n";
  // The meta file first, then the payload by rename, so the service never
  // sees a payload without its meta file or a partial payload
  std::string tmp = JoinPath(dir, "." + name + ".tmp");
  if(!WriteFileData(JoinPath(dir, "." + name + ".meta"), meta) || !WriteFileData(tmp, data) ||
     !MoveFileExA(tmp.c_str(), JoinPath(dir, name).c_str(), MOVEFILE_REPLACE_EXISTING)) {
    DeleteFileA(tmp.c_str());
    output += "ERROR - Cannot write to " + dir + ": " + GetLastErrorAsString() + "\n";
    return -1;
  }
  output += "Queued " + file + " for upload hook " + hook_id + " as " + JoinPath(dir, name) + "\n";
  if(send_now) {
    std::string flag = JoinPath(cfg.uploads_spool_dir, UPLOAD_SEND_NOW_FILE);
    if(WriteFileData(flag, "")) output += "The service sends it within a few seconds\n";
    else output += "It goes out with the next report (cannot write " + flag + ")\n";
  }
  else {
    output += "It goes out with the next report\n";
  }
  return 0;
}

bool UploadSendNowRequested(const AgentConfig &cfg)
{
  std::string flag = JoinPath(cfg.uploads_spool_dir, UPLOAD_SEND_NOW_FILE);
  if(!FileExistsA(flag)) return false;
  DeleteFileA(flag.c_str());
  return true;
}

// Payload files waiting in a spool folder, oldest first. Names starting
// with "." (meta and temporary files) are skipped. Files changed in the
// last two seconds may still be written and wait, except files from
// submit (they have a meta file and arrive complete by rename).
static std::vector<SpoolFile> PendingFiles(const std::string &dir)
{
  std::vector<SpoolFile> files;
  long long now = (long long)time(NULL);
  for(const SpoolFile &f : ListFiles(dir)) {
    if(f.name[0] == '.' || EndsWith(f.name, ".tmp")) continue;
    if(now - f.mtime < 2 && !FileExistsA(JoinPath(dir, "." + f.name + ".meta"))) continue;
    files.push_back(f);
  }
  std::stable_sort(files.begin(), files.end(), [](const SpoolFile &a, const SpoolFile &b) { return a.mtime < b.mtime; });
  return files;
}

static void MoveSpoolFile(const std::string &dir, const std::string &name, const std::string &to, int keep)
{
  std::string meta = JoinPath(dir, "." + name + ".meta");
  std::string from = JoinPath(dir, name);
  std::string tdir = JoinPath(dir, to);
  DeleteFileA(meta.c_str());
  if(keep <= 0 || !CreateDirTree(tdir)) {
    DeleteFileA(from.c_str());
    return;
  }
  std::string dest = JoinPath(tdir, name);
  if(!MoveFileExA(from.c_str(), dest.c_str(), MOVEFILE_REPLACE_EXISTING)) {
    DeleteFileA(from.c_str());
    return;
  }
  // Keep the newest "keep" files
  std::vector<SpoolFile> old = ListFiles(tdir);
  std::stable_sort(old.begin(), old.end(), [](const SpoolFile &a, const SpoolFile &b) { return a.mtime < b.mtime; });
  for(size_t i = 0; i + keep < old.size(); i++) DeleteFileA(JoinPath(tdir, old[i].name).c_str());
}

static std::string StateFile(const AgentConfig &cfg, const std::string &id)
{
  return JoinPath(JoinPath(cfg.data_dir, UPLOAD_STATE_DIR), id + ".state");
}

static std::string StateValue(const std::string &state, const std::string &key)
{
  size_t pos = 0;
  while(pos < state.length()) {
    size_t e = state.find('\n', pos);
    std::string line = state.substr(pos, e == std::string::npos ? std::string::npos : e - pos);
    if(line.compare(0, key.length() + 1, key + "=") == 0) return TrimValue(line.substr(key.length() + 1));
    if(e == std::string::npos) break;
    pos = e + 1;
  }
  return std::string();
}

static std::string Quote(const std::string &s)
{
  std::string q = "\"";
  for(char c : s) {
    if(c == '"' || c == '\\') q += '\\';
    if(c == '\n' || c == '\r' || c == '\t') c = ' ';
    q += c;
  }
  return q + "\"";
}

static void WriteUploadState(const AgentConfig &cfg, const std::string &id, const std::string &status,
			     long long bytes, const std::string &message)
{
  std::string old;
  ReadFileData(StateFile(cfg, id), old);
  std::string last_sent = StateValue(old, "last_sent");
  if(last_sent.empty()) last_sent = "0";
  if(status == "ok") last_sent = std::to_string((long long)time(NULL));
  std::string state = "status=" + status + "\nlast_sent=" + last_sent + "\nlast_try=" + std::to_string((long long)time(NULL)) +
    "\nbytes=" + std::to_string(bytes) + "\nmessage=" + Quote(message) + "\n";
  CreateDirTree(JoinPath(cfg.data_dir, UPLOAD_STATE_DIR));
  WriteFileData(StateFile(cfg, id), state);
}

static std::string MetaValue(const std::string &meta, const std::string &key)
{
  size_t pos = 0;
  std::string prefix = key + ": ";
  while(pos < meta.length()) {
    size_t e = meta.find('\n', pos);
    std::string line = meta.substr(pos, e == std::string::npos ? std::string::npos : e - pos);
    if(line.compare(0, prefix.length(), prefix) == 0) return TrimValue(line.substr(prefix.length()));
    if(e == std::string::npos) break;
    pos = e + 1;
  }
  return std::string();
}

int SendPendingUploads(const AgentConfig &cfg, std::string &output)
{
  if(!cfg.uploads_enabled) return 0;
  std::vector<UploadHookConfig> hooks = LoadUploadHooks(cfg, output);
  if(hooks.empty()) return 0;
  bool https = std::find(cfg.delivery_methods.begin(), cfg.delivery_methods.end(), "https") != cfg.delivery_methods.end();
  std::string hostname = GetAgentHostname();
  int error_level = 0;

  for(const UploadHookConfig &hook : hooks) {
    std::string dir = UploadSpoolDir(cfg, hook.id);
    CreateDirTree(dir);
    std::vector<SpoolFile> files = PendingFiles(dir);
    if(files.empty()) continue;
    if(!https) {
      output += "WARNING - Upload hook " + hook.id + " has " + std::to_string(files.size()) +
	" file(s) waiting: uploads are only sent with the https delivery method\n";
      WriteUploadState(cfg, hook.id, "waiting", 0, "uploads need https delivery");
      continue;
    }
    for(const SpoolFile &f : files) {
      std::string path = JoinPath(dir, f.name);
      if(f.size > hook.max_bytes) {
	output += "ERROR - " + path + " is " + std::to_string(f.size) + " bytes, upload hook " + hook.id +
	  " takes at most " + std::to_string(hook.max_bytes) + ", moved to failed\\\n";
	MoveSpoolFile(dir, f.name, "failed", 10);
	WriteUploadState(cfg, hook.id, "error", f.size, "file too large");
	error_level = -1;
	continue;
      }
      FormFile payload;
      if(!ReadFileData(path, payload.content)) {
	output += "ERROR - Cannot read " + path + "\n";
	error_level = -1;
	break;
      }
      std::string meta;
      ReadFileData(JoinPath(dir, "." + f.name + ".meta"), meta);
      std::string source = MetaValue(meta, "SOURCE") == "submit" ? "submit" : "spool";
      std::string file_name = SafeFileName(MetaValue(meta, "FILE_NAME").empty() ? f.name : MetaValue(meta, "FILE_NAME"));
      payload.field = "payload";
      payload.file_name = file_name;
      payload.content_type = ContentTypeFor(file_name);

      std::string envelope;
      envelope += "HOOK_ID: " + hook.id + "\n";
      envelope += "TITLE: " + hook.title + "\n";
      envelope += "SUBJECT: " + hostname + "\n";
      envelope += "FORMAT: " + hook.format + "\n";
      envelope += "CONTENT_TYPE: " + payload.content_type + "\n";
      envelope += "FILE_NAME: " + file_name + "\n";
      envelope += "COLLECTED: " + IsoTime(f.mtime) + "\n";
      envelope += "COLLECTED_ETIME: " + std::to_string(f.mtime) + "\n";
      envelope += "SOURCE: " + source + "\n";
      envelope += "AGENT_VERSION: " AGENT_VERSION "\n";
      envelope += "SHA256: " + Sha256Hex(payload.content) + "\n";

      std::string status;
      int rv = UploadHook(cfg, hook.id, hostname, envelope, payload, status, output);
      if(rv == 0) {
	MoveSpoolFile(dir, f.name, "sent", hook.keep_sent);
	WriteUploadState(cfg, hook.id, "ok", (long long)payload.content.length(), file_name);
	continue;
      }
      WriteUploadState(cfg, hook.id, rv > 0 ? "waiting" : "error", (long long)payload.content.length(), status);
      if(rv < 0) error_level = -1;
      // The rest of this hook's files wait for the next cycle, in order
      break;
    }
  }
  return error_level;
}

std::string UploadReportSection(const AgentConfig &cfg)
{
  std::string ignore;
  std::vector<UploadHookConfig> hooks = LoadUploadHooks(cfg, ignore);
  if(hooks.empty()) return std::string();
  std::string body;
  for(const UploadHookConfig &hook : hooks) {
    std::string state;
    ReadFileData(StateFile(cfg, hook.id), state);
    std::string status = StateValue(state, "status");
    std::string last_sent = StateValue(state, "last_sent");
    std::string message = StateValue(state, "message");
    if(status.empty()) status = "none";
    if(last_sent.empty()) last_sent = "0";
    body += hook.id + "_title=" + Quote(hook.title) + "\n";
    body += hook.id + "_status=" + status + "\n";
    body += hook.id + "_last_sent=" + last_sent + "\n";
    body += hook.id + "_pending=" + std::to_string(PendingFiles(UploadSpoolDir(cfg, hook.id)).size()) + "\n";
    if(!message.empty() && status != "ok") body += hook.id + "_message=" + message + "\n";
  }
  return "UPLOADS:\n" + body + "END UPLOADS\n\n";
}
