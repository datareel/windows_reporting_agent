<?php
// status.php
// Fleet status page: every computer's last report checked against the
// [status] rules in server.ini (check-in age, Windows updates, Defender,
// required and unwanted programs). Read only.

require_once __DIR__ . '/wra_lib.php';
require_once __DIR__ . '/wra_page.php';

wra_require_viewer();
$cfg = wra_config();
$now = wra_reference_time();

$hosts = array();
$reports_dir = rtrim($cfg['paths']['reports_dir'], '/');
foreach (glob($reports_dir . '/*_windows_report_agent.txt') ?: array() as $path) {
    $hostname = substr(basename($path), 0, -strlen('_windows_report_agent.txt'));
    if (!wra_valid_hostname($hostname)) continue;
    $hosts[] = wra_host_status($hostname, $path, $now);
}
$rank = array('error' => 0, 'warning' => 1, 'ok' => 2);
usort($hosts, function ($a, $b) use ($rank) {
    return $rank[$a['state']] - $rank[$b['state']] ?: strcmp($a['hostname'], $b['hostname']);
});

$count = array('ok' => 0, 'warning' => 0, 'error' => 0);
$recent = 0;
foreach ($hosts as $h) {
    $count[$h['state']]++;
    if ($h['age'] !== null && $h['age'] <= 86400) $recent++;
}

wra_page_start('Fleet status');
?>
<h1>Fleet status</h1>
<p class="muted"><?php echo count($hosts); ?> computers. Status as of <?php echo h(gmdate('Y-m-d H:i', $now)); ?> UTC.
Late after <?php echo h($cfg['status']['late_hours']); ?> hours, missing after <?php echo h($cfg['status']['missing_days']); ?> days,
Windows updates older than <?php echo h($cfg['status']['patch_max_age_days']); ?> days are flagged.</p>

<div class="tiles" role="group" aria-label="Filter by state">
  <button class="tile active" data-filter="all"><b><?php echo count($hosts); ?></b>All computers</button>
  <button class="tile error" data-filter="error"><b><?php echo $count['error']; ?></b>Need attention</button>
  <button class="tile warning" data-filter="warning"><b><?php echo $count['warning']; ?></b>Warnings</button>
  <button class="tile ok" data-filter="ok"><b><?php echo $count['ok']; ?></b>OK</button>
  <div class="tile" style="cursor:default"><b><?php echo $recent; ?></b>Reported in 24 hours</div>
</div>

<div class="toolbar">
  <input type="search" id="q" placeholder="Search computer, user, service tag, IP or problem" aria-label="Search">
  <span class="muted" id="shown"></span>
</div>

<div class="table-wrap">
<table id="fleet">
<thead><tr><th>Computer</th><th>State</th><th>Last report</th><th class="hide-sm">Windows update</th>
<th class="hide-sm">Service tag</th><th class="hide-sm">IP address</th><th class="hide-sm">Users</th><th>Problems</th></tr></thead>
<tbody>
<?php foreach ($hosts as $h) {
    $r = $h['report'];
    $ip = ($r && $r['ips']) ? $r['ips'][0]['ip'] : '';
    $users = $r ? implode(', ', $r['users']) : '';
    $search = strtolower($h['hostname'] . ' ' . $users . ' ' . ($r ? $r['service_tag'] : '') . ' ' . $ip . ' '
        . implode(' ', array_map(function ($p) { return $p[1]; }, $h['problems'])));
?>
<tr data-state="<?php echo h($h['state']); ?>" data-search="<?php echo h($search); ?>">
  <td class="nowrap"><a href="report.php?host=<?php echo urlencode($h['hostname']); ?>"><?php echo h($h['hostname']); ?></a></td>
  <td><?php echo wra_state_label($h['state']); ?></td>
  <td class="nowrap" data-sort="<?php echo $h['age'] === null ? 999999999 : (int)$h['age']; ?>"><?php echo h(wra_age_text($h['age'])); ?></td>
  <td class="nowrap hide-sm"><?php echo h($h['last_update']); ?></td>
  <td class="mono hide-sm"><?php echo h($r ? $r['service_tag'] : ''); ?></td>
  <td class="mono hide-sm"><?php echo h($ip); ?></td>
  <td class="hide-sm"><?php echo h($users); ?></td>
  <td><?php if ($h['problems']) { ?><ul class="problems"><?php foreach ($h['problems'] as $p) { ?><li class="<?php echo h($p[0]); ?>"><?php echo h($p[1]); ?></li><?php } ?></ul><?php } ?></td>
</tr>
<?php } ?>
</tbody>
</table>
</div>

<script>
(function () {
  var filter = 'all', q = document.getElementById('q'), rows = document.querySelectorAll('#fleet tbody tr');
  function apply() {
    var term = q.value.trim().toLowerCase(), n = 0;
    rows.forEach(function (tr) {
      var show = (filter === 'all' || tr.dataset.state === filter) && (!term || tr.dataset.search.indexOf(term) >= 0);
      tr.style.display = show ? '' : 'none';
      if (show) n++;
    });
    document.getElementById('shown').textContent = n + ' shown';
  }
  document.querySelectorAll('.tile[data-filter]').forEach(function (b) {
    b.addEventListener('click', function () {
      document.querySelectorAll('.tile[data-filter]').forEach(function (x) { x.classList.remove('active'); });
      b.classList.add('active');
      filter = b.dataset.filter;
      apply();
    });
  });
  q.addEventListener('input', apply);
  // Sort by a column when its header is clicked
  document.querySelectorAll('#fleet th').forEach(function (th, i) {
    var asc = true;
    th.addEventListener('click', function () {
      var body = document.querySelector('#fleet tbody'), list = Array.prototype.slice.call(body.rows);
      list.sort(function (a, b) {
        var x = a.cells[i].dataset.sort || a.cells[i].textContent.trim().toLowerCase();
        var y = b.cells[i].dataset.sort || b.cells[i].textContent.trim().toLowerCase();
        if (a.cells[i].dataset.sort) { x = +x; y = +y; }
        return (x < y ? -1 : x > y ? 1 : 0) * (asc ? 1 : -1);
      });
      asc = !asc;
      list.forEach(function (tr) { body.appendChild(tr); });
    });
  });
  apply();
})();
</script>
<?php
wra_page_end();
?>
