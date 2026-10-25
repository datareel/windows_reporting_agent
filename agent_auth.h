#ifndef AGENT_AUTH_H
#define AGENT_AUTH_H

// Agent identity and signed uploads
//
// Each agent has an RSA key in the machine key store (Microsoft Software
// Key Storage Provider, not exportable) and a self-signed certificate for
// that key. The agent registers the certificate with register.php, and
// once the server approves it, every upload is signed with the key.
// upload.php checks the signature against the registered certificate.

#include "common.h"
#include <wincrypt.h>
#include <bcrypt.h>
#include <ncrypt.h>
#include <utility>

#define AGENT_KEY_NAME L"WindowsReportAgent"
#define AGENT_KEY_BITS 3072
#define AGENT_CERT_FILE "agent_cert.pem"
#define AGENT_STATE_FILE "registration_status.txt"

struct AgentIdentity {
    std::string hostname;           // lower case computer name
    std::string cert_pem;           // self-signed certificate, PEM
    std::string key_fingerprint;    // SHA-256 of the public key (SPKI DER), hex
    NCRYPT_KEY_HANDLE hKey;
};

typedef std::vector<std::pair<std::string, std::string> > FormFields;

std::string GetAgentHostname();
bool CreateDirTree(const std::string &dir);
std::string Sha256Hex(const std::string &data);
std::string RandomHex(int nbytes);
int LoadOrCreateAgentIdentity(const AgentConfig &cfg, AgentIdentity &id, std::string &output);
void FreeAgentIdentity(AgentIdentity &id);
int SignString(const AgentIdentity &id, const std::string &data, std::string &sig_b64, std::string &output);
int HttpPostMultipart(const AgentConfig &cfg, const std::string &path, const FormFields &fields,
                      const std::string &file_field, const std::string &file_name, const std::string &file_content,
                      DWORD &http_status, std::string &response, std::string &error);
std::string ParseStatus(const std::string &response);
std::string ReadRegistrationState(const AgentConfig &cfg);
void WriteRegistrationState(const AgentConfig &cfg, const std::string &state);
int RegisterAgent(const AgentConfig &cfg, const AgentIdentity &id, std::string &status, std::string &output);

#endif
