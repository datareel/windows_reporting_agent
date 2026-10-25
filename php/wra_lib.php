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
?>
