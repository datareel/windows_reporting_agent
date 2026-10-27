# DataReel Reporting Agent for Windows

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
- [The fleet status page](#the-fleet-status-page) (and DRSM)
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
| PERFORMANCE | CPU count and CPU use (sampled over 5 seconds), memory total, available and used, commit charge, processes, threads, uptime |
| DISK USAGE | Size, used, free and percent used of each local fixed drive |
| NETWORK | Per adapter that is up: link speed, bytes, packets, errors and discards received and sent (totals since the adapter came up) |
| USERS | Profile folders in `C:\Users` |
| ALL INSTALLED PROGRAMS | Name and version of every program in the 64-bit and 32-bit uninstall registry keys |
| WINDOWS DEFENDER ALERTS | Critical, error and warning events from the Defender event log for the last 30 days |
| INSTALLED WINDOWS PATCHES | Windows Update installation history |
| HOT FIXES | Installed hotfixes from WMI (`Win32_QuickFixEngineering`) |

The PERFORMANCE, DISK USAGE and NETWORK sections are `name=value` lines
for trend graphs, for example in the DataReel System Monitor (DRSM). The
names match the DRSM Linux metrics where there is one:

```
PERFORMANCE:
cpu_count=4
cpu_used_pct=7.2
mem_total_mb=16364
mem_available_mb=12307
mem_used_pct=24.8
commit_limit_mb=17388
commit_used_mb=3740
process_count=184
thread_count=1937
uptime_seconds=11230
END PERFORMANCE

DISK USAGE:
drive=C: size_mb=510963 used_mb=108062 free_mb=402901 used_pct=21.1
END DISK USAGE

NETWORK:
adapter="Ethernet" if_index=10 speed_mbps=1000 rx_bytes=91234664 tx_bytes=37097265 rx_packets=82036 tx_packets=23540 rx_errors=0 tx_errors=0 rx_discards=0 tx_discards=0
END NETWORK
```

The network counters are running totals: take the difference between two
reports for a rate. Idle virtual adapters with no traffic are left out.

The service sends a new report every 15 minutes by default
(`report_interval_minutes` in `drra.ini`, or `/INTERVAL=` in setup).

## How it works

```
 Windows computers                           Report server (Apache + PHP)
 +----------------------------+  HTTPS POST  +------------------------------+
 | DataReelReportAgent service | -----------> | register.php  (first run)    |
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
  `upload.php`, `admin.php`, `status.php`, `report.php`, `wra_lib.php`
  and `wra_page.php`.
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
to `wra_lib.php` and `wra_page.php`, and puts `admin.php`, `status.php`
and `report.php` behind a Basic auth login. Create
or change the admin login with:

```
$ htpasswd -B /var/lib/windows_reporting_agent/admin.htpasswd admin
```

If the pages sit under a DataReel System Monitor dashboard (for example
`/dashboard/windows`), they can use the dashboard login instead of a
separate one: add `--drsm-login <dashboard>/config/drsm.php` to
`install_server.sh`. People signed in to the dashboard can then open
`admin.php`, `status.php` and `report.php`, and anyone else is sent to the
dashboard login and back.

Last, set an enrollment token in `DATA_DIR/server.ini` if you want new
computers approved automatically (see [Server settings](#server-settings)).

## Installing the agent

Run `drra_setup_<version>.exe` as an administrator. The
setup asks for:

- **Server host name** of the report server
- **Port**, 443 for HTTPS
- **Web path of the upload pages folder**, for example
  `/windows_reporting_agent/upload`
- **Enrollment token**, or leave it blank to approve the computer on
  `admin.php`
- **HTTPS or HTTP**. Use HTTPS. HTTP is only for a test server.

It installs to `C:\Program Files\DataReel\ReportingAgent`, writes the settings
to `drra.ini` there, creates the `DataReelReportAgent`
service (automatic start, restarts on failure) and starts it. The first
report is sent right away.

### Unattended installs

For Group Policy, PDQ Deploy, a login script or any other software
deployment tool, pass the settings on the command line:

```
drra_setup_3.3.exe /VERYSILENT /SUPPRESSMSGBOXES /NORESTART ^
    /SERVER=reports.example.com /PORT=443 /URLPATH=/windows_reporting_agent/upload ^
    /TOKEN=<enrollment token>
```

Add `/INTERVAL=<minutes>` to change the report interval (default 15),
`/HTTP` for a plain HTTP test server and `/LOG=C:\path\setup.log` to
keep a setup log. Settings you leave out keep the values from an existing
install, so running a new version with only `/VERYSILENT` upgrades in place.

### Upgrading and uninstalling

Run the new setup over the old one. It stops the service, replaces the
programs, keeps `drra.ini` and starts the service again.

Earlier test builds were called "Windows Reporting Agent" (service
`WindowsReportAgent`, `C:\Program Files\WindowsReportAgent`,
`C:\ProgramData\WindowsReportAgent`). Release 3.3 uses DataReel names so
the agent is never mistaken for part of Windows, and its setup moves such
an install over by itself: the settings (with their
paths updated), the agent certificate, registration state and share
password move to the new folders, the old service, uninstall entry and
folders are removed, and the agent keeps its machine key, so it does not
need to be approved again. If an Explorer window has the old folder
open, the empty folder is removed at the next reboot.

Uninstall from **Settings > Apps** or with `unins000.exe /VERYSILENT` in
the install folder. The uninstaller removes the service and programs but
keeps the INI file and `C:\ProgramData\DataReel\ReportingAgent`, so a reinstall
reuses the same key and does not need to be approved again. Delete those
by hand to remove the agent completely.

## Approving agents

Open `admin.php` (for example
`https://reports.example.com/windows_reporting_agent/upload/admin.php`)
and log in. Each registered computer is listed with its status, key
fingerprint, registration time, last report and last address.

- **Approve** a pending computer. Check first that its key fingerprint
  matches the one in `C:\ProgramData\DataReel\ReportingAgent\drra.log`
  on that computer.
- **Revoke** a computer to stop accepting its reports. It stays listed.
- **Delete registration** when a computer is reinstalled or renamed. Its
  new key is refused until the old registration is deleted.
- **Delete report** removes the saved report file.

## Agent settings

`drra.ini` sits next to `drra_svc.exe`.
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
| cifs | user | (empty) | Share login for sites without a Windows domain; save its password with `set-cifs-password` |
| scp | host, port, user | (empty), `22`, (empty) | SSH server and account |
| scp | key_file | `C:\ProgramData\DataReel\ReportingAgent\id_ed25519` | Private key with no passphrase |
| scp | remote_dir | (empty) | Directory on the SSH server |
| scp | known_hosts_file | `C:\ProgramData\DataReel\ReportingAgent\known_hosts` | Host keys for the server |
| scp | strict_host_key_checking | `yes` | `yes`, or `accept-new` to save the host key on the first connection |
| scp | scp_path | `%SystemRoot%\System32\OpenSSH\scp.exe` | Path to `scp.exe` |
| scp | timeout_seconds | `120` | Stop a hung transfer after this many seconds |
| agent | report_interval_minutes | `15` | Minutes between reports |
| agent | report_dir | `C:\ProgramData\DataReel\ReportingAgent` | Where the report is written before delivery |
| agent | log_file | `C:\ProgramData\DataReel\ReportingAgent\drra.log` | Result of the last delivery |
| agent | users_dir | `C:\Users` | Folder listed in the USERS section |
| agent | event_lookback_days | `30` | Days of Defender alerts to include |
| agent | data_dir | `C:\ProgramData\DataReel\ReportingAgent` | Agent certificate and registration state |

### CIFS share

The service runs as LocalSystem, so on a domain it connects to the share
as the computer account (`DOMAIN\HOSTNAME$`). Give the computer accounts,
or the Domain Computers group, write access to the share and folder.

Without a domain (a Samba server, a NAS, a workgroup), give the agent a
share login: set `user=` in `[cifs]` and save the password from an
administrator command prompt:

```
"C:\Program Files\DataReel\ReportingAgent\drra_svc.exe" set-cifs-password
```

It asks for the password and stores it encrypted with Windows DPAPI in
`C:\ProgramData\DataReel\ReportingAgent\cifs_password.dat`, which only SYSTEM
and Administrators can read. For unattended installs use the setup
switches `/METHODS=cifs /CIFSSHARE=\\server\share /CIFSUSER=<user>
/CIFSPASS=<password>`. Setup logs record the command line, so do not
combine `/CIFSPASS=` with `/LOG`, or delete the log afterwards. The
report is written under a temporary name and then renamed, so a reader
never sees a partial file. CIFS and SCP deliveries do not use the
certificate registration: access is controlled by the share permissions
or the SSH key.

### SCP

Create a key with no passphrase for the agent, put the public key in the
SSH account's `authorized_keys`, and limit the private key to SYSTEM and
Administrators:

```
ssh-keygen -t ed25519 -N "" -f C:\ProgramData\DataReel\ReportingAgent\id_ed25519
icacls C:\ProgramData\DataReel\ReportingAgent\id_ed25519 /inheritance:r /grant SYSTEM:F /grant Administrators:F
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
| admin | require_remote_user | `1` | `admin.php`, `status.php` and `report.php` only run behind web server authentication |
| admin | login | `basic` | `basic` for the web server login, or `drsm` to use the DataReel System Monitor dashboard login (the pages must be under the dashboard's web folder) |
| admin | drsm_config | (empty) | With `login = drsm`: the dashboard's `config/drsm.php`, absolute or relative to the pages folder |
| status | late_hours | `24` | A report older than this is late (warning) |
| status | missing_days | `15` | A report older than this is missing (error) |
| status | patch_max_age_days | `45` | Warn when the newest Windows update is older than this |
| status | required_apps | (empty) | Programs every computer must have, comma separated, `*` and `?` wildcards |
| status | required_apps_skip_hosts | (empty) | Computers the required programs check skips, such as `srv-*` |
| status | unwanted_apps | (empty) | Programs no computer should have |
| status | public | `0` | `1` lets the status pages run without a login. Only for a demo with sample data |
| status | reference_time | (empty) | Measure report ages from this epoch time, or `fleet` for the sample fleet's time |

Relative paths work too: `WRA_SERVER_INI` relative to the pages folder,
and `reports_dir` and `data_dir` relative to `server.ini`.

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
  `C:\ProgramData\DataReel\ReportingAgent` to SYSTEM and Administrators, so
  users cannot read the reports or change the registration state.
- **Transport:** use HTTPS. The signature protects the report from
  changes, but only TLS keeps the inventory private on the network.
- **Admin page:** protected by web server authentication and a CSRF token.
  All output is escaped.

## The fleet status page

`status.php` lists every computer with its state, worst first:

- **Needs attention:** no report for `missing_days`, an empty or bad
  report, or a Windows Defender detection, failed action or disabled
  protection
- **Warning:** a late report, Windows updates older than
  `patch_max_age_days`, a missing required program or an unwanted program
- **OK:** none of the above

The tiles at the top filter the list, and the search box matches
computer names, users, service tags, IP addresses and problems.
`report.php?host=<name>` shows one computer's report: problems,
hardware, users, Defender events, installed programs (with a filter),
Windows updates and hotfixes, and a link to the report file. Both pages
are read only and behind the admin login. Set the rules in the
`[status]` section of `server.ini`, for example:

```
[status]
required_apps = "Microsoft 365 Apps*, Google Chrome"
required_apps_skip_hosts = "srv-*"
unwanted_apps = "uTorrent*, CCleaner*"
```

### Live demo

`php/install_demo.sh` installs `status.php` and `report.php` in a
folder of their own with a sample fleet from `tools/gen_sample_fleet.py`
and no login, as on <https://datareel.com/windows_reporting_agent/demo/>.
It is a dry run unless you add `--apply`:

```
$ php/install_demo.sh --apply /var/www/html/windows_reporting_agent/demo
```

### DataReel System Monitor (DRSM)

DRSM shows Windows computers on its dashboard next to the Linux hosts.
It never connects to a Windows computer: its `wra` collector runs the
`windows/*` plugins on the monitor host against each computer's last
report in `WRA_REPORTS_DIR`. The simplest way to get the reports there is
the CIFS delivery to a share on the monitor host; reports that arrive by
HTTPS on another server have to be copied over.

| DRSM plugin | Checks |
| --- | --- |
| `windows/checkin` | The computer reported recently and completely, agent version |
| `windows/defender` | Defender threats, protection turned off, old definitions |
| `windows/patches` | Age of the newest Windows patch |
| `windows/apps` | Required and forbidden programs |
| `windows/resources` | CPU, memory, disk and network from the usage sections, for the trends page (DRSM release after 1.31) |

```
# DRSM etc/hosts/_defaults.conf
WRA_REPORTS_DIR=/path/to/reports

# DRSM etc/hosts/pc-0142.conf
HOST=pc-0142
PLATFORM=windows
COLLECTOR=wra
PROBES=""
PLUGINS="windows/checkin windows/defender windows/patches windows/apps windows/resources"
```

### Status page script

`bash/run_windows_laptop_inventory.sh` is the original status page:
it reads the reports directory and
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
nmake installer     build output\drra_setup_<version>.exe
nmake clean
```

`tools/gen_sample_fleet.py` writes a sample fleet of reports with made up
hostnames, covering current, late and missing check-ins, empty and bad
report files, computers behind on updates, missing and unwanted programs
and Defender detections. Use it to try the status page or the DRSM
plugins without real computers:

```
$ python3 tools/gen_sample_fleet.py --out /tmp/sample_fleet --count 48
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
| `drra_svc.exe set-cifs-password` | Save the `[cifs] user` share password, encrypted |

## Troubleshooting

Look in `C:\ProgramData\DataReel\ReportingAgent\drra.log`
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
| `No share password, run: ... set-cifs-password` | `[cifs] user=` is set but its password was never saved |
| `Could not connect to \\... as ...` with `error code 1326` | Wrong share user name or password: run `set-cifs-password` again |
| `error code 53` (network path not found) | The computer cannot reach the file server on port 445 |
| `error code 1219` (multiple connections) | Another login to the same server with other credentials is open, for example a mapped drive in a desktop session |
| `Could not copy report to \\...` | The login works but the write fails: share or folder permissions, or SELinux on a Samba server |

Run `send_test_report.exe` from an administrator command prompt to see
the result right away instead of waiting for the next cycle.

## License

GNU General Public License v3, see `LICENSE`.
