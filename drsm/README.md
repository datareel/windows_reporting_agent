# DRSM plugins for the Windows Reporting Agent

These files add Windows hosts to the DataReel System Monitor (DRSM).
DRSM reads the report each agent sends (by HTTPS upload, CIFS share or
SCP) and runs five checks on it, without ever connecting to the
Windows host:

| Plugin | Checks |
| --- | --- |
| `windows/checkin` | Host reported recently (warning after 48 hours, error after 15 days), complete report, agent version |
| `windows/defender` | Defender threat detections, real-time protection turned off, failed updates, old definitions |
| `windows/patches` | Age of the newest Windows patch (warning after 45 days, error after 90) |
| `windows/apps` | Required, forbidden and warning program lists |
| `windows/resources` | CPU, memory, commit charge, disk space per drive and network use (agent 3.3 and later), with the same metrics as Linux hosts for the DRSM gauges and trends |

Results show in the DRSM host reports, dashboard and email/text
alerts like any other DRSM plugin. This replaces
`bash/run_windows_laptop_inventory.sh` for DRSM users.

## Install

Needs DRSM 1.31 or later. DRSM 1.32 and later include these files, so
skip this step there.

```
$ drsm/install.sh            # lists what would be copied
$ drsm/install.sh --apply
```

## Set up

1. In `~/drsm/etc/hosts/_defaults.conf`, point DRSM at the directory
   the reports land in: `reports_dir` from `php/server.ini`, or the
   CIFS share or SCP directory:

   ```
   WRA_REPORTS_DIR=/var/www/html/windows/reports
   ```

   The user that runs DRSM needs read access to the reports.

2. Create one host file per reporting host (dry run first):

   ```
   $ ~/drsm/utils/wra_hosts.sh
   $ ~/drsm/utils/wra_hosts.sh --apply
   ```

   Or copy `etc/hosts/windows.conf_example` by hand. A Windows host
   file sets `PLATFORM=windows`, `COLLECTOR=wra` and `PROBES=""`.

3. Run once without alerts and read the reports:

   ```
   $ ~/drsm/bin/drsm_run.sh -n -H laptop1
   ```

Thresholds and program lists are plugin settings, for example
`windows_checkin_CRIT_DAYS=30` or
`windows_apps_REQUIRED="Microsoft Defender;Mozilla Firefox"`, set in
`_defaults.conf` or a host file. See `plugins/windows/*/plugin.conf`
and the Windows hosts section of the DRSM `PLUGINS.md`.

The main copy of these files is in the dr_system_monitor repository.
Maintainers refresh this folder with `drsm/sync_from_drsm.sh`.
