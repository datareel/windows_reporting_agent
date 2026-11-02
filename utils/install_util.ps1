<#
.SYNOPSIS
  Installs a util from utils\<name>\ on a computer with the agent.

.DESCRIPTION
  Copies the util to C:\Program Files\DataReel\ReportingAgent\utils\<name>\
  and its upload hook file to
  C:\ProgramData\DataReel\ReportingAgent\uploads.d\, and with -Schedule
  adds a scheduled task DataReel\<name> that runs it as SYSTEM every
  15 minutes with -Submit. A dry run unless -Apply is given. The DRSM
  manifest (utils\<name>\drsm\uploads\<name>) goes on the monitor host
  by hand.

.EXAMPLE
  powershell -ExecutionPolicy Bypass -File utils\install_util.ps1 logon_watch -Schedule
.EXAMPLE
  powershell -ExecutionPolicy Bypass -File utils\install_util.ps1 logon_watch -Schedule -Apply
#>
param(
  [Parameter(Mandatory = $true, Position = 0)][ValidatePattern('^[a-z0-9_]{1,32}$')][string]$Name,
  [switch]$Schedule,
  [int]$Minutes = 15,
  [switch]$Apply
)
$ErrorActionPreference = 'Stop'
$src = Join-Path $PSScriptRoot $Name
$program = Join-Path $src "$Name.ps1"
if (-not (Test-Path $program)) { Write-Error "No $program"; exit 2 }
$dest = Join-Path $env:ProgramFiles "DataReel\ReportingAgent\utils\$Name"
$uploadsD = Join-Path $env:ProgramData 'DataReel\ReportingAgent\uploads.d'
$ini = Join-Path $src "$Name.ini"

function Step([string]$what, [scriptblock]$do) {
  if ($Apply) { "+ $what"; & $do } else { "would: $what" }
}

if ($Apply) {
  $admin = ([Security.Principal.WindowsPrincipal][Security.Principal.WindowsIdentity]::GetCurrent()).IsInRole(
    [Security.Principal.WindowsBuiltInRole]::Administrator)
  if (-not $admin) { Write-Error 'Run -Apply as an administrator'; exit 1 }
} else {
  'Dry run, nothing is changed (add -Apply)'
}

Step "copy $program to $dest" {
  New-Item -ItemType Directory -Force $dest | Out-Null
  Copy-Item -Force $program $dest
  Copy-Item -Force (Join-Path $src 'README.md') $dest -ErrorAction SilentlyContinue
}
if (Test-Path $ini) {
  if (-not (Test-Path $uploadsD)) {
    "WARNING - $uploadsD is missing: install the agent (3.3-beta2 or later) first, or use $Name without -Submit"
  } elseif (Test-Path (Join-Path $uploadsD "$Name.ini")) {
    "$uploadsD\$Name.ini is there already, left as it is"
  } else {
    Step "copy $ini to $uploadsD" { Copy-Item $ini $uploadsD }
  }
}
if ($Schedule) {
  $file = Join-Path $dest "$Name.ps1"
  $arg = "-NoProfile -ExecutionPolicy Bypass -File `"$file`" -Submit"
  Step "scheduled task \DataReel\$Name every $Minutes minutes as SYSTEM: powershell.exe $arg" {
    $action = New-ScheduledTaskAction -Execute 'powershell.exe' -Argument $arg
    $trigger = New-ScheduledTaskTrigger -Once -At (Get-Date).AddMinutes(1) -RepetitionInterval (New-TimeSpan -Minutes $Minutes)
    $settings = New-ScheduledTaskSettingsSet -ExecutionTimeLimit (New-TimeSpan -Minutes 10) -StartWhenAvailable
    Register-ScheduledTask -Force -TaskPath '\DataReel\' -TaskName $Name -Action $action -Trigger $trigger `
      -Settings $settings -User 'SYSTEM' -RunLevel Highest | Out-Null
  }
}
"DRSM manifest for the monitor host: $src\drsm\uploads\$Name"
