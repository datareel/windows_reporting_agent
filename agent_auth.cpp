// Windows includes that must come first
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

// C STD lib includes
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>

// Windows includes
#include <wininet.h>

// C++ STD lib includes
#include <string>
#include <cctype>
#include <ctime>
#include <vector>

// APP includes
#include "common.h"
#include "agent_auth.h"

// Ensure the library is linked
#pragma comment(lib, "Wininet.lib")
#pragma comment(lib, "crypt32.lib")
#pragma comment(lib, "bcrypt.lib")
#pragma comment(lib, "ncrypt.lib")

static std::string ToHex(const BYTE *data, DWORD len)
{
  static const char digits[] = "0123456789abcdef";
  std::string hex;
  hex.reserve(len * 2);
  for (DWORD i = 0; i < len; i++) {
    hex += digits[data[i] >> 4];
    hex += digits[data[i] & 0x0f];
  }
  return hex;
}

static std::string StatusToString(SECURITY_STATUS status)
{
  char buf[32];
  snprintf(buf, sizeof(buf), "0x%08lx", (unsigned long)status);
  return std::string(buf);
}

std::string GetAgentHostname()
{
  char buffer[MAX_COMPUTERNAME_LENGTH + 1];
  DWORD size = MAX_COMPUTERNAME_LENGTH + 1;
  if(!GetComputerNameA(buffer, &size)) return std::string();
  std::string hostname = buffer;
  for (char &c : hostname) {
    c = std::tolower((unsigned char)c);
  }
  return hostname;
}

static bool Sha256Raw(const std::string &data, BYTE hash[32])
{
  BCRYPT_ALG_HANDLE hAlg = NULL;
  if(!BCRYPT_SUCCESS(BCryptOpenAlgorithmProvider(&hAlg, BCRYPT_SHA256_ALGORITHM, NULL, 0))) return false;
  NTSTATUS status = BCryptHash(hAlg, NULL, 0, (PUCHAR)data.data(), (ULONG)data.length(), hash, 32);
  BCryptCloseAlgorithmProvider(hAlg, 0);
  return BCRYPT_SUCCESS(status);
}

std::string Sha256Hex(const std::string &data)
{
  BYTE hash[32];
  if(!Sha256Raw(data, hash)) return std::string();
  return ToHex(hash, 32);
}

std::string RandomHex(int nbytes)
{
  std::vector<BYTE> buf(nbytes);
  if(!BCRYPT_SUCCESS(BCryptGenRandom(NULL, buf.data(), (ULONG)nbytes, BCRYPT_USE_SYSTEM_PREFERRED_RNG))) {
    return std::string();
  }
  return ToHex(buf.data(), (DWORD)nbytes);
}

// Create a directory and any missing parent directories
bool CreateDirTree(const std::string &dir)
{
  if(dir.empty()) return false;
  if(GetFileAttributesA(dir.c_str()) != INVALID_FILE_ATTRIBUTES) return true;
  size_t pos = dir.find_last_of("\\/");
  if(pos != std::string::npos && pos > 2) {
    CreateDirTree(dir.substr(0, pos));
  }
  return CreateDirectoryA(dir.c_str(), NULL) || GetLastError() == ERROR_ALREADY_EXISTS;
}

static bool ReadWholeFile(const std::string &fname, std::string &content)
{
  content.clear();
  HANDLE hFile = CreateFileA(fname.c_str(), GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
  if(hFile == INVALID_HANDLE_VALUE) return false;
  char buffer[4096];
  DWORD bytesRead = 0;
  while(ReadFile(hFile, buffer, sizeof(buffer), &bytesRead, NULL) && bytesRead > 0) {
    content.append(buffer, bytesRead);
  }
  CloseHandle(hFile);
  return true;
}

static bool WriteWholeFile(const std::string &fname, const std::string &content)
{
  HANDLE hFile = CreateFileA(fname.c_str(), GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
  if(hFile == INVALID_HANDLE_VALUE) return false;
  DWORD bytesWritten = 0;
  BOOL result = WriteFile(hFile, content.data(), (DWORD)content.length(), &bytesWritten, NULL);
  CloseHandle(hFile);
  return result && bytesWritten == content.length();
}

// DER encoding of a SubjectPublicKeyInfo, used for the key fingerprint
static std::string EncodePublicKeyInfo(const CERT_PUBLIC_KEY_INFO *info)
{
  BYTE *pb = NULL;
  DWORD cb = 0;
  if(!CryptEncodeObjectEx(X509_ASN_ENCODING, X509_PUBLIC_KEY_INFO, info, CRYPT_ENCODE_ALLOC_FLAG, NULL, &pb, &cb)) {
    return std::string();
  }
  std::string der((char *)pb, cb);
  LocalFree(pb);
  return der;
}

static std::string KeyPublicKeyInfo(NCRYPT_KEY_HANDLE hKey)
{
  DWORD cb = 0;
  if(!CryptExportPublicKeyInfo((HCRYPTPROV_OR_NCRYPT_KEY_HANDLE)hKey, 0, X509_ASN_ENCODING, NULL, &cb)) {
    return std::string();
  }
  std::vector<BYTE> buf(cb);
  CERT_PUBLIC_KEY_INFO *info = (CERT_PUBLIC_KEY_INFO *)buf.data();
  if(!CryptExportPublicKeyInfo((HCRYPTPROV_OR_NCRYPT_KEY_HANDLE)hKey, 0, X509_ASN_ENCODING, info, &cb)) {
    return std::string();
  }
  return EncodePublicKeyInfo(info);
}

static std::string CertToPem(PCCERT_CONTEXT pCert)
{
  DWORD cch = 0;
  if(!CryptBinaryToStringA(pCert->pbCertEncoded, pCert->cbCertEncoded, CRYPT_STRING_BASE64HEADER, NULL, &cch)) {
    return std::string();
  }
  std::vector<char> buf(cch);
  if(!CryptBinaryToStringA(pCert->pbCertEncoded, pCert->cbCertEncoded, CRYPT_STRING_BASE64HEADER, buf.data(), &cch)) {
    return std::string();
  }
  // Use plain \n line endings so the agent and server hash the same bytes
  std::string pem;
  for (DWORD i = 0; i < cch && buf[i] != '\0'; i++) {
    if(buf[i] != '\r') pem += buf[i];
  }
  return pem;
}

static PCCERT_CONTEXT PemToCert(const std::string &pem)
{
  DWORD cb = 0;
  if(!CryptStringToBinaryA(pem.c_str(), (DWORD)pem.length(), CRYPT_STRING_BASE64HEADER, NULL, &cb, NULL, NULL)) {
    return NULL;
  }
  std::vector<BYTE> der(cb);
  if(!CryptStringToBinaryA(pem.c_str(), (DWORD)pem.length(), CRYPT_STRING_BASE64HEADER, der.data(), &cb, NULL, NULL)) {
    return NULL;
  }
  return CertCreateCertificateContext(X509_ASN_ENCODING, der.data(), cb);
}

static PCCERT_CONTEXT CreateAgentCert(NCRYPT_KEY_HANDLE hKey, const std::string &hostname, std::string &output)
{
  std::string subject = "CN=" + hostname;
  DWORD cb = 0;
  if(!CertStrToNameA(X509_ASN_ENCODING, subject.c_str(), CERT_X500_NAME_STR, NULL, NULL, &cb, NULL)) {
    output += "ERROR - CertStrToName failed: " + GetLastErrorAsString() + "\n";
    return NULL;
  }
  std::vector<BYTE> name(cb);
  if(!CertStrToNameA(X509_ASN_ENCODING, subject.c_str(), CERT_X500_NAME_STR, NULL, name.data(), &cb, NULL)) {
    output += "ERROR - CertStrToName failed: " + GetLastErrorAsString() + "\n";
    return NULL;
  }
  CERT_NAME_BLOB subject_blob = { cb, name.data() };

  CRYPT_ALGORITHM_IDENTIFIER sig_alg;
  memset(&sig_alg, 0, sizeof(sig_alg));
  sig_alg.pszObjId = (LPSTR)szOID_RSA_SHA256RSA;

  SYSTEMTIME start, end;
  GetSystemTime(&start);
  end = start;
  end.wYear += 20;
  if(end.wMonth == 2 && end.wDay == 29) end.wDay = 28;

  PCCERT_CONTEXT pCert = CertCreateSelfSignCertificate((HCRYPTPROV_OR_NCRYPT_KEY_HANDLE)hKey, &subject_blob, 0, NULL,
						       &sig_alg, &start, &end, NULL);
  if(!pCert) {
    output += "ERROR - CertCreateSelfSignCertificate failed: " + GetLastErrorAsString() + "\n";
  }
  return pCert;
}

int LoadOrCreateAgentIdentity(const AgentConfig &cfg, AgentIdentity &id, std::string &output)
{
  id.hostname = GetAgentHostname();
  id.cert_pem.clear();
  id.key_fingerprint.clear();
  id.hKey = 0;

  if(id.hostname.empty()) {
    output += "ERROR - Error getting computer name\n";
    return -1;
  }
  if(!CreateDirTree(cfg.data_dir)) {
    output += "ERROR - Could not create data directory " + cfg.data_dir + "\n";
    return -1;
  }

  NCRYPT_PROV_HANDLE hProv = 0;
  SECURITY_STATUS status = NCryptOpenStorageProvider(&hProv, MS_KEY_STORAGE_PROVIDER, 0);
  if(status != ERROR_SUCCESS) {
    output += "ERROR - Could not open key storage provider " + StatusToString(status) + "\n";
    return -1;
  }

  // Open the agent key, or create it on the first run. The key is a
  // machine key that cannot be exported.
  NCRYPT_KEY_HANDLE hKey = 0;
  bool new_key = false;
  status = NCryptOpenKey(hProv, &hKey, AGENT_KEY_NAME, 0, NCRYPT_MACHINE_KEY_FLAG);
  if(status == NTE_BAD_KEYSET) {
    // A computer upgraded from 3.2 or older keeps its old key, so its
    // registration with the report server stays valid
    status = NCryptOpenKey(hProv, &hKey, AGENT_LEGACY_KEY_NAME, 0, NCRYPT_MACHINE_KEY_FLAG);
  }
  if(status == NTE_BAD_KEYSET) {
    status = NCryptCreatePersistedKey(hProv, &hKey, BCRYPT_RSA_ALGORITHM, AGENT_KEY_NAME, 0, NCRYPT_MACHINE_KEY_FLAG);
    if(status == ERROR_SUCCESS) {
      DWORD bits = AGENT_KEY_BITS;
      DWORD export_policy = 0;
      NCryptSetProperty(hKey, NCRYPT_LENGTH_PROPERTY, (PBYTE)&bits, sizeof(bits), 0);
      NCryptSetProperty(hKey, NCRYPT_EXPORT_POLICY_PROPERTY, (PBYTE)&export_policy, sizeof(export_policy), 0);
      status = NCryptFinalizeKey(hKey, 0);
      if(status != ERROR_SUCCESS) {
	NCryptDeleteKey(hKey, 0);
	hKey = 0;
      }
    }
    new_key = true;
  }
  NCryptFreeObject(hProv);
  if(status != ERROR_SUCCESS || hKey == 0) {
    output += "ERROR - Could not open or create the agent key " + StatusToString(status) + "\n";
    return -1;
  }
  if(new_key) output += "Created new agent key\n";

  std::string key_spki = KeyPublicKeyInfo(hKey);
  if(key_spki.empty()) {
    output += "ERROR - Could not read the agent public key: " + GetLastErrorAsString() + "\n";
    NCryptFreeObject(hKey);
    return -1;
  }

  // Use the saved certificate if it matches the key and the hostname,
  // otherwise make a new one
  std::string cert_fname = JoinPath(cfg.data_dir, AGENT_CERT_FILE);
  std::string pem;
  PCCERT_CONTEXT pCert = NULL;
  if(ReadWholeFile(cert_fname, pem)) {
    pCert = PemToCert(pem);
    if(pCert) {
      char cn[256];
      CertGetNameStringA(pCert, CERT_NAME_ATTR_TYPE, 0, (void *)szOID_COMMON_NAME, cn, sizeof(cn));
      if(EncodePublicKeyInfo(&pCert->pCertInfo->SubjectPublicKeyInfo) != key_spki || id.hostname != cn) {
	CertFreeCertificateContext(pCert);
	pCert = NULL;
      }
    }
  }
  if(!pCert) {
    pCert = CreateAgentCert(hKey, id.hostname, output);
    if(!pCert) {
      NCryptFreeObject(hKey);
      return -1;
    }
    pem = CertToPem(pCert);
    if(!WriteWholeFile(cert_fname, pem)) {
      output += "WARNING - Could not save the agent certificate to " + cert_fname + "\n";
    }
    output += "Created new agent certificate " + cert_fname + "\n";
  }
  CertFreeCertificateContext(pCert);

  id.cert_pem = pem;
  id.key_fingerprint = Sha256Hex(key_spki);
  id.hKey = hKey;
  return 0;
}

void FreeAgentIdentity(AgentIdentity &id)
{
  if(id.hKey) NCryptFreeObject(id.hKey);
  id.hKey = 0;
}

int SignString(const AgentIdentity &id, const std::string &data, std::string &sig_b64, std::string &output)
{
  sig_b64.clear();
  BYTE hash[32];
  if(!Sha256Raw(data, hash)) {
    output += "ERROR - Could not hash data for signing\n";
    return -1;
  }

  // RSA PKCS#1 v1.5 with SHA-256, checked in PHP with openssl_verify()
  BCRYPT_PKCS1_PADDING_INFO pad = { BCRYPT_SHA256_ALGORITHM };
  DWORD cb = 0;
  SECURITY_STATUS status = NCryptSignHash(id.hKey, &pad, hash, sizeof(hash), NULL, 0, &cb, BCRYPT_PAD_PKCS1);
  if(status != ERROR_SUCCESS) {
    output += "ERROR - Could not sign data " + StatusToString(status) + "\n";
    return -1;
  }
  std::vector<BYTE> sig(cb);
  status = NCryptSignHash(id.hKey, &pad, hash, sizeof(hash), sig.data(), cb, &cb, BCRYPT_PAD_PKCS1);
  if(status != ERROR_SUCCESS) {
    output += "ERROR - Could not sign data " + StatusToString(status) + "\n";
    return -1;
  }

  DWORD cch = 0;
  CryptBinaryToStringA(sig.data(), cb, CRYPT_STRING_BASE64 | CRYPT_STRING_NOCRLF, NULL, &cch);
  std::vector<char> b64(cch);
  if(!CryptBinaryToStringA(sig.data(), cb, CRYPT_STRING_BASE64 | CRYPT_STRING_NOCRLF, b64.data(), &cch)) {
    output += "ERROR - Could not encode signature\n";
    return -1;
  }
  sig_b64 = b64.data();
  return 0;
}

int HttpPostMultipart(const AgentConfig &cfg, const std::string &path, const FormFields &fields,
		      const std::string &file_field, const std::string &file_name, const std::string &file_content,
		      DWORD &http_status, std::string &response, std::string &error)
{
  http_status = 0;
  response.clear();
  error.clear();

  // 1. Initialize WinINet
  std::string user_agent = "DataReelReportAgent/" AGENT_VERSION;
  HINTERNET hSession = InternetOpenA(user_agent.c_str(), INTERNET_OPEN_TYPE_DIRECT, NULL, NULL, 0);
  if (!hSession) {
    error = "ERROR - Could not initialize WinINet session";
    return -1;
  }

  // 2. Connect to the server
  HINTERNET hConnect = InternetConnectA(hSession, cfg.server_host.c_str(), (INTERNET_PORT)cfg.server_port, NULL, NULL, INTERNET_SERVICE_HTTP, 0, 0);
  if (!hConnect) {
    InternetCloseHandle(hSession);
    error = "ERROR - Could not connect to server " + cfg.server_host;
    return -1;
  }

  // 3. Create the request
  const char* acceptTypes[] = { "*/*", NULL };
  DWORD request_flags = INTERNET_FLAG_RELOAD | INTERNET_FLAG_NO_CACHE_WRITE | INTERNET_FLAG_NO_COOKIES;
  if (cfg.use_https) request_flags |= INTERNET_FLAG_SECURE;
  HINTERNET hRequest = HttpOpenRequestA(hConnect, "POST", path.c_str(), "HTTP/1.1", NULL, acceptTypes, request_flags, 0);
  if (!hRequest) {
    error = "ERROR - Could not create request: " + GetLastErrorAsString();
    InternetCloseHandle(hConnect);
    InternetCloseHandle(hSession);
    return -1;
  }

  // 06/04/2026: Fix for error code: 12057 cannot validate the SSL/TLS certificate's revocation status.
  // This is usually caused by network security settings or blocked access to a Certificate Revocation List (CRL) server.
  // Set ignore_cert_revocation = 0 in the INI file to enforce revocation checks.
  if (cfg.use_https && cfg.ignore_cert_revocation) {
    DWORD dwFlags = 0;
    DWORD dwBuffLen = sizeof(dwFlags);
    InternetQueryOption(hRequest, INTERNET_OPTION_SECURITY_FLAGS, &dwFlags, &dwBuffLen);
    dwFlags |= SECURITY_FLAG_IGNORE_REVOCATION;
    InternetSetOption(hRequest, INTERNET_OPTION_SECURITY_FLAGS, &dwFlags, sizeof(dwFlags));
  }

  // 4. Construct Multipart/Form-Data body
  std::string boundary = "----DataReelReportAgent" + RandomHex(16);
  std::string headers = "Content-Type: multipart/form-data; boundary=" + boundary;
  std::string body;
  for (const auto &field : fields) {
    body += "--" + boundary + "\r\n";
    body += "Content-Disposition: form-data; name=\"" + field.first + "\"\r\n\r\n";
    body += field.second + "\r\n";
  }
  if (!file_field.empty()) {
    body += "--" + boundary + "\r\n";
    body += "Content-Disposition: form-data; name=\"" + file_field + "\"; filename=\"" + file_name + "\"\r\n";
    body += "Content-Type: text/plain\r\n\r\n";
    body += file_content + "\r\n";
  }
  body += "--" + boundary + "--\r\n";

  // 5. Send the request
  int error_level = 0;
  BOOL sent = HttpSendRequestA(hRequest, headers.c_str(), (DWORD)headers.length(), (LPVOID)body.data(), (DWORD)body.length());
  if (sent) {
    DWORD len = sizeof(http_status);
    HttpQueryInfoA(hRequest, HTTP_QUERY_STATUS_CODE | HTTP_QUERY_FLAG_NUMBER, &http_status, &len, NULL);
    char buffer[1024];
    DWORD bytesRead;
    while (InternetReadFile(hRequest, buffer, sizeof(buffer) - 1, &bytesRead) && bytesRead > 0) {
      buffer[bytesRead] = '\0';
      response += buffer;
    }
  }
  else {
    error = "ERROR - Request to " + cfg.server_host + path + " failed. Error message: " + GetLastErrorAsString();
    error_level = -1;
  }

  // Clean up
  InternetCloseHandle(hRequest);
  InternetCloseHandle(hConnect);
  InternetCloseHandle(hSession);

  return error_level;
}

// The server answers with a "STATUS: <word>" line, such as STATUS: approved
std::string ParseStatus(const std::string &response)
{
  size_t pos = response.find("STATUS: ");
  if(pos == std::string::npos) return std::string();
  pos += 8;
  std::string status;
  while(pos < response.length() && (std::isalnum((unsigned char)response[pos]) || response[pos] == '_')) {
    status += (char)std::tolower((unsigned char)response[pos]);
    pos++;
  }
  return status;
}

std::string ReadRegistrationState(const AgentConfig &cfg)
{
  std::string state;
  ReadWholeFile(JoinPath(cfg.data_dir, AGENT_STATE_FILE), state);
  while(!state.empty() && (state.back() == '\r' || state.back() == '\n' || state.back() == ' ')) {
    state.pop_back();
  }
  return state;
}

void WriteRegistrationState(const AgentConfig &cfg, const std::string &state)
{
  WriteWholeFile(JoinPath(cfg.data_dir, AGENT_STATE_FILE), state + "\n");
}

int RegisterAgent(const AgentConfig &cfg, const AgentIdentity &id, std::string &status, std::string &output)
{
  status.clear();
  std::string timestamp = std::to_string((long long)std::time(nullptr));
  std::string nonce = RandomHex(16);

  // Signing the request proves the agent holds the key for this certificate
  std::string to_sign = "register\n" + id.hostname + "\n" + timestamp + "\n" + nonce + "\n" + Sha256Hex(id.cert_pem);
  std::string signature;
  if(SignString(id, to_sign, signature, output) != 0) return -1;

  FormFields fields;
  fields.push_back(std::make_pair("hostname", id.hostname));
  fields.push_back(std::make_pair("timestamp", timestamp));
  fields.push_back(std::make_pair("nonce", nonce));
  fields.push_back(std::make_pair("cert", id.cert_pem));
  fields.push_back(std::make_pair("enrollment_token", cfg.enrollment_token));
  fields.push_back(std::make_pair("signature", signature));

  DWORD http_status = 0;
  std::string response, error;
  if(HttpPostMultipart(cfg, cfg.register_path, fields, "", "", "", http_status, response, error) != 0) {
    output += error + "\n";
    return -1;
  }

  status = ParseStatus(response);
  if(status.empty()) {
    output += "ERROR - Unexpected answer from " + cfg.register_path + " (HTTP " + std::to_string(http_status) + "): " + response + "\n";
    return -1;
  }
  output += "Registration status from server: " + status + "\n";
  return 0;
}
