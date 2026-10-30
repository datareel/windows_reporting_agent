#ifndef UPLOADS_H
#define UPLOADS_H

// Hook uploads: reports from other sources
//
// An upload hook is one <id>.ini file in [uploads] dir
// (C:\ProgramData\DataReel\ReportingAgent\uploads.d). Any tool puts a
// file in the hook's spool folder, <spool_dir>\<id>\, or an
// administrator runs "drra_svc.exe submit --hook <id> <file>", and the
// service sends it to hook.php on the report server with a signed
// upload. The server side manifest for the same hook ID says how the
// data is read, alerted on and shown (DRSM plugins/uploads/<id>/).
//
//   [upload]
//   id=defender_scan        a-z, 0-9 and _, 1 to 32 characters
//   title=Defender full scan
//   format=csv              a hint for the server, its manifest decides
//   max_bytes=20000000      at most [uploads] max_bytes
//   keep_sent=3             sent files kept in <spool_dir>\<id>\sent\
//
// Both folders are under C:\ProgramData\DataReel\ReportingAgent, which
// the installer limits to SYSTEM and Administrators, so only an
// administrator can add a hook or queue a file.

#include "common.h"

#define UPLOAD_STATE_DIR "uploads"
#define UPLOAD_SEND_NOW_FILE ".send_now"

struct UploadHookConfig {
  std::string id;
  std::string title;
  std::string format;
  long long max_bytes;
  int keep_sent;
  std::string file;        // the hook's INI file
};

bool ValidUploadId(const std::string &id);
std::vector<UploadHookConfig> LoadUploadHooks(const AgentConfig &cfg, std::string &output);
std::string UploadSpoolDir(const AgentConfig &cfg, const std::string &id);

// Copies file into the hook's spool folder; with send_now the running
// service sends it within a few seconds
int SubmitUploadFile(const AgentConfig &cfg, const std::string &hook_id, const std::string &file, bool send_now,
                     std::string &output);

// Sends every spooled file of every upload hook, oldest first. Returns
// 0 when nothing failed.
int SendPendingUploads(const AgentConfig &cfg, std::string &output);

// True (and clears the request) after "submit --now"
bool UploadSendNowRequested(const AgentConfig &cfg);

// UPLOADS report section: state of each upload hook, empty when there are none
std::string UploadReportSection(const AgentConfig &cfg);

#endif
