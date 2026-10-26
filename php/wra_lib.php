<?php
// wra_lib.php
// Shared code for register.php, upload.php and admin.php
//
// Agents sign every request with their RSA key. The server keeps one
// registration record per hostname with the agent certificate, and only
// accepts reports from approved agents whose signature checks out.

function wra_config() {
    static $cfg = null;
    if ($cfg !== null) {
        return $cfg;
    }

    $defaults = array(
        'paths' => array(
            'reports_dir' => '/var/www/html/windows/reports',
            'data_dir' => '/var/lib/windows_reporting_agent',
        ),
        'registration' => array(
            'enrollment_token' => '',
            'auto_approve_with_token' => '1',
        ),
        'upload' => array(
            'max_upload_bytes' => '2000000',
            'max_clock_skew_seconds' => '300',
        ),
        'admin' => array(
            'require_remote_user' => '1',
            'login' => 'basic',
            'drsm_config' => '',
        ),
        'status' => array(
            'late_hours' => '24',
            'missing_days' => '15',
            'patch_max_age_days' => '45',
            'required_apps' => '',
            'unwanted_apps' => '',
            'required_apps_skip_hosts' => '',
            'public' => '0',
            'reference_time' => '',
        ),
    );

    // Set with "SetEnv WRA_SERVER_INI /path/server.ini" in the web server.
    // Some PHP handlers only pass it through $_SERVER.
    $ini_path = getenv('WRA_SERVER_INI');
    if (($ini_path === false || $ini_path === '') && isset($_SERVER['WRA_SERVER_INI'])) {
        $ini_path = $_SERVER['WRA_SERVER_INI'];
    }
    if ($ini_path === false || $ini_path === '') {
        $ini_path = '/etc/windows_reporting_agent/server.ini';
    }
    // A relative path is relative to this directory, so the same .htaccess
    // works wherever the pages are installed (shared hosting, the demo)
    if ($ini_path[0] !== '/') {
        $ini_path = __DIR__ . '/' . $ini_path;
    }
    $ini = array();
    if (is_readable($ini_path)) {
        $ini = parse_ini_file($ini_path, true, INI_SCANNER_RAW);
        if ($ini === false) {
            $ini = array();
        }
    }

    $cfg = $defaults;
    foreach ($defaults as $section => $keys) {
        foreach ($keys as $key => $value) {
            if (isset($ini[$section][$key])) {
                $cfg[$section][$key] = trim($ini[$section][$key]);
            }
        }
    }
    // Relative directories in server.ini are relative to the server.ini file
    foreach (array('reports_dir', 'data_dir') as $key) {
        $dir = $cfg['paths'][$key];
        if ($dir !== '' && $dir[0] !== '/') {
            $cfg['paths'][$key] = dirname($ini_path) . '/' . $dir;
        }
    }
    return $cfg;
}

// Answer an agent request. Agents read the "STATUS: <word>" line.
function wra_reply($http_code, $status, $message = '') {
    http_response_code($http_code);
    header('Content-Type: text/plain; charset=utf-8');
    echo "STATUS: $status\n";
    if ($message !== '') {
        echo "$message\n";
    }
    exit;
}

function wra_valid_hostname($hostname) {
    return is_string($hostname) && preg_match('/^[a-z0-9][a-z0-9_-]{0,62}$/', $hostname) === 1;
}

function wra_ensure_dir($dir) {
    if (!is_dir($dir) && !mkdir($dir, 0750, true) && !is_dir($dir)) {
        return false;
    }
    return true;
}

function wra_agents_dir() {
    $cfg = wra_config();
    return rtrim($cfg['paths']['data_dir'], '/') . '/agents';
}

function wra_report_path($hostname) {
    $cfg = wra_config();
    return rtrim($cfg['paths']['reports_dir'], '/') . '/' . $hostname . '_windows_report_agent.txt';
}

function wra_load_agent($hostname) {
    $path = wra_agents_dir() . '/' . $hostname . '.json';
    if (!is_file($path)) {
        return null;
    }
    $record = json_decode(file_get_contents($path), true);
    return is_array($record) ? $record : null;
}

function wra_save_agent($record) {
    $dir = wra_agents_dir();
    if (!wra_ensure_dir($dir)) {
        return false;
    }
    $path = $dir . '/' . $record['hostname'] . '.json';
    return wra_write_atomic($path, json_encode($record, JSON_PRETTY_PRINT | JSON_UNESCAPED_SLASHES) . "\n", 0640);
}

function wra_delete_agent($hostname) {
    $path = wra_agents_dir() . '/' . $hostname . '.json';
    return !is_file($path) || unlink($path);
}

function wra_list_agents() {
    $agents = array();
    foreach (glob(wra_agents_dir() . '/*.json') ?: array() as $path) {
        $record = json_decode(file_get_contents($path), true);
        if (is_array($record) && isset($record['hostname'])) {
            $agents[$record['hostname']] = $record;
        }
    }
    ksort($agents);
    return $agents;
}

// Write to a temporary file and rename it, so readers never see a partial file
function wra_write_atomic($path, $data, $mode) {
    $tmp = $path . '.tmp' . bin2hex(random_bytes(4));
    if (file_put_contents($tmp, $data) === false) {
        return false;
    }
    chmod($tmp, $mode);
    if (!rename($tmp, $path)) {
        unlink($tmp);
        return false;
    }
    return true;
}

// SHA-256 of the DER public key (SubjectPublicKeyInfo). The agent logs the
// same value, so an admin can match the two before approving.
function wra_key_fingerprint($cert_pem) {
    $pub = openssl_pkey_get_public($cert_pem);
    if ($pub === false) {
        return false;
    }
    $details = openssl_pkey_get_details($pub);
    if ($details === false || !isset($details['key'])) {
        return false;
    }
    $b64 = preg_replace('/-----[A-Z ]+-----|\s+/', '', $details['key']);
    return hash('sha256', base64_decode($b64));
}

function wra_verify_signature($cert_pem, $data, $signature_b64) {
    $signature = base64_decode($signature_b64, true);
    if ($signature === false || $signature === '') {
        return false;
    }
    $pub = openssl_pkey_get_public($cert_pem);
    if ($pub === false) {
        return false;
    }
    return openssl_verify($data, $signature, $pub, OPENSSL_ALGO_SHA256) === 1;
}

function wra_check_timestamp($timestamp) {
    $cfg = wra_config();
    if (!is_string($timestamp) || !ctype_digit($timestamp)) {
        return false;
    }
    return abs(time() - (int)$timestamp) <= (int)$cfg['upload']['max_clock_skew_seconds'];
}

function wra_valid_nonce($nonce) {
    return is_string($nonce) && preg_match('/^[0-9a-f]{32}$/', $nonce) === 1;
}

// Record a nonce so the same signed request cannot be sent twice.
// Returns false if the nonce was already used.
function wra_use_nonce($hostname, $nonce) {
    $cfg = wra_config();
    $dir = rtrim($cfg['paths']['data_dir'], '/') . '/nonces';
    if (!wra_ensure_dir($dir)) {
        return false;
    }
    $fp = fopen($dir . '/' . $hostname . '.json', 'c+');
    if ($fp === false) {
        return false;
    }
    flock($fp, LOCK_EX);
    $nonces = json_decode(stream_get_contents($fp), true);
    if (!is_array($nonces)) {
        $nonces = array();
    }

    // Requests older than the clock skew window are rejected anyway
    $cutoff = time() - 2 * (int)$cfg['upload']['max_clock_skew_seconds'];
    foreach ($nonces as $n => $t) {
        if ($t < $cutoff) {
            unset($nonces[$n]);
        }
    }

    $fresh = !isset($nonces[$nonce]);
    if ($fresh) {
        $nonces[$nonce] = time();
        ftruncate($fp, 0);
        rewind($fp);
        fwrite($fp, json_encode($nonces));
        fflush($fp);
    }
    flock($fp, LOCK_UN);
    fclose($fp);
    return $fresh;
}

function wra_post($name) {
    return isset($_POST[$name]) && is_string($_POST[$name]) ? $_POST[$name] : '';
}

function wra_remote_addr() {
    return isset($_SERVER['REMOTE_ADDR']) ? $_SERVER['REMOTE_ADDR'] : '';
}

// ------------------------------------------------------------------ reports

// Parse an agent report into its parts. Returns null for an empty file.
function wra_parse_report($text) {
    if (trim($text) === '') {
        return null;
    }
    $r = array('version' => '', 'hostname' => '', 'etime' => null, 'datetime' => '',
               'service_tag' => '', 'asset_tag' => '', 'ips' => array(), 'users' => array(),
               'apps' => array(), 'defender' => array(), 'patches' => array(), 'hotfixes' => array());
    foreach (array('VERSION' => 'version', 'HOSTNAME' => 'hostname', 'DATETIME' => 'datetime') as $k => $f) {
        if (preg_match('/^' . $k . ': *(.*)$/m', $text, $m)) {
            $r[$f] = trim($m[1]);
        }
    }
    if (preg_match('/^ETIME: *(\d+)/m', $text, $m)) {
        $r['etime'] = (int)$m[1];
    }
    $sections = array();
    if (preg_match_all('/^([A-Z][A-Z ]+):\n(.*?)^END \1$/ms', $text, $mm, PREG_SET_ORDER)) {
        foreach ($mm as $m) {
            $sections[$m[1]] = $m[2];
        }
    }
    $sec = function ($name) use ($sections) { return isset($sections[$name]) ? $sections[$name] : ''; };

    if (preg_match('/Service Tag \(Serial Number\): *(\S*)/', $sec('SERVICE TAG AND BARCODE'), $m)) $r['service_tag'] = $m[1];
    if (preg_match('/Asset Tag \(Barcode\): *(\S*)/', $sec('SERVICE TAG AND BARCODE'), $m)) $r['asset_tag'] = $m[1];
    if (preg_match_all('/^Adapter: *(.*)\nIPv4 Address: *(\S+)/m', $sec('IP ADDRESS'), $mm, PREG_SET_ORDER)) {
        foreach ($mm as $m) $r['ips'][] = array('adapter' => trim($m[1]), 'ip' => $m[2]);
    }
    $skip = array('all users', 'default', 'default user', 'public');
    if (preg_match_all('/^\[DIR\] +(.+)$/m', $sec('USERS'), $mm)) {
        foreach ($mm[1] as $u) if (!in_array(strtolower(trim($u)), $skip, true)) $r['users'][] = trim($u);
    }
    if (preg_match_all('/^Name: (.*?) \| Version: *(.*)$/m', $sec('ALL INSTALLED PROGRAMS'), $mm, PREG_SET_ORDER)) {
        foreach ($mm as $m) $r['apps'][] = array('name' => trim($m[1]), 'version' => trim($m[2]));
    }
    if (preg_match_all('/<Event .*?<\/Event>/s', $sec('WINDOWS DEFENDER ALERTS'), $mm)) {
        foreach ($mm[0] as $ev) {
            $e = array('id' => 0, 'level' => 0, 'time' => '', 'threat' => '', 'action' => '', 'path' => '');
            if (preg_match('/<EventID>(\d+)/', $ev, $m)) $e['id'] = (int)$m[1];
            if (preg_match('/<Level>(\d+)/', $ev, $m)) $e['level'] = (int)$m[1];
            if (preg_match("/SystemTime='([^']+)'/", $ev, $m)) $e['time'] = $m[1];
            if (preg_match("/<Data Name='Threat Name'>([^<]*)/", $ev, $m)) $e['threat'] = html_entity_decode($m[1], ENT_QUOTES);
            if (preg_match("/<Data Name='Action Name'>([^<]*)/", $ev, $m)) $e['action'] = html_entity_decode($m[1], ENT_QUOTES);
            if (preg_match("/<Data Name='Path'>([^<]*)/", $ev, $m)) $e['path'] = html_entity_decode($m[1], ENT_QUOTES);
            $r['defender'][] = $e;
        }
    }
    if (preg_match_all('/^Date: +(\d+)-(\d+)-(\d+)\n Patch: (.*)$/m', $sec('INSTALLED WINDOWS PATCHES'), $mm, PREG_SET_ORDER)) {
        foreach ($mm as $m) $r['patches'][] = array('date' => sprintf('%04d-%02d-%02d', $m[1], $m[2], $m[3]), 'title' => trim($m[4]));
    }
    if (preg_match_all('/^Hotfix ID: *(\S+)\nDescription: *(.*)\nInstalled By: *(.*)\nInstalled On: *(.*)$/m', $sec('HOT FIXES'), $mm, PREG_SET_ORDER)) {
        foreach ($mm as $m) $r['hotfixes'][] = array('id' => $m[1], 'description' => trim($m[2]), 'by' => trim($m[3]), 'on' => trim($m[4]));
    }
    return $r;
}

// Defender event IDs that need attention: malware detected, action failed,
// real-time protection disabled, antimalware engine or tamper protection off
function wra_defender_problem($event_id) {
    return in_array($event_id, array(1006, 1015, 1116, 1118, 1119, 5001, 5008, 5010, 5012), true);
}

function wra_defender_label($event_id) {
    $labels = array(1002 => 'Scan stopped', 1006 => 'Malware found', 1015 => 'Suspicious behavior',
                    1116 => 'Malware detected', 1117 => 'Action taken', 1118 => 'Action failed',
                    1119 => 'Action failed', 5001 => 'Real-time protection disabled',
                    5008 => 'Engine failure', 5010 => 'Spyware scanning disabled', 5012 => 'Virus scanning disabled');
    return isset($labels[$event_id]) ? $labels[$event_id] : 'Event ' . $event_id;
}

// Comma separated list of program name patterns (* and ? wildcards)
function wra_pattern_list($list) {
    $out = array();
    foreach (explode(',', $list) as $p) {
        $p = trim($p, " \t\"'");
        if ($p !== '') $out[] = $p;
    }
    return $out;
}

function wra_app_matches($apps, $pattern) {
    foreach ($apps as $a) {
        if (fnmatch($pattern, $a['name'], FNM_CASEFOLD)) return $a['name'];
    }
    return false;
}

// Latest Windows cumulative or security update date (Y-m-d), or ''
function wra_last_windows_update($patches) {
    $last = '';
    foreach ($patches as $p) {
        if (preg_match('/Cumulative Update for Windows|^\d{4}-\d{2} Security Update/i', $p['title']) && $p['date'] > $last) {
            $last = $p['date'];
        }
    }
    return $last;
}

// Check one computer's report against the [status] rules.
// Returns array('host', 'state' => ok|warning|error, 'problems' => list, 'report' => parsed or null, ...)
function wra_host_status($hostname, $path, $now) {
    $cfg = wra_config();
    $st = $cfg['status'];
    $text = @file_get_contents($path);
    $h = array('hostname' => $hostname, 'state' => 'ok', 'problems' => array(), 'report' => null,
               'age' => null, 'last_update' => '', 'alerts' => 0, 'file' => basename($path));
    $add = function ($level, $msg) use (&$h) {
        $h['problems'][] = array($level, $msg);
        if ($level === 'error' || ($level === 'warning' && $h['state'] === 'ok')) $h['state'] = $level;
    };
    $r = ($text === false) ? null : wra_parse_report($text);
    if ($r === null) {
        $add('error', 'Empty report: the agent is not installed or has never reported');
        return $h;
    }
    $h['report'] = $r;
    if ($r['etime'] === null) {
        $add('error', 'Bad report: no time stamp');
    }
    else {
        $h['age'] = max(0, $now - $r['etime']);
        $days = floor($h['age'] / 86400);
        if ($h['age'] > (int)$st['missing_days'] * 86400) {
            $add('error', "No report for $days days");
        }
        else if ($h['age'] > (int)$st['late_hours'] * 3600) {
            $add('warning', 'Last report ' . ($days >= 1 ? "$days days" : floor($h['age'] / 3600) . ' hours') . ' ago');
        }
    }
    $h['last_update'] = wra_last_windows_update($r['patches']);
    $ref = $r['etime'] !== null ? $r['etime'] : $now;
    if ($h['last_update'] === '') {
        $add('warning', 'No Windows updates in the patch history');
    }
    else {
        $age = floor(($ref - strtotime($h['last_update'] . ' 12:00:00 UTC')) / 86400);
        if ($age > (int)$st['patch_max_age_days']) {
            $add('warning', "Last Windows update $age days before the report (" . $h['last_update'] . ')');
        }
    }
    foreach ($r['defender'] as $e) {
        if (wra_defender_problem($e['id'])) {
            $h['alerts']++;
        }
    }
    $seen = array();
    foreach ($r['defender'] as $e) {
        if (!wra_defender_problem($e['id'])) continue;
        $msg = 'Defender: ' . wra_defender_label($e['id']) . ($e['threat'] !== '' ? ' ' . $e['threat'] : '');
        if (isset($seen[$msg])) continue;
        $seen[$msg] = 1;
        $add('error', $msg);
    }
    $skip_required = false;
    foreach (wra_pattern_list($st['required_apps_skip_hosts']) as $p) {
        if (fnmatch($p, $hostname, FNM_CASEFOLD)) $skip_required = true;
    }
    foreach ($skip_required ? array() : wra_pattern_list($st['required_apps']) as $p) {
        if (wra_app_matches($r['apps'], $p) === false) {
            $add('warning', 'Missing required program: ' . rtrim($p, '*'));
        }
    }
    foreach (wra_pattern_list($st['unwanted_apps']) as $p) {
        $name = wra_app_matches($r['apps'], $p);
        if ($name !== false) {
            $add('warning', 'Unwanted program: ' . $name);
        }
    }
    return $h;
}

// Time the status page measures report ages from. A demo can set
// reference_time to a fixed time (epoch), or to "fleet" to use the time a
// sample fleet was generated (fleet.json in reports_dir).
function wra_reference_time() {
    $cfg = wra_config();
    $ref = $cfg['status']['reference_time'];
    if ($ref === 'fleet') {
        $f = rtrim($cfg['paths']['reports_dir'], '/') . '/fleet.json';
        $j = is_file($f) ? json_decode(file_get_contents($f), true) : null;
        if (is_array($j) && isset($j['generated'])) return (int)$j['generated'];
    }
    else if (ctype_digit($ref)) {
        return (int)$ref;
    }
    return time();
}

function wra_age_text($seconds) {
    if ($seconds === null) return 'unknown';
    if ($seconds < 3600) return max(1, floor($seconds / 60)) . ' min ago';
    if ($seconds < 86400) return floor($seconds / 3600) . ' h ago';
    return floor($seconds / 86400) . ' days ago';
}
?>
