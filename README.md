# Windows Reporting Agent

A small Windows service that collects a hardware, software, patch and
Windows Defender inventory from each computer and sends it to a report
server on a schedule. It is meant for sites that need to know what is on
their Windows computers but do not run an enterprise management suite
such as Microsoft Configuration Manager (SCCM/MCM), Intune or BigFix.

The agent was first written to sort out a deployment of 120 laptops with
inventory gaps, missing software and patching problems. It is written in
C++ against the Win32 API, with no runtime dependencies, and the server
side is a few PHP pages that run on any Apache and PHP host, including
shared hosting.

The agent can also act as the Windows collector for the DataReel System
Monitor (DRSM).

HTML documentation is in the `docs` directory: open `docs/index.html` in
a browser. It is also at <https://datareel.com/windows_reporting_agent/>.

## Contents

- [What the agent reports](#what-the-agent-reports)
- [How it works](#how-it-works)
- [Requirements](#requirements)
- [Installing the report server](#installing-the-report-server)
- [Installing the agent](#installing-the-agent)
- [Approving agents](#approving-agents)
- [Agent settings](#agent-settings)
- [Server settings](#server-settings)
- [Security](#security)
- [The fleet status page](#the-fleet-status-page)
- [Building from source](#building-from-source)
- [Command line tools](#command-line-tools)
- [Troubleshooting](#troubleshooting)
- [License](#license)

## What the agent reports

Each report is a plain text file named `<hostname>_windows_report_agent.txt`
with these sections:

| Section | Contents |
| --- | --- |
| Header | Agent version, hostname, report time (epoch and local) |
| SERVICE TAG AND BARCODE | Service tag (BIOS serial number, the Dell service tag on Dell computers) and asset tag from WMI |
| IP ADDRESS | IPv4 addresses of the network adapters that are up |
| USERS | Profile folders in `C:\Users` |
| ALL INSTALLED PROGRAMS | Name and version of every program in the 64-bit and 32-bit uninstall registry keys |
| WINDOWS DEFENDER ALERTS | Critical, error and warning events from the Defender event log for the last 30 days |
| INSTALLED WINDOWS PATCHES | Windows Update installation history |
| HOT FIXES | Installed hotfixes from WMI (`Win32_QuickFixEngineering`) |

The service sends a new report every 45 minutes by default.

## How it works

```
 Windows computers                           Report server (Apache + PHP)
 +----------------------------+  HTTPS POST  +------------------------------+
 | WindowsReportAgent service | -----------> | register.php  (first run)    |
 |  - collects the inventory  |  signed with | upload.php    (each report)  |
 |  - signs it with its key   |  the agent's | admin.php     (approve)      |
 +----------------------------+  machine key +------------------------------+
            |                                     | reports_dir/
            | or: copy to a CIFS share            v <host>_windows_report_agent.txt
            | or: scp to an SSH server       fleet status page, DRSM plugin
```

1. On its first run the agent creates an RSA key in the Windows machine
   key store (the key cannot be exported) and a self-signed certificate
   for it.
2. It registers the certificate with `register.php`. A new computer is
   either approved right away with an enrollment token, or waits for an
   admin to approve it on `admin.php`.
3. Once approved, every report it uploads is signed with its key.
   `upload.php` checks the signature against the registered certificate
   and only then saves the report.

The agent can also, or instead, copy the report to a Windows file share
(CIFS/SMB) or to an SSH server with the OpenSSH client built into Windows.

## Requirements

**Agent:** Windows 10 or 11, or Windows Server 2016 or later, 64-bit.
The SCP option needs the Windows OpenSSH client (installed by default on
Windows 10 1809 and later).

**Report server:** Apache with PHP 7.4 or later (PHP 8 supported) and the
OpenSSL extension. Any host that runs PHP works, including shared hosting.
HTTPS with a certificate the Windows computers trust (for example from
Let's Encrypt) is required for real use.

**Building from source:** Visual Studio 2022 or later with the C++
desktop workload, and Inno Setup 6 for the installer.

## Installing the report server

The PHP pages are in the `php` directory. `install_server.sh` copies them
into place, writes the Apache `.htaccess` file and creates the data
directory. It only shows what it would do unless you add `--apply`.

```
$ cd windows_reporting_agent/php
$ ./install_server.sh /var/www/html/windows_reporting_agent /var/lib/windows_reporting_agent
$ sudo ./install_server.sh --apply --admin-user admin \
      /var/www/html/windows_reporting_agent /var/lib/windows_reporting_agent
```

- `WEB_DIR` (first argument) gets an `upload` folder with `register.php`,
  `upload.php`, `admin.php` and `wra_lib.php`.
- `DATA_DIR` (second argument) holds `server.ini`, the agent registrations,
  replay protection data, the admin password file, and the reports in
  `DATA_DIR/reports`. Keep it outside the web root when you can. On shared
  hosting where everything is under `public_html`, put it inside `WEB_DIR`
  and the script adds a `Require all denied` `.htaccess` to it.

Then give the web server user write access to `DATA_DIR`:

```
# setfacl -R -m u:apache:rwX -m d:u:apache:rwX /var/lib/windows_reporting_agent
```

With SELinux enforcing (RHEL, Rocky, Alma, Fedora), also label it:

```
# semanage fcontext -a -t httpd_sys_rw_content_t "/var/lib/windows_reporting_agent(/.*)?"
# restorecon -R /var/lib/windows_reporting_agent
```

The `.htaccess` file needs `AllowOverride All` (or at least `FileInfo
AuthConfig Limit`) for the directory in the Apache configuration. It sets
`WRA_SERVER_INI` to point at `DATA_DIR/server.ini`, blocks direct access
to `wra_lib.php`, and puts `admin.php` behind a Basic auth login. Create
or change the admin login with:

```
$ htpasswd -B /var/lib/windows_reporting_agent/admin.htpasswd admin
```

Last, set an enrollment token in `DATA_DIR/server.ini` if you want new
computers approved automatically (see [Server settings](#server-settings)).

## Installing the agent

Run `windows_reporting_agent_setup_<version>.exe` as an administrator. The
setup asks for:

- **Server host name** of the report server
- **Port**, 443 for HTTPS
- **Web path of the upload pages folder**, for example
  `/windows_reporting_agent/upload`
- **Enrollment token**, or leave it blank to approve the computer on
  `admin.php`
- **HTTPS or HTTP**. Use HTTPS. HTTP is only for a test server.

It installs to `C:\Program Files\WindowsReportAgent`, writes the settings
to `windows_reporting_agent.ini` there, creates the `WindowsReportAgent`
service (automatic start, restarts on failure) and starts it. The first
report is sent right away.

### Unattended installs

For Group Policy, PDQ Deploy, a login script or any other software
deployment tool, pass the settings on the command line:

```
windows_reporting_agent_setup_3.2.exe /VERYSILENT /SUPPRESSMSGBOXES /NORESTART ^
    /SERVER=reports.example.com /PORT=443 /URLPATH=/windows_reporting_agent/upload ^
    /TOKEN=<enrollment token>
```

Add `/HTTP` for a plain HTTP test server and `/LOG=C:\path\setup.log` to
keep a setup log. Settings you leave out keep the values from an existing
install, so running a new version with only `/VERYSILENT` upgrades in place.

### Upgrading and uninstalling

Run the new setup over the old one. It stops the service, replaces the
programs, keeps `windows_reporting_agent.ini` and starts the service again.

Uninstall from **Settings > Apps** or with `unins000.exe /VERYSILENT` in
the install folder. The uninstaller removes the service and programs but
keeps the INI file and `C:\ProgramData\WindowsReportAgent`, so a reinstall
reuses the same key and does not need to be approved again. Delete those
by hand to remove the agent completely.

## Approving agents

Open `admin.php` (for example
`https://reports.example.com/windows_reporting_agent/upload/admin.php`)
and log in. Each registered computer is listed with its status, key
fingerprint, registration time, last report and last address.

- **Approve** a pending computer. Check first that its key fingerprint
  matches the one in `C:\ProgramData\WindowsReportAgent\windows_report_agent.log`
  on that computer.
- **Revoke** a computer to stop accepting its reports. It stays listed.
- **Delete registration** when a computer is reinstalled or renamed. Its
  new key is refused until the old registration is deleted.
- **Delete report** removes the saved report file.

## Agent settings

`windows_reporting_agent.ini` sits next to `windows_reporting_agent_svc.exe`.
The service reads it again before every report, so changes take effect
on the next cycle without a restart. Any setting left out uses its default.

| Section | Setting | Default | Meaning |
| --- | --- | --- | --- |
| delivery | methods | `https` | One or more of `https`, `cifs`, `scp`, comma separated. Every listed method runs each cycle. |
| https | host | `reports.example.com` | Report server |
| https | port | `443` | `80` when `use_https=0` |
| https | upload_path | `/windows/upload/upload.php` | Web path of `upload.php` |
| https | register_path | `/windows/upload/register.php` | Web path of `register.php` |
| https | use_https | `1` | `0` for plain HTTP (test servers only) |
| https | ignore_cert_revocation | `1` | Skip the CRL/OCSP check, for networks that block CRL servers (WinINet error 12057) |
| https | enrollment_token | (empty) | Token from the server's `server.ini` for automatic approval |
| cifs | share | (empty) | UNC path such as `\\fileserver\reports` |
| scp | host, port, user | (empty), `22`, (empty) | SSH server and account |
| scp | key_file | `C:\ProgramData\WindowsReportAgent\id_ed25519` | Private key with no passphrase |
| scp | remote_dir | (empty) | Directory on the SSH server |
| scp | known_hosts_file | `C:\ProgramData\WindowsReportAgent\known_hosts` | Host keys for the server |
| scp | strict_host_key_checking | `yes` | `yes`, or `accept-new` to save the host key on the first connection |
| scp | scp_path | `%SystemRoot%\System32\OpenSSH\scp.exe` | Path to `scp.exe` |
| scp | timeout_seconds | `120` | Stop a hung transfer after this many seconds |
| agent | report_interval_minutes | `45` | Minutes between reports |
| agent | report_dir | `C:\ProgramData\WindowsReportAgent` | Where the report is written before delivery |
| agent | log_file | `C:\ProgramData\WindowsReportAgent\windows_report_agent.log` | Result of the last delivery |
| agent | users_dir | `C:\Users` | Folder listed in the USERS section |
| agent | event_lookback_days | `30` | Days of Defender alerts to include |
| agent | data_dir | `C:\ProgramData\WindowsReportAgent` | Agent certificate and registration state |

### CIFS share

The service runs as LocalSystem, so on a domain it connects to the share
as the computer account (`DOMAIN\HOSTNAME$`). Give the computer accounts,
or the Domain Computers group, write access to the share and folder. The
report is written under a temporary name and then renamed, so a reader
never sees a partial file. CIFS and SCP deliveries do not use the
certificate registration: access is controlled by the share permissions
or the SSH key.

### SCP

Create a key with no passphrase for the agent, put the public key in the
SSH account's `authorized_keys`, and limit the private key to SYSTEM and
Administrators:

```
ssh-keygen -t ed25519 -N "" -f C:\ProgramData\WindowsReportAgent\id_ed25519
icacls C:\ProgramData\WindowsReportAgent\id_ed25519 /inheritance:r /grant SYSTEM:F /grant Administrators:F
```

Add the server's host key to `known_hosts_file` (for example with
`ssh-keyscan`), or set `strict_host_key_checking=accept-new` for the
first connection.

## Server settings

`server.ini` is found through the `WRA_SERVER_INI` environment variable
that `install_server.sh` sets in `.htaccess`, or at
`/etc/windows_reporting_agent/server.ini`. See `php/server.ini.example`.

| Section | Setting | Default | Meaning |
| --- | --- | --- | --- |
| paths | reports_dir | `/var/www/html/windows/reports` | Where accepted reports are saved |
| paths | data_dir | `/var/lib/windows_reporting_agent` | Registrations and replay protection data |
| registration | enrollment_token | (empty) | Shared secret the agents send when they register |
| registration | auto_approve_with_token | `1` | `1` approves an agent with the right token, `0` still needs an admin |
| upload | max_upload_bytes | `2000000` | Largest report accepted |
| upload | max_clock_skew_seconds | `300` | Largest clock difference allowed between agent and server |
| admin | require_remote_user | `1` | `admin.php` only runs behind web server authentication |

An agent that sends a wrong token is refused. An agent that sends no
token is registered as pending.

## Security

- **Agent identity:** a 3072-bit RSA key per computer in the Microsoft
  Software Key Storage Provider as a machine key, marked not exportable.
  The certificate is self-signed with `CN=<hostname>`. The key
  fingerprint is the SHA-256 of the public key (SubjectPublicKeyInfo).
- **Registration:** the registration request is signed with the key,
  which proves the agent holds it. A hostname can only be registered with
  one key: a different key is refused until an admin deletes the old
  registration.
- **Signed uploads:** each upload is signed (RSA PKCS#1 v1.5, SHA-256)
  over the hostname, a timestamp, a random nonce and the SHA-256 of the
  report. The server rejects bad signatures, unregistered, pending and
  revoked hosts, timestamps outside the clock skew window and reused
  nonces, and always saves the report under the signing host's name, so
  one computer cannot overwrite another computer's report.
- **On the computer:** the installer limits
  `C:\ProgramData\WindowsReportAgent` to SYSTEM and Administrators, so
  users cannot read the reports or change the registration state.
- **Transport:** use HTTPS. The signature protects the report from
  changes, but only TLS keeps the inventory private on the network.
- **Admin page:** protected by web server authentication and a CSRF token.
  All output is escaped.

## The fleet status page

`bash/run_windows_laptop_inventory.sh` reads the reports directory and
builds an HTML and text status page: how many computers reported in the
last 24 hours, in the last 15 days, not at all, or sent an empty or bad
report. It uses the DRSM configuration (`~/.drsm.sh` or
`$BASEdir/etc/drsm.sh`) and is normally run from cron on the report
server. Edit the paths at the top of the script (`SYSLOG_DIR` is the
reports directory) to match your server.

## Building from source

Open a **x64 Native Tools Command Prompt for Visual Studio** (or run
`vcvars64.bat`) in the source directory:

```
nmake               build the programs and the event message DLL
nmake installer     build output\windows_reporting_agent_setup_<version>.exe
nmake clean
```

`nmake installer` needs Inno Setup 6 in its default location,
`C:\Program Files (x86)\Inno Setup 6`. `build.txt` lists the
individual compiler commands and a manual service install for
development.

## Command line tools

These are installed with the agent and print one section of the report.
Run them from an administrator command prompt to check what a computer
will report.

| Program | Shows |
| --- | --- |
| `send_test_report.exe` | Builds and sends one report now with the current settings (without the patch and hotfix sections, which are slow) and prints the result |
| `list_all_apps.exe` | Installed programs |
| `list_all_patches.exe` | Windows Update history |
| `list_defender_alerts.exe` | Defender alerts of the last 30 days |
| `get_ip_address.exe` | IP addresses |
| `get_service_tag.exe` | Service tag and asset tag |
| `svccontrol.exe`, `svcconfig.exe` | Start, stop, query and configure the service |

## Troubleshooting

Look in `C:\ProgramData\WindowsReportAgent\windows_report_agent.log`
first. It has the result of the last report for each delivery method.

| Log message | What to do |
| --- | --- |
| `this agent is pending on the server` | Approve the computer on `admin.php` (check the fingerprint in the log) or set the enrollment token |
| `key_mismatch` | The computer was reinstalled or its key changed. Delete its old registration on `admin.php` |
| `bad_timestamp` | Fix the clock on the computer or the server (allowed difference: `max_clock_skew_seconds`) |
| `bad_token` | The enrollment token in the agent INI does not match `server.ini` |
| `error code 12007` | The server name cannot be resolved |
| `error code 12029` | The server cannot be reached on that port |
| `error code 12057` | Certificate revocation check failed: leave `ignore_cert_revocation=1` |
| `error code 12175` or `12045` | The server's TLS certificate is not trusted by Windows |

Run `send_test_report.exe` from an administrator command prompt to see
the result right away instead of waiting for the next cycle.

## License

GNU General Public License v3, see `LICENSE`.
