<?php
// wra_auth.php
// Who may open admin.php, status.php and report.php. Include it at file
// scope (not inside a function) after wra_lib.php, and define
// WRA_AUTH_PAGE first: 'admin' or 'status'. Sets $wra_viewer to the
// signed in user name.
//
// [admin] login in server.ini picks the login:
//   basic  web server authentication (.htaccess AuthType Basic), the
//          page needs REMOTE_USER unless require_remote_user = 0
//   drsm   the DataReel System Monitor dashboard login: the pages must be
//          under the dashboard's web folder (they share its session
//          cookie), and drsm_config points at the dashboard's
//          config/drsm.php. Users, lockouts and sign out are the
//          dashboard's.

if (isset($_SERVER['SCRIPT_FILENAME']) && realpath($_SERVER['SCRIPT_FILENAME']) === __FILE__) {
    http_response_code(404);
    exit;
}

$wra_auth_cfg = wra_config();
$wra_viewer = '';

if (WRA_AUTH_PAGE === 'status' && $wra_auth_cfg['status']['public'] === '1') {
    // Demo with sample data: no login
}
else if ($wra_auth_cfg['admin']['login'] === 'drsm') {
    $wra_drsm_config = $wra_auth_cfg['admin']['drsm_config'];
    if ($wra_drsm_config !== '' && $wra_drsm_config[0] !== '/') {
        $wra_drsm_config = __DIR__ . '/' . $wra_drsm_config;
    }
    if ($wra_drsm_config === '' || !is_readable($wra_drsm_config)) {
        http_response_code(500);
        echo "login = drsm needs drsm_config in server.ini to point at the dashboard's config/drsm.php.\n";
        exit;
    }
    // Loaded here at file scope, so the dashboard settings are globals as
    // its code expects. drsm_lib.php sends anyone not signed in to the
    // dashboard login page, which brings them back here afterwards.
    require_once $wra_drsm_config;
    require_once "{$WWWdir}/php/drsm_lib.php";
    $wra_viewer = (isset($AUTHuser) && $AUTHuser !== '') ? $AUTHuser : 'dashboard';
}
else {
    $wra_viewer = isset($_SERVER['REMOTE_USER']) ? (string)$_SERVER['REMOTE_USER'] : '';
    if ($wra_auth_cfg['admin']['require_remote_user'] === '1' && $wra_viewer === '') {
        http_response_code(403);
        echo "This page must be protected by web server authentication (REMOTE_USER is not set).\n";
        exit;
    }
    if ($wra_viewer === '') {
        $wra_viewer = 'admin';
    }
}
?>
