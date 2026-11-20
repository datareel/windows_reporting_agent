# DataReel Reporting Agent for Windows: AI-assisted install, build, upgrade and support

This file is for AI-assisted work on the DataReel Reporting Agent for
Windows (DRRA): the `DataReelReportAgent` Windows service
(`drra_svc.exe`), its command line tools, the PHP report server pages
(`register.php`, `upload.php`, `hook.php`, `admin.php`, `status.php`,
`report.php`) and the DataReel System Monitor (DRSM) plugins in `drsm/`.
It tells an AI agent (Gemini CLI, Claude Code, GitHub Copilot, Codex and
others) how to assist a site's own administrators and developers to set
up the report server, install the agent on Windows computers, approve
them, upgrade older releases to the current release, 3.6, build it from
source, maintain a fork, and troubleshoot. The agent
assists; it does not replace the administrators or DataReel support, and
it never acts autonomously (section 0). The people review and approve
every step, and problems the agent cannot solve go to DataReel (section
9.4). `GEMINI.md` and `CLAUDE.md` in this folder only import this file.

The full documentation is `README.md` and the HTML pages in `docs/`
(install, report server, approving computers, configuration, reports,
security, building, troubleshooting, what's new), also at
https://datareel.com/windows_reporting_agent/. The security design is
described at https://datareel.com/windows_reporting_agent/security_design.php
(`docs/security_design.html` in this folder). This file points to them
instead of repeating them. When this file and the README or the code
disagree, the README and the code win: say so to the person you work for.

## Before you use AI assistance

**Review all of your agency's or company's AI usage policies, and get formal
written approval from your management team, in the form of a business use
case and justification, before you use any AI assistance with the
Reporting Agent.** AI desktops and command line tools may be used with this
guide only in strict compliance with those policies and that approval.

- **Use only approved enterprise AI models.** An AI agent working with the
  Reporting Agent may process potentially sensitive data: computer names,
  user names, IP addresses, service tags, installed programs, missing
  security updates, Windows Defender detections, server names, command
  output and logs. A fleet inventory tells an attacker where to look, so
  treat it as sensitive. Use only your agency's or company's enterprise AI
  models that have built-in safeguards against disclosing information and
  that guarantee your internal information will not be used to train
  public models.
- **Warning: public or free AI tools, open source AI models and self-built
  AI models** may expose your information to public learning models or to
  other parties, depending on how the model is hosted, configured and
  licensed. Use them only after a comprehensive review of what information
  might be exposed and what the potential risks are.
- The entire risk of AI-assisted use rests with the user. DataReel cannot
  be held accountable for actions taken, commands run or other outcomes of
  AI assistance (see https://datareel.com/terms.php).
- **No endorsement.** Any mention of a specific AI tool, model or product
  in this guide, such as Gemini, Gemini CLI, Claude Code or GitHub Copilot,
  is a reference only. It is not an endorsement or a recommendation to use
  that product. Product names are trademarks of their respective owners.

**For the agent:** at the start of a session, ask the person to confirm
that this AI-assisted use is approved under their organization's AI usage
policy and that you are an approved enterprise AI model. If they cannot
confirm it, stop and do not ask for or read any data from their systems.

## 0. Human in the loop: the agent never acts on its own

AI-assisted means the agent assists a person; it never acts
autonomously. Every change to a computer, the report server or a
registration is made by a person or explicitly approved by one, one step
at a time.

- **Advisory mode is the default.** The agent explains the next step and
  prints the exact command and the computer it is for (a Windows computer,
  the report server or the DRSM monitor host). The person runs it and
  pastes back only output that holds no secret (rule 1). Use this mode
  unless the organization allows the agent to run tools.
- **If tool execution is allowed,** each command still needs its own
  explicit approval before it runs. Never use auto-approve modes: no
  Gemini CLI `--yolo` or auto-accept, no Claude Code permission bypass
  (`--dangerously-skip-permissions`, bypass mode), no auto-run in other
  agents. Commands that type or store a secret (rule 1) are run by the
  person, never by the agent.
- **No unattended work:** no scheduled, background, batch or scripted
  runs of changing commands, and no loops over computers that the person
  has not approved as one listed step. Rolling setup.exe out to many
  computers (Group Policy, PDQ Deploy, a login script) is the site's own
  deployment process: the agent helps write and check the command line,
  and the people run the rollout, starting with one test computer.
- **Read-only checks first** (section 9.1); they are shown and approved
  the same way.

**Works with strict AI policies.** These files contain no code that runs
by itself: `AGENTS.md`, `GEMINI.md` and `CLAUDE.md` are text. Everything in
this guide works in advisory mode, with no tool execution by the agent.

## 1. Rules for agents (read these first)

The Reporting Agent decides which computers a report server trusts, and
its reports describe every computer in a fleet. A wrong step can let a
computer that is not yours send reports, stop a fleet from reporting, or
expose the inventory. Follow these rules on every task:

1. **Never see a secret.** Never ask for, read, store or repeat the
   enrollment token, a file share (CIFS) password, an SCP private key, an
   admin or dashboard password, or the contents of `server.ini`,
   `admin.htpasswd`, the DRSM `config/drsm.php`, `cifs_password.dat`, the
   SCP key file (`id_ed25519`) or a setup log made with `/CIFSPASS=`. Ask
   for the name of a setting and whether it is set, never its value.
   The person types secrets themselves: `drra_svc.exe set-cifs-password`
   at its prompt (never with the password on the command line), `htpasswd`
   at its prompt, and the token into `drra.ini`, `server.ini` or the setup
   command they run.
2. **The agent key is never exported.** The agent's RSA key is a machine
   key in the Microsoft Software Key Storage Provider, marked not
   exportable. Never suggest exporting it, changing its export policy,
   copying it to another computer or deleting it: a computer with a new
   key has to be approved again (`key_mismatch`).
3. **Show, then the person runs or approves it.** Before any command that
   changes a computer, the report server or a registration (setup.exe,
   `unins000.exe`, `sc`/`svccontrol`/`svcconfig`, `set-cifs-password`,
   `submit`, `send-uploads`, `scan` (it sends a report), edits to
   `drra.ini`, `server.ini`, `.htaccess` or ACLs, `install_server.sh
   --apply`, `install_demo.sh --apply`, `drsm/install.sh --apply`, and
   Approve, Revoke, Delete registration or Delete report on `admin.php`),
   print the command or the action and the computer. In advisory mode the
   person runs it; otherwise wait for an explicit yes for that one step
   (section 0).
4. **Check the fingerprint before an approval.** A computer is approved on
   `admin.php` only after the person has matched its key fingerprint with
   the one in `C:\ProgramData\DataReel\ReportingAgent\drra.log` on that
   computer. Never suggest approving a pending computer nobody can vouch
   for, and never suggest deleting a registration to clear a
   `key_mismatch` before the person knows why the key changed.
5. **Back up before changing.** Before a change on the report server, the
   person copies `server.ini`, the `agents` folder and the pages folder to
   a dated backup only they can read. Before an upgrade, keep the old
   setup.exe. Never delete reports, registrations, nonces, hook data or
   the agent data folder; move aside only with the person's OK.
6. **Dry runs first.** `install_server.sh`, `install_demo.sh` and
   `drsm/install.sh` only show what they would do until run with
   `--apply`: run them without it first and read the list. Test a new
   release or a new setting on one computer before a rollout, with
   `send_test_report.exe` or `drra_svc.exe scan --no-send`.
7. **Stop on any failed check.** If a check fails or the output is not what
   this guide or the README says to expect, stop and report. Do not
   improvise a fix on the report server or across a fleet.
8. **Never weaken a safeguard.** Do not suggest `use_https=0` or setup
   `/HTTP` outside a test server, `require_remote_user = 0`,
   `[status] public = 1` for real reports (it is only for the demo with
   sample data), `auto_approve_with_token = 1` without the person's
   decision, a larger `max_clock_skew_seconds` to hide a wrong clock,
   `strict_host_key_checking=no`, changing the ACL of
   `C:\ProgramData\DataReel\ReportingAgent` (SYSTEM and Administrators
   only), putting `DATA_DIR` in the web root without its `Require all
   denied`, removing the `.htaccess` login, or turning off signature,
   nonce or timestamp checks in the PHP pages. `ignore_cert_revocation=1`
   is the default for networks that block revocation servers; setting it
   to 0 where revocation servers can be reached is stronger. Report
   instead of working around a check.
9. **No secrets or site data in files you write** (tokens, passwords,
   keys, server names, computer names, user names), unless the person asks
   for that file on their own systems. Examples use `reports.example.com`
   and `pc-0142`.
10. **Reports are site data.** Ask only for the report section or log
    lines a task needs, not whole reports or the reports folder, and leave
    out names when the person asks. Never send reports anywhere outside
    the site.
11. **Keep a log.** List each command, on which computer, and its result
    (exit code and non-secret output), and give it to the person at the
    end.

## 2. What the Reporting Agent is

A Windows service that collects a hardware, software, patch and Windows
Defender inventory from each computer and sends it, every 15 minutes by
default, to a report server, a Windows file share (CIFS) or an SSH server
(SCP). It is C++ against the Win32 API with no runtime dependencies. On
its first run it creates a non-exportable 3072-bit RSA machine key and a
self-signed certificate, registers the certificate with `register.php`,
and signs every upload (RSA PKCS#1 v1.5 with SHA-256 over the host name,
a timestamp, a one-time nonce and the SHA-256 of the report). The report
server keeps reports only from approved computers. The service only makes
outgoing connections; nothing connects to it. DRSM reads the reports and
never connects to a Windows computer. The design is at
https://datareel.com/windows_reporting_agent/security_design.php.

| Item | Where |
| --- | --- |
| Service | `DataReelReportAgent` (LocalSystem, automatic start, restarts on failure), `C:\Program Files\DataReel\ReportingAgent\drra_svc.exe` |
| Settings | `drra.ini` next to `drra_svc.exe`, read again before every report |
| Agent data (SYSTEM and Administrators only) | `C:\ProgramData\DataReel\ReportingAgent`: `agent_cert.pem`, `registration_status.txt`, the report, `drra.log`, `cifs_password.dat`, `scan_vulnerabilities.txt`, `uploads.d\`, `uploads\` |
| Agent key | Machine key `DataReelReportAgent` in the Microsoft Software Key Storage Provider (installs upgraded from test builds keep `WindowsReportAgent`) |
| Report | `<hostname>_windows_report_agent.txt` |
| Event log | Application log, source `DataReelReportAgent` |
| Report server | `WEB_DIR/upload/*.php`; `DATA_DIR/server.ini`, `agents/`, `nonces/`, `reports/`, `hooks/`, `admin.htpasswd` |
| DRSM | `wra` collector and `windows/*` plugins (DRSM 1.32-beta2 and later, or `drsm/install.sh` for DRSM 1.31) |

Read-only checks (they change nothing and need no secret), on the Windows
computer from an administrator prompt:

```
sc query DataReelReportAgent
type "C:\ProgramData\DataReel\ReportingAgent\drra.log"
findstr /b "VERSION:" "C:\ProgramData\DataReel\ReportingAgent\*_windows_report_agent.txt"
"C:\Program Files\DataReel\ReportingAgent\drra_svc.exe" uploads
"C:\Program Files\DataReel\ReportingAgent\drra_svc.exe" scan --no-send
powershell "(Get-ItemProperty 'HKLM:\Software\Microsoft\Windows\CurrentVersion\Uninstall\*' | ? DisplayName -like '*DataReel*Reporting*').DisplayVersion"
```

`scan --no-send` asks Windows Update for the updates not installed yet
and prints them (seconds to minutes of CPU and network); it sends nothing.
`list_all_apps.exe`, `list_all_patches.exe`, `list_defender_alerts.exe`,
`get_ip_address.exe` and `get_service_tag.exe` print one report section
each. `send_test_report.exe` builds and **sends** a report now, so it is a
changing step (rule 3).

On the report server: `ls -l` and `ls -Z` of `DATA_DIR`, the Apache error
log, and `admin.php` (read the list; do not press buttons without the
person, rule 3).

## 3. Repository layout and building (development work)

| Path | Contents |
| --- | --- |
| `drra_svc.cpp` | The service: main loop, report builder, command line (`scan`, `submit`, `send-uploads`, `uploads`, `set-cifs-password`) |
| `common.cpp`, `common.h` | Settings, inventory collection (WMI, registry, event log), CIFS and SCP delivery, HTTPS upload; `AGENT_VERSION` |
| `agent_auth.cpp`, `agent_auth.h` | Machine key, certificate, signing, registration, multipart POST (WinINet) |
| `scan.cpp`, `scan.h` | Vulnerability scan with the Windows Update Agent API |
| `uploads.cpp`, `uploads.h` | Hook uploads: `uploads.d`, spool folders, sending to `hook.php` |
| `send_test_report.cpp`, `list_*.cpp`, `get_*.cpp`, `svc*.cpp` | Command line tools |
| `drra_messages.mc`, `drra_installer.iss`, `drra.ini`, `Makefile` | Event messages, Inno Setup script, default settings, nmake build |
| `php/` | Report server pages, `install_server.sh`, `install_demo.sh`, `server.ini.example` |
| `drsm/` | DRSM plugins and `install.sh` for DRSM 1.31 (`sync_from_drsm.sh` copies them from DRSM) |
| `utils/` | Standalone report programs that send through the agent (`logon_watch`), `install_util.ps1` |
| `tools/` | `gen_docs.py` (the `docs/` pages), `gen_sample_fleet.py` (sample reports, made up names) |
| `docs/` | HTML documentation (made by `tools/gen_docs.py`; edit the script, not the pages), `security_design.html` (made by `docs/make_docs_html.py` from the website page) |

Build on Windows in an **x64 Native Tools Command Prompt for Visual
Studio** (Visual Studio 2022 or later with the C++ desktop workload):
`nmake` (programs and the message DLL), `nmake installer` (needs Inno
Setup 6, `output\drra_setup_<version>.exe`), `nmake binzip`
(`output\drra_<version>_bin_x64.zip`), `nmake utilszip`, `nmake clean`.
`build.txt` has the single compiler commands and a manual service install
for development.

Build in a copy of the source (for example from `git archive`), not in
the clone you commit from, and stage files by name: never commit `.exe`,
`.obj`, `.dll` or `output\` files.

Tests: there is no automatic test suite for the service. Test on a test
computer or virtual machine, never on a production computer: build, run
`send_test_report.exe` against a test report server (`php -S` or a test
Apache folder installed with `install_server.sh`), check `drra.log`, the
`STATUS:` replies, `admin.php` and `status.php`; `scan --no-send` for the
scan; `utils\<name>\tests\` for each util. `tools/gen_sample_fleet.py
--out <dir> --count 48` makes sample reports for the status pages and the
DRSM plugins. Check PHP changes with `php -l` on every page.

Every release bumps `AGENT_VERSION` in `common.h`, `AppVersion` and the
version info in `drra_installer.iss`, the `VERSION` in `Makefile`, adds a
release to the changelog in `tools/gen_docs.py` (then `python3
tools/gen_docs.py docs`), and updates the version in `README.md`.

## 4. Setting up the report server

README "Installing the report server" and `docs/server.html`.

1. Apache with PHP 7.4 or later and the OpenSSL extension, and HTTPS with
   a certificate the Windows computers trust.
2. `php/install_server.sh WEB_DIR DATA_DIR` (dry run, read the list), then
   with the person's OK `--apply` (add `--admin-user NAME`, or
   `--drsm-login <dashboard>/config/drsm.php` under a DRSM dashboard).
   It never overwrites an existing `server.ini` or `admin.htpasswd`. Keep
   `DATA_DIR` outside the web root when the host allows it.
3. The person gives the web server user write access to `DATA_DIR`
   (`setfacl`, and `semanage fcontext ... httpd_sys_rw_content_t` plus
   `restorecon` with SELinux) and sets the admin password with `htpasswd
   -B` at its prompt.
4. The person decides the approval policy: admin approval only (no token),
   or an `enrollment_token` with `auto_approve_with_token`. The token is a
   shared secret (rule 1); with auto-approval on, anyone who has it can
   register a computer under an unused name.
5. Check: `https://<server>/<path>/upload/admin.php` asks for the login,
   and `register.php` answers a browser GET with `STATUS: error` and
   `POST required`.

## 5. Installing the agent

README "Installing the agent" and `docs/install.html`.

1. Download `drra_setup_<version>.exe` and its `.sha256sum` from
   https://datareel.com/downloads/windows_reporting_agent/ and check it:
   `certutil -hashfile drra_setup_<version>.exe SHA256` and compare. Stop
   if it does not match.
2. Install on **one test computer** first (Windows 11, Windows Server 2022
   or 2025, 64-bit), as an administrator: interactively, or
   `drra_setup_<version>.exe /VERYSILENT /SUPPRESSMSGBOXES /NORESTART
   /SERVER=reports.example.com /PORT=443
   /URLPATH=/windows_reporting_agent/upload` with `/TOKEN=` typed by the
   person when the site uses one. `/INTERVAL=` sets the minutes between
   reports. Never combine `/CIFSPASS=` with `/LOG` (the log keeps the
   command line); for a CIFS login prefer `set-cifs-password` afterwards.
3. Check `drra.log`: the key fingerprint, and either `Upload successful`
   or `pending on the server`. Approve the computer (rule 4), then check
   that the next report arrives (`status.php`, or `admin.php` last seen).
4. CIFS and SCP delivery: README "CIFS share" and "SCP". The SCP key is
   made by the person (`ssh-keygen -t ed25519 -N ""`), limited to SYSTEM
   and Administrators with `icacls`, and the server's host key goes into
   `known_hosts_file`. CIFS and SCP deliveries are not signed: share
   permissions and the SSH account control who can write.
5. Only after the test computer works: the site's own rollout (section 0).

## 6. AI-assisted upgrade to 3.6

### 6.1 What changes, and what does not

- **Kept by setup:** `drra.ini`, the agent key, `agent_cert.pem`, the
  registration state and the share password, so an upgraded computer
  does not need to be approved again. New settings take their defaults.
- **3.6:** setup limits `drra.ini` (it holds the enrollment token) to
  SYSTEM and Administrators on every install and upgrade, so standard
  users can no longer read it; edit it from an administrator editor. This
  guide and the security design white paper are in the release. No change
  to the report format or the server pages.
- **3.5:** vulnerability scan (`drra_svc.exe scan`, `[scan] enabled=0`
  by default, VULNERABILITIES section); DRSM needs the
  `windows/vulnerabilities` plugin for it.
- **3.4:** stable release of the 3.3 betas; bin zip.
- **3.3 beta 2:** hook uploads: the report server needs the new
  `hook.php` and `hook_lib.php` (`install_server.sh` installs them) and a
  `[hooks]` section in `server.ini`.
- **Test builds named Windows Reporting Agent** (before 3.3): setup moves
  them to the DataReel names and keeps the old key, so they stay
  approved.
- **Report server pages:** the agent works with the pages of its own
  release or later. Upgrade the server pages first, then the computers.

### 6.2 Steps (confirm each)

1. **Back up** the report server (rule 5) and keep the old setup.exe.
2. **Report server:** `install_server.sh` with the same `WEB_DIR` and
   `DATA_DIR` (dry run, then `--apply`); it keeps `server.ini` and the
   login. Add new `server.ini` sections from `server.ini.example` by hand.
3. **One test computer:** run the new setup over the old one
   (`/VERYSILENT` keeps every setting). Check the installed version (DisplayVersion, section 2), `sc
   query`, `drra.log` (`Upload successful`, no new registration) and the
   `VERSION:` line of the next report.
4. **DRSM:** update the `windows/*` plugins (DRSM release, or
   `drsm/install.sh` dry run then `--apply`) and add
   `windows/vulnerabilities` to `PLUGINS` where wanted.
5. **Rollout** to the rest of the computers with the site's deployment
   tool (section 0).

Roll back: run the previous setup.exe over the new one; the settings and
the key are kept. Report server pages: copy the backup back.

## 7. Hook uploads, utils and DRSM

README "Hook uploads", `utils/README.md` and DRSM's PLUGINS.md.

- An upload hook is a file in `uploads.d\<id>.ini`; the report server only
  takes a hook ID with a manifest in `[hooks] manifest_dir` (DRSM
  `plugins/uploads/<id>`), from a registered computer, about itself.
- `drra_svc.exe submit --hook <id> [--now] <file>` queues a file (a
  changing step, rule 3). Check what the file holds before it is sent:
  it goes to the report server and the DRSM dashboard.
- Utils such as `logon_watch` report failed logons with user names and
  addresses: site data (rule 10).
- DRSM reads reports from `WRA_REPORTS_DIR`; a host file has
  `PLATFORM=windows`, `COLLECTOR=wra` and the `windows/*` plugins.

## 8. Forks and contributions

The Reporting Agent is free software under the GNU General Public License
version 3 (see `LICENSE`).

- Keep the license and copyright notices, and make the changed source
  available under the same license when you distribute a fork.
- Use your own product name, service name, folders and version numbers,
  so a fork is never taken for a DataReel release.
- Keep the signed request formats (`register`, `upload` and `hook`
  strings), the `STATUS:` replies and the report section format unless
  every report server, DRSM plugin and agent changes together; DRSM
  plugins parse the report sections.
- Do not remove a server-side check (signature, approval, nonce,
  timestamp, host name pattern, size limits, CSRF token, output escaping)
  or the data folder ACL (rule 8).
- Security issues: report them privately through
  https://datareel.com/contact.php, not in public issue trackers.

## 9. AI-assisted troubleshooting and support

Diagnose read-only first; change nothing until the person agrees on the
cause and the fix (rule 3).

### 9.1 Where to look

| What | Command or file |
| --- | --- |
| Service state | `sc query DataReelReportAgent`, Application event log source `DataReelReportAgent` |
| Last delivery result | `C:\ProgramData\DataReel\ReportingAgent\drra.log` |
| Agent version | `VERSION:` line of the report, installed DisplayVersion (section 2) |
| Registration state | `registration_status.txt` (state, key fingerprint, server) and `admin.php` |
| Settings | `drra.ini` (the person reads out the setting names asked for; `enrollment_token` is a secret) |
| Hook uploads | `drra_svc.exe uploads`, the UPLOADS section of the report |
| Vulnerability scan | `drra_svc.exe scan --no-send`, `scan_vulnerabilities.txt` |
| Report server | Apache error log, `ls -l`/`ls -Z` of `DATA_DIR`, `STATUS:` text in `drra.log` |
| DRSM | the host's DRSM report for `windows/checkin`, `WRA_REPORTS_DIR` |

### 9.2 Common problems

The full table is README "Troubleshooting" and `docs/troubleshooting.html`.

- **`this agent is pending on the server`:** approve it after the
  fingerprint check (rule 4), or the person sets the enrollment token.
- **`key_mismatch`:** the computer was reinstalled, renamed or its key
  changed. The person finds out why, then deletes the old registration on
  `admin.php` and approves the new key (rule 4).
- **`bad_timestamp`:** fix the clock (Windows time service or the
  server's NTP); do not raise `max_clock_skew_seconds` (rule 8).
- **`bad_token`:** the token in `drra.ini` does not match `server.ini`; the
  person compares them without pasting either (rule 1).
- **`bad_signature` or `replay`:** something changed the request on the
  way (a proxy) or sent it twice. Stop and report (9.3) if it repeats.
- **WinINet 12007, 12029, 12057, 12175 or 12045:** name resolution, port,
  revocation check, or a server certificate Windows does not trust; fix
  DNS, the firewall or the certificate, not `use_https`.
- **CIFS errors 1326, 53, 1219:** wrong share login, port 445 blocked, or
  another login to the same server; the person runs `set-cifs-password`
  again at its prompt.
- **SCP `Host key verification failed`:** add the server's key to
  `known_hosts_file` after the person checks it out of band.
- **Server `Could not save the registration` / `Reports directory is
  missing`:** the web server user cannot write `DATA_DIR` (owner, ACL,
  SELinux label).
- **Status page `Unknown` severities:** Windows Update leaves severity and
  CVEs empty on some updates; DRSM counts those security updates as
  Important.

### 9.3 Stop and escalate at once

Do not try to fix these yourself. Report to the person straight away, with
the summary in 9.4:

- A pending or `key_mismatch` computer nobody can vouch for, a
  registration nobody made, or reports from a computer the site does not
  own.
- Repeated `bad_signature` or `replay` replies, or a report that does not
  match its computer.
- A report server whose `DATA_DIR`, `server.ini`, reports or
  `admin.htpasswd` can be read from the web, or an admin page without a
  login.
- An enrollment token, share password or SCP key that may have been seen
  by someone who should not have it (change it: the person does).
- Anything that needs a secret to be shared, or a safeguard to be turned
  off (rules 1 and 8).

### 9.4 Support summary

When a problem is not solved, give the person a summary they can send to
their team or to DataReel (https://datareel.com/contact.php). AI-assisted
support ends here: the people take it from this summary.

1. Agent version, Windows version, how it was installed (setup.exe
   interactive or silent, bin zip, source build), and the delivery
   methods in use.
2. The `drra.log` lines and the `STATUS:` reply for the failing step, with
   host and server names replaced when the person asks.
3. Report server: PHP and Apache versions, shared hosting or not, the
   login type (`basic` or `drsm`), and the Apache error log lines.
4. DRSM version and the plugin output, if DRSM is involved.
5. What happened, when, what changed before it, and every command already
   tried with its result.

Never include the enrollment token, passwords, keys, `server.ini`,
`admin.htpasswd`, `cifs_password.dat` or whole reports in the summary.
Remove computer, user and server names when the person asks.

The Reporting Agent is free software, provided as is. Check every command
an agent proposes before it runs; the person who runs it is responsible
for the changes made.

## 10. Report back

End every task with: the computers and servers touched (names only), each
command and its result (exit code and non-secret output), the backups made
and where, each approval, revoke or delete on `admin.php` and who checked
the fingerprint, every warning and what was decided, and the final agent
version and `drra.log` result.
