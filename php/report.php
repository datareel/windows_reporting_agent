<?php
// report.php
// One computer's last report: problems, hardware, users, Defender events,
// installed programs, Windows updates and hotfixes. ?raw=1 shows the
// report file as text. Read only.

require_once __DIR__ . '/wra_lib.php';
define('WRA_AUTH_PAGE', 'status');
require __DIR__ . '/wra_auth.php';
require_once __DIR__ . '/wra_page.php';
$hostname = isset($_GET['host']) && is_string($_GET['host']) ? strtolower($_GET['host']) : '';
if (!wra_valid_hostname($hostname) || !is_file(wra_report_path($hostname))) {
    http_response_code(404);
    wra_page_start('Not found');
    echo '<h1>Not found</h1><p>There is no report for that computer. <a href="status.php">Back to the fleet</a></p>';
    wra_page_end();
    exit;
}
$path = wra_report_path($hostname);

if (isset($_GET['raw'])) {
    header('Content-Type: text/plain; charset=utf-8');
    header('X-Content-Type-Options: nosniff');
    readfile($path);
    exit;
}

$h = wra_host_status($hostname, $path, wra_reference_time());
$r = $h['report'];

wra_page_start($hostname);
?>
<p><a href="status.php">&larr; All computers</a></p>
<h1><?php echo h($hostname); ?> <?php echo wra_state_label($h['state']); ?></h1>
<p class="muted">Last report <?php echo h(wra_age_text($h['age'])); ?><?php if ($r && $r['datetime'] !== '') echo ' (' . h($r['datetime']) . ', computer time)'; ?>.
<a href="report.php?host=<?php echo urlencode($hostname); ?>&amp;raw=1">Report file</a></p>

<?php if ($h['problems']) { ?>
<div class="card"><ul class="problems"><?php foreach ($h['problems'] as $p) { ?><li class="<?php echo h($p[0]); ?>"><?php echo h($p[1]); ?></li><?php } ?></ul></div>
<?php } ?>

<?php if ($r) { ?>
<h2>Computer</h2>
<div class="card"><dl class="facts">
  <div><dt>Service tag</dt><dd class="mono"><?php echo h($r['service_tag'] !== '' ? $r['service_tag'] : '-'); ?></dd></div>
  <div><dt>Asset tag</dt><dd class="mono"><?php echo h($r['asset_tag'] !== '' ? $r['asset_tag'] : '-'); ?></dd></div>
  <div><dt>IP addresses</dt><dd><?php foreach ($r['ips'] as $ip) echo '<span class="mono">' . h($ip['ip']) . '</span> <span class="muted">' . h($ip['adapter']) . '</span><br>'; ?></dd></div>
  <div><dt>User profiles</dt><dd><?php echo h($r['users'] ? implode(', ', $r['users']) : '-'); ?></dd></div>
  <div><dt>Last Windows update</dt><dd><?php echo h($h['last_update'] !== '' ? $h['last_update'] : '-'); ?></dd></div>
  <div><dt>Agent version</dt><dd><?php echo h($r['version']); ?></dd></div>
</dl></div>

<?php if ($r['performance'] || $r['disks'] || $r['nics']) {
    $pf = $r['performance'];
    $v = function ($k, $suffix = '') use ($pf) { return isset($pf[$k]) ? h($pf[$k] . $suffix) : '-'; };
    $up = isset($pf['uptime_seconds']) ? (int)$pf['uptime_seconds'] : null;
?>
<h2>Usage</h2>
<div class="card"><dl class="facts">
  <div><dt>CPU used</dt><dd><?php echo $v('cpu_used_pct', ' %'); ?> <span class="muted">of <?php echo $v('cpu_count'); ?> CPUs</span></dd></div>
  <div><dt>Memory used</dt><dd><?php echo $v('mem_used_pct', ' %'); ?> <span class="muted"><?php echo $v('mem_available_mb'); ?> of <?php echo $v('mem_total_mb'); ?> MB free</span></dd></div>
  <div><dt>Commit charge</dt><dd><?php echo $v('commit_used_mb'); ?> <span class="muted">of <?php echo $v('commit_limit_mb'); ?> MB</span></dd></div>
  <div><dt>Processes</dt><dd><?php echo $v('process_count'); ?></dd></div>
  <div><dt>Up for</dt><dd><?php echo $up === null ? '-' : h(floor($up / 86400) . ' days ' . floor(($up % 86400) / 3600) . ' h'); ?></dd></div>
</dl></div>
<?php if ($r['disks']) { ?>
<div class="table-wrap" style="margin-top:12px"><table>
<thead><tr><th>Drive</th><th>Size MB</th><th>Used MB</th><th>Free MB</th><th>Used</th></tr></thead><tbody>
<?php foreach ($r['disks'] as $d) { ?><tr><td class="mono"><?php echo h(isset($d['drive']) ? $d['drive'] : ''); ?></td><td><?php echo h(isset($d['size_mb']) ? $d['size_mb'] : ''); ?></td><td><?php echo h(isset($d['used_mb']) ? $d['used_mb'] : ''); ?></td><td><?php echo h(isset($d['free_mb']) ? $d['free_mb'] : ''); ?></td><td><?php echo h(isset($d['used_pct']) ? $d['used_pct'] . ' %' : ''); ?></td></tr><?php } ?>
</tbody></table></div>
<?php } ?>
<?php if ($r['nics']) { ?>
<div class="table-wrap" style="margin-top:12px"><table>
<thead><tr><th>Adapter</th><th>Speed</th><th>Received</th><th>Sent</th><th>Errors</th><th>Discards</th></tr></thead><tbody>
<?php foreach ($r['nics'] as $n) {
    $g = function ($k) use ($n) { return isset($n[$k]) ? $n[$k] : '0'; }; ?>
<tr><td><?php echo h($g('adapter')); ?></td><td><?php echo h($g('speed_mbps') . ' Mb/s'); ?></td>
<td><?php echo h(round((float)$g('rx_bytes') / 1048576, 1) . ' MB'); ?></td><td><?php echo h(round((float)$g('tx_bytes') / 1048576, 1) . ' MB'); ?></td>
<td><?php echo h(((int)$g('rx_errors') + (int)$g('tx_errors'))); ?></td><td><?php echo h(((int)$g('rx_discards') + (int)$g('tx_discards'))); ?></td></tr>
<?php } ?>
</tbody></table></div>
<p class="muted">Network counters are totals since the adapter came up.</p>
<?php } ?>
<?php } ?>

<h2>Windows Defender (<?php echo count($r['defender']); ?>)</h2>
<?php if ($r['defender']) { ?>
<div class="table-wrap"><table>
<thead><tr><th>Time (UTC)</th><th>Event</th><th>Threat</th><th>Action</th><th class="hide-sm">Path</th></tr></thead><tbody>
<?php foreach ($r['defender'] as $e) { ?>
<tr><td class="nowrap"><?php echo h(substr(str_replace('T', ' ', $e['time']), 0, 16)); ?></td>
<td><?php echo wra_defender_problem($e['id']) ? '<span class="pill error">' . h(wra_defender_label($e['id'])) . '</span>' : h(wra_defender_label($e['id'])); ?></td>
<td><?php echo h($e['threat']); ?></td><td><?php echo h($e['action']); ?></td><td class="mono hide-sm"><?php echo h($e['path']); ?></td></tr>
<?php } ?>
</tbody></table></div>
<?php } else { ?><p class="muted">No critical, error or warning events.</p><?php } ?>

<h2>Installed programs (<?php echo count($r['apps']); ?>)</h2>
<div class="toolbar"><input type="search" id="appq" placeholder="Filter programs" aria-label="Filter programs"></div>
<div class="table-wrap"><table id="apps">
<thead><tr><th>Name</th><th>Version</th></tr></thead><tbody>
<?php $apps = $r['apps']; usort($apps, function ($a, $b) { return strcasecmp($a['name'], $b['name']); });
foreach ($apps as $a) { ?><tr><td><?php echo h($a['name']); ?></td><td class="mono"><?php echo h($a['version']); ?></td></tr><?php } ?>
</tbody></table></div>

<h2>Windows updates (<?php echo count($r['patches']); ?>)</h2>
<div class="table-wrap"><table>
<thead><tr><th>Date</th><th>Update</th></tr></thead><tbody>
<?php foreach ($r['patches'] as $p) { ?><tr><td class="nowrap"><?php echo h($p['date']); ?></td><td><?php echo h($p['title']); ?></td></tr><?php } ?>
</tbody></table></div>

<h2>Hotfixes (<?php echo count($r['hotfixes']); ?>)</h2>
<div class="table-wrap"><table>
<thead><tr><th>Hotfix</th><th>Description</th><th>Installed on</th><th class="hide-sm">Installed by</th></tr></thead><tbody>
<?php foreach ($r['hotfixes'] as $x) { ?><tr><td class="mono"><?php echo h($x['id']); ?></td><td><?php echo h($x['description']); ?></td><td><?php echo h($x['on']); ?></td><td class="hide-sm"><?php echo h($x['by']); ?></td></tr><?php } ?>
</tbody></table></div>
<script>
document.getElementById('appq').addEventListener('input', function () {
  var t = this.value.toLowerCase();
  document.querySelectorAll('#apps tbody tr').forEach(function (tr) {
    tr.style.display = tr.textContent.toLowerCase().indexOf(t) >= 0 ? '' : 'none';
  });
});
</script>
<?php } ?>
<?php
wra_page_end();
?>
