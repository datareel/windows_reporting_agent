# logon_watch

Failed logons and account lockouts since the last run, from the
Windows Security log, for Windows 11 and Server 2022/2025 (PowerShell
5.1). Remote attempts are kept apart from the computer's own, so the
owner's typos do not look like an attack:

| Scope | Which logons |
|-------|--------------|
| `remote` | From another address: network (type 3), RDP (10), cleartext (8), ssh; a lockout caused by another computer |
| `trusted` | Same, from an address or computer named with `-Trusted` (for example the office network) |
| `localhost` | From `127.0.0.1` or `::1` |
| `local` | Typed on this computer: sign-in (2), unlock (7), cached (11); lockouts caused here; audit log cleared |

Events:

| Event | From |
|-------|------|
| `failed` | 4625, a wrong password, disabled or expired account, ... (`reason` says which) |
| `invalid_user` | 4625 with no such user (`no_such_user`), or sshd "Invalid user" |
| `lockout` | 4740, the account is locked out; the source is the computer the bad passwords came from |
| `log_cleared` | 1102, the Security log was cleared |
| `too_many`, `preauth_closed`, `accepted` | sshd, from the `OpenSSH/Operational` log |

Windows OpenSSH logs its failed logons in the Security log as logon
type 8 with no address (and user `NOUSER` for unknown users). When the
OpenSSH Server is installed, logon_watch reads the address from
sshd's own log instead (the same lines as on Linux) and skips those
4625 events. Without that log they count as remote with source
`ssh_client`.

## Output

CSV, one row per scope, event, logon type, user, source and reason,
with the count and the first and last time:

```
scope,event,logon_type,user,source,reason,count,first,last
remote,failed,ssh_password,lwtest,192.168.122.1,sshd,6,2026-11-01T23:58:54-05:00,2026-11-02T00:02:05-05:00
remote,invalid_user,ssh,nosuchremote,192.168.122.1,sshd,1,2026-11-01T23:58:54-05:00,2026-11-01T23:58:54-05:00
localhost,failed,network,lwtest,127.0.0.1,bad_password,1,2026-11-02T00:00:36-05:00,2026-11-02T00:00:36-05:00
local,failed,interactive,WIN-DEV-KVM\lwtest,,bad_password,2,2026-11-02T00:00:36-05:00,2026-11-02T00:00:36-05:00
local,lockout,,lwtest,,locked_out,1,2026-11-02T00:02:10-05:00,2026-11-02T00:02:10-05:00
```

`-Format lines` (and `-Submit`) gives DRSM lines: the same table as
`UPLOAD columns|` and `UPLOAD row|` lines, and `DETAIL` values the
dashboard rules use: `<scope>_failed`, `<scope>_invalid_user`,
`<scope>_lockouts` for every scope, `<scope>_too_many`,
`<scope>_preauth_closed`, `<scope>_accepted` and `<scope>_sources`
(addresses with a failure) for remote, trusted and localhost, and
`log_cleared`, `log_reset` (the log was cleared or wrapped since the
last run, so events were missed), `ssh_log`, `events`, `first_event`,
`last_event`.

## Where it starts

The position is the last Security (and OpenSSH) event record number,
saved in `C:\ProgramData\DataReel\ReportingAgent\utils\logon_watch.state`
when the agent is installed (administrators only), otherwise in
`%LOCALAPPDATA%\DataReel\utils\`. The next run starts after it. The
first run reads the last 24 hours. `-Since 30m|24h|7d|<date>` starts
somewhere else, `-NoSave` leaves the position alone, `-EvtxFile FILE`
reads a saved Security log export with no position.

## Parameters

```
logon_watch.ps1 [-EvtxFile FILE] [-Since 24h] [-State FILE] [-NoSave]
                [-Trusted 10.0.0.0/8,OFFICE-PC] [-Format csv|lines] [-Out FILE]
                [-Submit [-Now] [-Hook ID] [-Drra PATH]] [-Version]
```

Run it as an administrator (the Security log is administrators only).
`Get-Help .\logon_watch.ps1 -Full` has the details.

## On the dashboard

```
# On the monitor host
cp -r utils/logon_watch/drsm/uploads/logon_watch <drsm>/plugins/uploads/

# On the computer, administrator prompt
powershell -ExecutionPolicy Bypass -File utils\install_util.ps1 logon_watch -Schedule          (dry run)
powershell -ExecutionPolicy Bypass -File utils\install_util.ps1 logon_watch -Schedule -Apply
powershell -ExecutionPolicy Bypass -File "C:\Program Files\DataReel\ReportingAgent\utils\logon_watch\logon_watch.ps1" -Submit -Now
"C:\Program Files\DataReel\ReportingAgent\drra_svc.exe" uploads
```

The scheduled task runs it every 15 minutes as SYSTEM, and the agent
sends it with its next report. The manifest (`FORMAT=drsm-lines`,
`DISPLAY=table`) warns when `remote_failed`, `remote_invalid_user`,
`remote_too_many`, `remote_lockouts` or `local_lockouts` is above 0,
raises an error at 50 remote failures or when the Security log was
cleared, and warns when the data is over 2 hours old. Local and
localhost typos show in the table but do not alert; a lockout does,
even a local one, because the account cannot sign in until it ends.
Change the rules for one computer with `uploads_logon_watch_WARN_IF=...`
in its DRSM host file.

## Test

`tests\logon_watch_test.ps1` runs on a test computer as an
administrator. It creates a throwaway local account `lwtest` (deleted
at the end), makes failed logons for it, and checks the scopes, the
saved position, the lines format and `-Out`. `-Lockout` locks the
account out (10 more failures), `-ExpectRemote` checks for ssh failures
made from another computer first, and `-AgentDir` sends a report
through a test copy of `drra_svc.exe` (with its own `drra.ini`,
`uploads.d\logon_watch.ini` and a test report server that has the
manifest).
