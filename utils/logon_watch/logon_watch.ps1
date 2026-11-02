<#
.SYNOPSIS
  Failed logons and account lockouts since the last run, from the
  Windows Security log, remote apart from the computer's own.

.DESCRIPTION
  logon_watch reads Security log events 4625 (failed logon), 4740
  (account locked out) and 1102 (audit log cleared) after the position
  saved by the last run. Each event gets a scope:

    remote     from another address (logon type 3 network, 10 RDP, 8
               cleartext, ...), or a lockout caused by another computer
    trusted    the same, from an address or computer in -Trusted
    localhost  from 127.0.0.1 or ::1
    local      typed on this computer (logon type 2, 7 unlock, 11, ...),
               no address, and the audit log cleared events

  so the owner's typos do not look like an attack.

  It runs on its own (CSV table to the console or -Out FILE), or hands
  its result to the DataReel Reporting Agent with -Submit
  (drra_svc.exe submit --hook logon_watch), which sends it to the
  dashboard. Run it as an administrator (the Security log and the
  agent's upload queue are administrator only). See README.md.

.PARAMETER EvtxFile
  Read a saved .evtx file instead of the Security log; no saved position.
.PARAMETER Since
  Start here instead of the saved position: minutes ("30m"), hours
  ("24h"), days ("7d") or a date and time.
.PARAMETER State
  Saved position file.
.PARAMETER NoSave
  Do not save the new position.
.PARAMETER Trusted
  Addresses or computer names that count as trusted, not remote: IPv4
  CIDRs or addresses, IPv6 addresses, computer names.
.PARAMETER Format
  csv (default) or lines (DRSM lines: values and the table).
.PARAMETER Out
  Write to this file instead of the console.
.PARAMETER Submit
  Send the result with drra_svc.exe submit (always in the lines format
  the logon_watch manifest reads).
.PARAMETER Now
  With -Submit: the agent sends it within seconds.
.PARAMETER Hook
  Upload hook ID (default logon_watch).
.PARAMETER Drra
  Agent program (default C:\Program Files\DataReel\ReportingAgent\drra_svc.exe).

.EXAMPLE
  powershell -ExecutionPolicy Bypass -File logon_watch.ps1 -Since 7d -NoSave
.EXAMPLE
  powershell -ExecutionPolicy Bypass -File logon_watch.ps1 -Submit -Now
#>
[CmdletBinding()]
param(
  [string]$EvtxFile,
  [string]$Since,
  [string]$State,
  [switch]$NoSave,
  [string[]]$Trusted = @(),
  [ValidateSet('csv', 'lines')][string]$Format = 'csv',
  [string]$Out,
  [switch]$Submit,
  [switch]$Now,
  [string]$Hook = 'logon_watch',
  [string]$Drra = (Join-Path $env:ProgramFiles 'DataReel\ReportingAgent\drra_svc.exe'),
  [switch]$Version
)

$ErrorActionPreference = 'Stop'
$Program = 'logon_watch'
$ProgramVersion = '1.0'
if ($Version) { "$Program $ProgramVersion"; exit 0 }

function Fail([string]$msg) {
  [Console]::Error.WriteLine("${Program}: $msg")
  exit 1
}

$agentData = Join-Path $env:ProgramData 'DataReel\ReportingAgent'
if (-not $State) {
  # The agent's data folder is administrators only, so the position
  # cannot be moved by another user
  if (Test-Path $agentData) { $State = Join-Path $agentData "utils\$Program.state" }
  else { $State = Join-Path $env:LOCALAPPDATA "DataReel\utils\$Program.state" }
}
if ($Submit -and -not (Test-Path $Drra)) { Fail "agent program $Drra not found, install the agent or use -Drra" }
$isAdmin = ([Security.Principal.WindowsPrincipal][Security.Principal.WindowsIdentity]::GetCurrent()).IsInRole(
  [Security.Principal.WindowsBuiltInRole]::Administrator)
if (-not $EvtxFile -and -not $isAdmin) { Fail 'run as an administrator to read the Security log' }

# --------------------------------------------------------------------
# Which events: after the saved record number, or since -Since, or the
# last 24 hours on the first run. The newest record now is the end, so
# an event logged meanwhile waits for the next run.
# --------------------------------------------------------------------
function Since-Ms([string]$s) {
  if ($s -match '^(\d+)m$') { return [int64]$Matches[1] * 60000 }
  if ($s -match '^(\d+)h$') { return [int64]$Matches[1] * 3600000 }
  if ($s -match '^(\d+)d$') { return [int64]$Matches[1] * 86400000 }
  try { $t = [datetime]::Parse($s) } catch { Fail "-Since: '$s' is not 30m, 24h, 7d or a date" }
  return [int64]((Get-Date) - $t).TotalMilliseconds
}

$saved = @{}
if ((Test-Path $State) -and -not $EvtxFile) {
  foreach ($l in Get-Content $State) { if ($l -match '^([a-z_]+)=(.*)$') { $saved[$Matches[1]] = $Matches[2] } }
}
$NoSave = $NoSave -or [bool]$EvtxFile

# Events $ids of the log in $q after the record saved under $key
function Read-Events([hashtable]$q, [string]$ids, [string]$key) {
  $top = 0
  $reset = 0
  $last = 0
  if (-not $q.Path) {
    $newest = Get-WinEvent @q -MaxEvents 1 -ErrorAction SilentlyContinue
    if ($newest) { $top = $newest.RecordId }
  }
  if ($Since) {
    $cond = " and TimeCreated[timediff(@SystemTime) <= $(Since-Ms $Since)]"
  } elseif ($saved[$key] -match '^\d+$') {
    $last = [int64]$saved[$key]
    if ($top -lt $last) {
      # Record numbers start again after the log is cleared
      $reset = 1
      $last = 0
    }
    $cond = " and EventRecordID > $last"
  } else {
    $cond = ' and TimeCreated[timediff(@SystemTime) <= 86400000]'
  }
  if ($top -gt 0) { $cond += " and EventRecordID <= $top" }
  try {
    $ev = @(Get-WinEvent @q -FilterXPath "*[System[($ids)$cond]]" -Oldest -ErrorAction Stop)
  } catch {
    if ($_.FullyQualifiedErrorId -like 'NoMatchingEventsFound*') { $ev = @() }
    else { Fail "cannot read $($q.Values): $($_.Exception.Message)" }
  }
  if ($last -gt 0) {
    # Records after the saved one are gone: the log wrapped
    $oldest = Get-WinEvent @q -MaxEvents 1 -Oldest -ErrorAction SilentlyContinue
    if ($oldest -and $oldest.RecordId -gt $last + 1) { $reset = 1 }
  }
  if ($top -eq 0 -and $saved[$key]) { $top = $saved[$key] }
  return @{ Events = $ev; Top = $top; Reset = $reset }
}

$q = if ($EvtxFile) { @{ Path = $EvtxFile } } else { @{ LogName = 'Security' } }
$sec = Read-Events $q 'EventID=4625 or EventID=4740 or EventID=1102' 'record'
$events = $sec.Events
$logReset = $sec.Reset
# Windows OpenSSH logs its failures in the Security log as logon type 8
# with no address; its own log has the address, as on Linux
$sshLog = 'OpenSSH/Operational'
$ssh = @{ Events = @(); Top = 0; Reset = 0 }
$sshRead = (-not $EvtxFile) -and [bool](Get-WinEvent -ListLog $sshLog -ErrorAction SilentlyContinue)
if ($sshRead) { $ssh = Read-Events @{ LogName = $sshLog } 'EventID=4' 'ssh_record' }

# --------------------------------------------------------------------
# Classify
# --------------------------------------------------------------------
$logonTypes = @{
  '2' = 'interactive'; '3' = 'network'; '4' = 'batch'; '5' = 'service'; '7' = 'unlock'
  '8' = 'network_cleartext'; '9' = 'new_credentials'; '10' = 'remote_interactive'; '11' = 'cached_interactive'
}
$reasons = @{
  '0XC000006A' = 'bad_password'; '0XC0000064' = 'no_such_user'; '0XC0000234' = 'locked_out'
  '0XC0000072' = 'disabled'; '0XC000006F' = 'time_restriction'; '0XC0000070' = 'workstation_restriction'
  '0XC0000071' = 'password_expired'; '0XC0000193' = 'account_expired'; '0XC0000224' = 'must_change_password'
  '0XC000015B' = 'logon_type_not_granted'; '0XC0000133' = 'clock_skew'; '0XC000005E' = 'no_logon_server'
  '0XC0000413' = 'firewall_protected'; '0XC000006E' = 'account_restriction'
}

function Ip4([string]$s) {
  $a = $null
  if ($s -notmatch '^\d+\.\d+\.\d+\.\d+$' -or -not [Net.IPAddress]::TryParse($s, [ref]$a)) { return $null }
  $b = $a.GetAddressBytes()
  return ((([int64]$b[0] * 256 + $b[1]) * 256 + $b[2]) * 256 + $b[3])
}
$trustList = @($Trusted | ForEach-Object { $_ -split '[ ,]+' } | Where-Object { $_ })
function Test-Trusted([string]$src) {
  foreach ($t in $trustList) {
    if ($t -ieq $src) { return $true }
    $p = $t -split '/'
    if ($p.Count -eq 2) {
      $a = Ip4 $src; $b = Ip4 $p[0]
      if ($null -ne $a -and $null -ne $b) {
        $div = [Math]::Pow(2, 32 - [int]$p[1])
        if ([Math]::Floor($a / $div) -eq [Math]::Floor($b / $div)) { return $true }
      }
    }
  }
  return $false
}
function Get-Scope([string]$src) {
  if (-not $src -or $src -eq '-') { return 'local' }
  $s = $src -replace '^::ffff:', ''
  if ($s -match '^127\.' -or $s -eq '::1') { return 'localhost' }
  if ($s -ieq $env:COMPUTERNAME) { return 'local' }
  if (Test-Trusted $s) { return 'trusted' }
  return 'remote'
}
function Clean([string]$s) {
  if ($null -eq $s) { return '' }
  return ($s -replace '[\x00-\x1f\x7f|,"]', '_').Substring(0, [Math]::Min($s.Length, 64))
}

$groups = @{}
$totals = @{}
$sources = @{}
$nEvents = 0
function Add-Event($scope, $event, $ltype, $user, $src, $reason, [datetime]$time) {
  $k = ($scope, $event, (Clean $ltype), (Clean $user), (Clean $src), (Clean $reason)) -join '|'
  $ts = $time.ToString('yyyy-MM-ddTHH:mm:sszzz')
  if (-not $groups.ContainsKey($k)) { $groups[$k] = [pscustomobject]@{ Key = $k; Scope = $scope; Count = 0; First = $ts; Last = $ts } }
  $groups[$k].Count++
  $groups[$k].Last = $ts
  $totals["${scope}_$event"] = 1 + [int]$totals["${scope}_$event"]
  if ($event -ne 'log_cleared' -and $scope -ne 'local') { $sources["$scope|$src"] = 1 }
  $script:nEvents++
}

foreach ($e in $ssh.Events) {
  $m = $e.Message -replace '^[^:]+: ', ''
  if ($m -match '^Failed (\S+) for (invalid user )?(\S*) from (\S+) port \d+') {
    Add-Event (Get-Scope $Matches[4]) 'failed' "ssh_$($Matches[1])" $Matches[3] $Matches[4] 'sshd' $e.TimeCreated
  } elseif ($m -match '^Invalid user (\S*) ?from (\S+) port \d+') {
    Add-Event (Get-Scope $Matches[2]) 'invalid_user' 'ssh' $Matches[1] $Matches[2] 'sshd' $e.TimeCreated
  } elseif ($m -match '^(error: )?maximum authentication attempts exceeded for (invalid user )?(\S*) from (\S+) port \d+') {
    Add-Event (Get-Scope $Matches[4]) 'too_many' 'ssh' $Matches[3] $Matches[4] 'sshd' $e.TimeCreated
  } elseif ($m -match '^Connection (closed|reset) by authenticating user (\S*) (\S+) port \d+ \[preauth\]') {
    Add-Event (Get-Scope $Matches[3]) 'preauth_closed' 'ssh' $Matches[2] $Matches[3] 'sshd' $e.TimeCreated
  } elseif ($m -match '^Accepted (\S+) for (\S*) from (\S+) port \d+') {
    Add-Event (Get-Scope $Matches[3]) 'accepted' "ssh_$($Matches[1])" $Matches[2] $Matches[3] 'sshd' $e.TimeCreated
  }
}

foreach ($e in $events) {
  $x = [xml]$e.ToXml()
  $d = @{}
  foreach ($n in $x.Event.EventData.Data) { $d[$n.Name] = $n.'#text' }
  switch ($e.Id) {
    4625 {
      # sshd's failures, counted from its own log above
      if ($sshRead -and $d['ProcessName'] -match '\\sshd(-session)?\.exe$') { continue }
      $user = $d['TargetUserName']
      if ($d['TargetDomainName'] -and $d['TargetDomainName'] -ne '-') { $user = "$($d['TargetDomainName'])\$user" }
      $status = "$($d['Status'])".ToUpper()
      if ($status -eq '0XC000006D' -or -not $reasons.ContainsKey($status)) { $status = "$($d['SubStatus'])".ToUpper() }
      $reason = if ($reasons.ContainsKey($status)) { $reasons[$status] } else { $status.ToLower() }
      $ltype = if ($logonTypes.ContainsKey("$($d['LogonType'])")) { $logonTypes["$($d['LogonType'])"] } else { "$($d['LogonType'])" }
      $src = $d['IpAddress']
      $ws = $d['WorkstationName']
      # A network logon with no address (some NTLM paths) still names the
      # computer it came from
      if ((-not $src -or $src -eq '-') -and $d['LogonType'] -in '3', '8', '10' -and $ws -and $ws -ne '-' -and $ws -ine $env:COMPUTERNAME) {
        $src = $ws
      }
      $scope = Get-Scope $src
      if ($scope -eq 'local') { $src = '' }
      if ($scope -eq 'local' -and $d['ProcessName'] -match '\\sshd(-session)?\.exe$') {
        # No OpenSSH log to read: an ssh client, address unknown
        $scope = 'remote'
        $src = 'ssh_client'
      }
      $ev = if ($reason -eq 'no_such_user') { 'invalid_user' } else { 'failed' }
      Add-Event $scope $ev $ltype $user $src $reason $e.TimeCreated
    }
    4740 {
      # TargetDomainName is the computer the bad passwords came from
      $caller = $d['TargetDomainName']
      $scope = Get-Scope $caller
      if ($scope -eq 'local') { $caller = '' }
      Add-Event $scope 'lockout' '' $d['TargetUserName'] $caller 'locked_out' $e.TimeCreated
    }
    1102 {
      $u = $x.Event.UserData.LogFileCleared
      $who = if ($u) { "$($u.SubjectDomainName)\$($u.SubjectUserName)" } else { '' }
      Add-Event 'local' 'log_cleared' '' $who '' 'audit_log_cleared' $e.TimeCreated
    }
  }
}

# --------------------------------------------------------------------
# Output
# --------------------------------------------------------------------
$rank = @{ remote = 1; trusted = 2; localhost = 3; local = 4 }
$rows = @($groups.Values | Sort-Object @{ Expression = { $rank[$_.Scope] } }, @{ Expression = { $_.Count }; Descending = $true }, Key)
$values = [ordered]@{}
foreach ($s in 'remote', 'trusted', 'localhost', 'local') {
  foreach ($ev in 'failed', 'invalid_user', 'too_many', 'preauth_closed', 'accepted') {
    if ($s -ne 'local' -or $ev -in 'failed', 'invalid_user') { $values["${s}_$ev"] = [int]$totals["${s}_$ev"] }
  }
  $values["${s}_lockouts"] = [int]$totals["${s}_lockout"]
  if ($s -ne 'local') { $values["${s}_sources"] = @($sources.Keys | Where-Object { $_ -like "$s|*" }).Count }
}
$values['log_cleared'] = [int]$totals['local_log_cleared']
$values['log_reset'] = $logReset
$values['ssh_log'] = [int]$sshRead
$values['ssh_log_reset'] = $ssh.Reset
$values['events'] = $nEvents

function Format-Output([string]$fmt) {
  $lines = New-Object System.Collections.Generic.List[string]
  if ($fmt -eq 'csv') {
    $lines.Add('scope,event,logon_type,user,source,reason,count,first,last')
    foreach ($r in $rows) { $lines.Add((($r.Key -replace '\|', ','), $r.Count, $r.First, $r.Last) -join ',') }
    return $lines
  }
  $first = ($rows | ForEach-Object { $_.First } | Sort-Object | Select-Object -First 1)
  $last = ($rows | ForEach-Object { $_.Last } | Sort-Object | Select-Object -Last 1)
  $src = if ($EvtxFile) { 'evtx' } else { 'security_log' }
  $lines.Add("INFO - $Program $ProgramVersion on $($env:COMPUTERNAME): $($values.remote_failed) remote failed logons, " +
    "$($values.remote_invalid_user) unknown users from $($values.remote_sources) sources, " +
    "$($values.remote_lockouts + $values.local_lockouts + $values.trusted_lockouts + $values.localhost_lockouts) lockouts; " +
    "$($values.localhost_failed) localhost and $($values.local_failed) local failures not counted as remote")
  foreach ($k in $values.Keys) { $lines.Add("DETAIL $k=$($values[$k])") }
  $lines.Add("DETAIL source=$src")
  $lines.Add("DETAIL first_event=$(if ($first) { $first } else { 'none' })")
  $lines.Add("DETAIL last_event=$(if ($last) { $last } else { 'none' })")
  $lines.Add("DETAIL collected=$((Get-Date).ToString('yyyy-MM-ddTHH:mm:sszzz'))")
  $lines.Add('UPLOAD columns|Scope|Event|Logon type|User|Source|Reason|Count|First|Last')
  foreach ($r in $rows) { $lines.Add("UPLOAD row|$($r.Key)|$($r.Count)|$($r.First)|$($r.Last)") }
  return $lines
}

function Write-Lines([string]$path, $lines) {
  # UTF-8 without a byte order mark: the server reads the first line as is
  [IO.File]::WriteAllLines($path, [string[]]$lines, (New-Object Text.UTF8Encoding($false)))
}

$output = Format-Output $Format
if ($Out) {
  try { Write-Lines ([IO.Path]::GetFullPath([IO.Path]::Combine((Get-Location).Path, $Out))) $output } catch { Fail "cannot write ${Out}: $($_.Exception.Message)" }
} elseif (-not $Submit) {
  $output
}

if ($Submit) {
  $payload = Join-Path ([IO.Path]::GetTempPath()) ("${Program}_$($env:COMPUTERNAME)_$((Get-Date).ToString('yyyyMMddHHmmss')).txt")
  Write-Lines $payload (Format-Output 'lines')
  $drraArgs = @('submit', '--hook', $Hook)
  if ($Now) { $drraArgs += '--now' }
  & $Drra @drraArgs $payload
  $rc = $LASTEXITCODE
  Remove-Item $payload -ErrorAction SilentlyContinue
  if ($rc -ne 0) { Fail 'drra_svc.exe submit failed, the position is not saved' }
}

if (-not $NoSave) {
  try {
    New-Item -ItemType Directory -Force -Path (Split-Path $State) | Out-Null
    Write-Lines "$State.tmp" @("record=$([int64]$sec.Top)", "ssh_record=$([int64]$ssh.Top)", "saved=$((Get-Date).ToString('s'))")
    Move-Item -Force "$State.tmp" $State
  } catch { Fail "cannot save the position in ${State}: $($_.Exception.Message)" }
}
exit 0
