<?php
// admin.php
// Approve, revoke or delete agent registrations, and delete reports.
//
// Put this page behind web server authentication, for example in Apache:
//
//   <Files "admin.php">
//     AuthType Basic
//     AuthName "Windows Reporting Agent admin"
//     AuthUserFile /etc/windows_reporting_agent/admin.htpasswd
//     Require valid-user
//   </Files>
//
// With require_remote_user = 1 (the default) the page refuses to run when
// the web server has not authenticated the user.

require_once __DIR__ . '/wra_lib.php';

$cfg = wra_config();
$admin_user = isset($_SERVER['REMOTE_USER']) ? $_SERVER['REMOTE_USER'] : '';
if ($cfg['admin']['require_remote_user'] === '1' && $admin_user === '') {
    http_response_code(403);
    echo "admin.php must be protected by web server authentication (REMOTE_USER is not set).\n";
    exit;
}
if ($admin_user === '') {
    $admin_user = 'admin';
}

function h($str) {
    return htmlspecialchars((string)$str, ENT_QUOTES, 'UTF-8');
}

session_start();
if (empty($_SESSION['wra_csrf'])) {
    $_SESSION['wra_csrf'] = bin2hex(random_bytes(16));
}
$csrf = $_SESSION['wra_csrf'];

$message = '';
if ($_SERVER['REQUEST_METHOD'] === 'POST') {
    $action = wra_post('action');
    $hostname = strtolower(wra_post('hostname'));

    if (!hash_equals($csrf, wra_post('csrf'))) {
        $message = 'ERROR - Form expired, please try again.';
    }
    else if (!wra_valid_hostname($hostname)) {
        $message = 'ERROR - Invalid hostname.';
    }
    else {
        $record = wra_load_agent($hostname);
        $now = gmdate('Y-m-d H:i:s') . ' UTC';
        if (($action === 'approve' || $action === 'revoke') && $record === null) {
            $message = "ERROR - Host $hostname is not registered.";
        }
        else if ($action === 'approve') {
            $record['status'] = 'approved';
            $record['approved_at'] = $now;
            $record['approved_by'] = $admin_user;
            $message = wra_save_agent($record) ? "Approved $hostname." : "ERROR - Could not save $hostname.";
        }
        else if ($action === 'revoke') {
            $record['status'] = 'revoked';
            $record['revoked_at'] = $now;
            $record['revoked_by'] = $admin_user;
            $message = wra_save_agent($record) ? "Revoked $hostname." : "ERROR - Could not save $hostname.";
        }
        else if ($action === 'delete') {
            $message = wra_delete_agent($hostname) ? "Deleted the registration for $hostname. The agent will register again on its next report." : "ERROR - Could not delete $hostname.";
        }
        else if ($action === 'delete_report') {
            $path = wra_report_path($hostname);
            if (!is_file($path)) {
                $message = "ERROR - No report for $hostname.";
            }
            else {
                $message = unlink($path) ? "Deleted the report for $hostname." : "ERROR - Could not delete the report for $hostname.";
            }
        }
        else {
            $message = 'ERROR - Unknown action.';
        }
    }
}

$agents = wra_list_agents();

// Reports on disk with no registration, for example from older agents
$orphan_reports = array();
$reports_dir = rtrim($cfg['paths']['reports_dir'], '/');
foreach (glob($reports_dir . '/*_windows_report_agent.txt') ?: array() as $path) {
    $hostname = substr(basename($path), 0, -strlen('_windows_report_agent.txt'));
    if (!isset($agents[$hostname])) {
        $orphan_reports[] = $hostname;
    }
}

function action_button($action, $hostname, $label, $csrf, $confirm = '') {
    $onsubmit = $confirm !== '' ? ' onsubmit="return confirm(\'' . h($confirm) . '\')"' : '';
    return '<form method="post" style="display:inline"' . $onsubmit . '>'
        . '<input type="hidden" name="csrf" value="' . h($csrf) . '">'
        . '<input type="hidden" name="action" value="' . h($action) . '">'
        . '<input type="hidden" name="hostname" value="' . h($hostname) . '">'
        . '<input type="submit" value="' . h($label) . '"></form> ';
}
?>
<!DOCTYPE html>
<html>
<head>
    <title>Windows Reporting Agent admin</title>
    <style>
        body { font-family: sans-serif; }
        table { border-collapse: collapse; }
        th, td { border: 1px solid #ccc; padding: 4px 8px; text-align: left; vertical-align: top; }
        .fp { font-family: monospace; font-size: 85%; word-break: break-all; max-width: 22em; }
        .approved { color: #070; } .pending { color: #a60; } .revoked { color: #a00; }
    </style>
</head>
<body>
    <h2>Windows Reporting Agent admin</h2>
    <p>Signed in as <?php echo h($admin_user); ?>.
       Before approving, check that the key fingerprint matches the one in
       the agent log file on that computer.</p>
<?php if ($message !== '') { ?>
    <p><b><?php echo h($message); ?></b></p>
<?php } ?>

    <h3>Agents (<?php echo count($agents); ?>)</h3>
    <table>
        <tr><th>Hostname</th><th>Status</th><th>Key fingerprint (SHA-256)</th>
            <th>Registered</th><th>Approved</th><th>Last report</th><th>Last address</th><th>Actions</th></tr>
<?php foreach ($agents as $hostname => $a) { ?>
        <tr>
            <td><?php echo h($hostname); ?></td>
            <td class="<?php echo h($a['status']); ?>"><?php echo h($a['status']); ?></td>
            <td class="fp"><?php echo h($a['key_fingerprint']); ?></td>
            <td><?php echo h($a['registered_at']); ?></td>
            <td><?php echo h($a['approved_at'] !== '' ? $a['approved_at'] . ' by ' . $a['approved_by'] : ''); ?></td>
            <td><?php echo h($a['last_seen']); ?></td>
            <td><?php echo h($a['last_addr']); ?></td>
            <td>
<?php
    if ($a['status'] !== 'approved') {
        echo action_button('approve', $hostname, 'Approve', $csrf);
    }
    if ($a['status'] !== 'revoked') {
        echo action_button('revoke', $hostname, 'Revoke', $csrf, "Revoke $hostname?");
    }
    echo action_button('delete', $hostname, 'Delete registration', $csrf, "Delete the registration for $hostname?");
    if (is_file(wra_report_path($hostname))) {
        echo action_button('delete_report', $hostname, 'Delete report', $csrf, "Delete the report for $hostname?");
    }
?>
            </td>
        </tr>
<?php } ?>
    </table>

<?php if (count($orphan_reports) > 0) { ?>
    <h3>Reports with no registration</h3>
    <table>
        <tr><th>Hostname</th><th>Actions</th></tr>
<?php foreach ($orphan_reports as $hostname) { ?>
        <tr><td><?php echo h($hostname); ?></td>
            <td><?php echo action_button('delete_report', $hostname, 'Delete report', $csrf, "Delete the report for $hostname?"); ?></td></tr>
<?php } ?>
    </table>
<?php } ?>
</body>
</html>
