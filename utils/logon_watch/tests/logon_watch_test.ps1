<#
.SYNOPSIS
  Test of logon_watch.ps1 on a test computer: makes failed logons with a
  throwaway local account, checks how they are counted, the saved
  position and, with -AgentDir, -Submit through a test copy of the agent.

.DESCRIPTION
  Run as an administrator on a test computer only: it creates the local
  account lwtest (random password, deleted at the end) and logs failed
  logons for it. -Lockout makes 10 more, which locks lwtest out when
  the lockout threshold is 10 or less.

  -AgentDir is a folder with a copy of drra_svc.exe and a drra.ini next
  to it that points at a test report server (with the logon_watch
  manifest) and its own data_dir, uploads.d (with logon_watch.ini) and
  spool_dir. The installed agent is not used.

  Failed ssh logons from another computer, made before the test, show
  as remote rows; -ExpectRemote checks for them.

.EXAMPLE
  powershell -ExecutionPolicy Bypass -File tests\logon_watch_test.ps1 -Lockout -AgentDir C:\lw_agent
#>
param(
  [string]$AgentDir,
  [switch]$Lockout,
  [switch]$ExpectRemote,
  [switch]$KeepUser
)
$ErrorActionPreference = 'Continue'
$LW = Join-Path (Split-Path $PSScriptRoot) 'logon_watch.ps1'
$T = Join-Path ([IO.Path]::GetTempPath()) "logon_watch_test_$PID"
New-Item -ItemType Directory -Force $T | Out-Null
$failures = 0
function Pass($m) { "PASS - $m" }
function Fail($m) { "FAIL - $m"; $script:failures++ }
function Expect($m, $text, $want) {
  if (($text -join "`n").Contains($want)) { Pass $m } else { Fail "${m}: '$want' not found in:`n$($text -join "`n")" }
}
function Reject($m, $text, $want) {
  if (($text -join "`n").Contains($want)) { Fail "${m}: '$want' found" } else { Pass $m }
}
function Run-LW { & powershell -NoProfile -ExecutionPolicy Bypass -File $LW @args 2>&1 }

Add-Type -Namespace LwTest -Name Api -MemberDefinition '[DllImport("advapi32.dll", SetLastError=true, CharSet=CharSet.Unicode)] public static extern bool LogonUser(string u, string d, string p, int t, int pr, out System.IntPtr h);'
function Typo([int]$n) {
  $h = [IntPtr]::Zero
  for ($i = 0; $i -lt $n; $i++) { [LwTest.Api]::LogonUser('lwtest', '.', "wrong$i", 2, 0, [ref]$h) | Out-Null }
  # Windows writes the events a moment later
  Start-Sleep -Seconds 2
}

if (-not (Get-LocalUser lwtest -ErrorAction SilentlyContinue)) {
  $pw = ConvertTo-SecureString ('Lw' + [guid]::NewGuid().ToString('N').Substring(0, 10) + '!9') -AsPlainText -Force
  New-LocalUser -Name lwtest -Password $pw -Description 'logon_watch test account, delete me' | Out-Null
}
$state = Join-Path $T 'lw.state'

# Saved position: start now, nothing new, then one typo
$o = Run-LW -Since 1m -State $state
if ($LASTEXITCODE -ne 0) { Fail "first run: $o" }
$o = Run-LW -State $state
if (@($o).Count -eq 1) { Pass 'second run, nothing new' } else { Fail "second run read old events: $($o -join ' / ')" }
Typo 1
$o = Run-LW -State $state
Expect 'only the new typo' $o "local,failed,interactive,$($env:COMPUTERNAME)\lwtest,,bad_password,1,"
Expect 'state has the Security record' (Get-Content $state) 'record='
Expect 'state has the OpenSSH record' (Get-Content $state) 'ssh_record='

# Local typos, localhost network logon, lockout
Typo 2
cmd /c "net use \\127.0.0.1\IPC$ wrong /user:lwtest >nul 2>&1"
if ($Lockout) { Typo 10 } else { Start-Sleep -Seconds 2 }
$o = Run-LW -Since 1h -NoSave
Expect 'csv header' $o 'scope,event,logon_type,user,source,reason,count,first,last'
Expect 'local interactive typos' $o "local,failed,interactive,$($env:COMPUTERNAME)\lwtest,,bad_password,"
Expect 'localhost network logon' $o 'localhost,failed,network,lwtest,127.0.0.1,bad_password,'
Reject 'sshd address-less 4625 skipped' $o 'NOUSER'
if ($Lockout) { Expect 'lockout of lwtest' $o 'local,lockout,,lwtest,,locked_out,' }
if ($ExpectRemote) { Expect 'remote ssh failure' $o 'remote,failed,ssh_password,lwtest,' }

$l = Run-LW -Since 1h -NoSave -Format lines -Trusted '10.0.0.0/8'
Expect 'lines: local failures apart' $l 'DETAIL local_failed='
Expect 'lines: table' $l 'UPLOAD columns|Scope|Event|Logon type|User|Source|Reason|Count|First|Last'
Reject 'lines: no warnings in the payload' $l 'WARNING - '
if ($Lockout) {
  if (($l -join "`n") -match 'DETAIL local_lockouts=[1-9]') { Pass 'lines: local lockout value' } else { Fail 'lines: no local_lockouts' }
}
$out = Join-Path $T 'out.csv'
Run-LW -Since 1h -NoSave -Out $out | Out-Null
$bytes = if (Test-Path $out) { [IO.File]::ReadAllBytes($out) } else { @(0) }
if ($bytes[0] -eq 0x73) { Pass '-Out writes UTF-8 with no byte order mark' } else { Fail '-Out file starts with a byte order mark' }

if ($AgentDir) {
  $drra = Join-Path $AgentDir 'drra_svc.exe'
  $o = Run-LW -Since 1h -NoSave -Submit -Now -Drra $drra
  Expect 'submit queues the file' $o 'for upload hook logon_watch'
  Expect 'submit --now' $o 'within a few seconds'
  # The agent takes files a few seconds after they were last changed
  Start-Sleep -Seconds 4
  $o = & $drra send-uploads 2>&1
  Expect 'sent to hook.php' $o 'logon_watch'
  $o
  $o = Run-LW -Since 1h -NoSave -Submit -Hook nosuch -Drra $drra
  Expect 'unknown hook fails' $o 'submit failed'
}

if (-not $KeepUser) { Remove-LocalUser lwtest; 'lwtest deleted' }
Remove-Item -Recurse -Force $T
''
if ($failures -eq 0) { 'All tests passed' } else { "$failures test(s) failed" }
exit [int]($failures -ne 0)
