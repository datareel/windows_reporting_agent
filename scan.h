#ifndef SCAN_H
#define SCAN_H

// Vulnerability scan: the Windows updates this computer still needs,
// from the Windows Update Agent (WUA) API, with their MSRC severity and
// CVEs. The same VULNERABILITIES report section as the DataReel
// Reporting Agent for Linux, with source=wua:
//
//   VULNERABILITIES:
//   mode=manual
//   source=wua
//   feed=Windows Update
//   scan_etime=1793000000
//   scan_seconds=42
//   status=ok
//   critical=1 important=0 moderate=0 low=0 unknown=0 total=1 cves=12
//   advisory id=KB5066835 severity=Critical issued=2026-10-14 cves=CVE-2026-1,... packages=Windows 11 title="..."
//   other id=KB5066131 classification="Definition Updates" title="..."
//   END VULNERABILITIES
//
// "advisory" lines are security updates, "other" lines the rest of the
// updates waiting to install (drivers, definitions, feature updates).
//
// Scans run by hand ("drra_svc.exe scan") unless [scan] enabled=1. The
// result is kept in data_dir and goes in every report until the next
// scan.

#include "common.h"

#define SCAN_VULN_FILE "scan_vulnerabilities.txt"

bool ScanDue(const AgentConfig &cfg);
int RunScan(const AgentConfig &cfg, std::string &output);
std::string ScanReportSection(const AgentConfig &cfg);

#endif
