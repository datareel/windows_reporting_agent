# Utilities

Standalone programs that make a report about the computer, for example
a summary of failed logons. Each one works on its own, with no agent
installed, and can hand its result to the DataReel Reporting Agent for
Windows, which sends it to the report server and the DataReel System
Monitor (DRSM) dashboard. They use the agent's hook uploads
(`drra_svc.exe submit`, `uploads.d`, agent 3.3-beta2 and later), see
"Hook uploads" in the top README.

| Util | What it reports |
|------|-----------------|
| [logon_watch](logon_watch/README.md) | Failed logons and lockouts since the last run, remote apart from the computer's own |

## Download

Each agent release has the utils as their own asset,
`drra-utils-<version>.zip` next to `drra_setup_<version>.exe` (built by
`nmake utilszip`). It unpacks to `drra-utils-<version>\` with the files
of `utils\` below, without the tests (those are in the source tarball).
The steps below work the same from that folder:
`powershell -ExecutionPolicy Bypass -File install_util.ps1 <name>`.

## Layout

Each util lives in `utils\<name>\`:

| File | What it is |
|------|------------|
| `<name>.ps1` | The program (PowerShell 5.1, in every supported Windows; a compiled util would be `<name>.exe`) |
| `README.md` | What it reports, its options, how to install it |
| `<name>.ini` | Upload hook file for the agent: `C:\ProgramData\DataReel\ReportingAgent\uploads.d\<name>.ini` |
| `drsm\uploads\<name>\` | DRSM manifest (`plugin.conf`, `probe.sh`): copy to `<drsm>/plugins/uploads/<name>` on the monitor host |
| `tests\` | Test script, run on a test computer |

The upload hook ID, the DRSM manifest folder and the util are all
named `<name>` (a-z, 0-9 and `_`, up to 32 characters).

## Command line

Every util takes these parameters, and its own. PowerShell parameters
start with one dash and are not case sensitive.

| Parameter | Meaning |
|-----------|---------|
| (none) | Print the report to the console |
| `-Out FILE` | Write the report to FILE (UTF-8, no byte order mark) |
| `-Format F` | Output format, when the util has more than one (CSV is the usual default) |
| `-Submit` | Hand the report to the agent: `drra_svc.exe submit --hook <name> <file>` |
| `-Now` | With `-Submit`: the agent sends it within seconds instead of with the next report |
| `-Hook ID` | Upload hook ID, default `<name>` |
| `-Drra PATH` | Agent program, default `C:\Program Files\DataReel\ReportingAgent\drra_svc.exe` (a test copy reads the `drra.ini` next to it) |
| `-Version`, `Get-Help <file> -Full` | |

Run a script with `powershell -ExecutionPolicy Bypass -File <name>.ps1`
(scripts are not signed). Exit status 0 when the report was made (and
submitted), 1 on an error. A util that remembers where it stopped (an
event record number) saves it only after the report was written or
submitted, so a failed submit is read again next time.

With `-Submit` the util always sends the format its DRSM manifest
reads, whatever `-Format` says. `drra_svc.exe submit` needs an
administrator: the upload folders are SYSTEM and Administrators only.

## Sending a report to the dashboard

1. On the monitor host, copy the manifest to DRSM:
   `cp -r utils/<name>/drsm/uploads/<name> <drsm>/plugins/uploads/`.
   The report server's `hook.php` reads it from `[hooks] manifest_dir`,
   and the computer's DRSM host file needs `UPLOADS_DIR` (the server's
   `[hooks] dir`) and `UPLOADS=auto`.
2. On the computer, from an administrator prompt:
   `powershell -ExecutionPolicy Bypass -File utils\install_util.ps1 <name> -Schedule`
   shows what it would do (dry run), `-Apply` does it. It copies the util
   to `C:\Program Files\DataReel\ReportingAgent\utils\<name>\`, the hook
   file to `uploads.d`, and with `-Schedule` adds the scheduled task
   `\DataReel\<name>` (SYSTEM, every 15 minutes, `-Submit`).
3. Run it once by hand with `-Submit -Now`, then
   `drra_svc.exe uploads` shows the hook's state.

## Adding a util

Copy `logon_watch\` as a start: the program with the parameters above,
its `README.md`, the `.ini`, the DRSM manifest (`FORMAT` is one of
`drsm-lines`, `kv-sections`, `json`, `csv`, `xccdf-results`, `text`,
`html`; the rules can only use values the format gives, so `csv` gives
only `rows`) and a test. Add it to the table at the top. The Linux
agent has the same layout in its `utils/`.
