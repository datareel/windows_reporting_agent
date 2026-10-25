<?php
// upload.php
// Accepts a report only from an approved agent, signed with the key of the
// certificate it registered through register.php. The report is always
// saved as <hostname>_windows_report_agent.txt for the signing host, so
// one agent cannot overwrite another host's report.
//
// Request fields: hostname, timestamp, nonce, signature (base64 RSA SHA-256
// over "upload\n<hostname>\n<timestamp>\n<nonce>\n<sha256 hex of report>")
// and the report file as uploadedFile.

require_once __DIR__ . '/wra_lib.php';

if ($_SERVER['REQUEST_METHOD'] !== 'POST') {
    wra_reply(405, 'error', 'POST required');
}

$cfg = wra_config();
$hostname = strtolower(wra_post('hostname'));
$timestamp = wra_post('timestamp');
$nonce = wra_post('nonce');
$signature = wra_post('signature');

if (!wra_valid_hostname($hostname)) {
    wra_reply(400, 'error', 'Invalid hostname');
}
if (!wra_valid_nonce($nonce)) {
    wra_reply(400, 'error', 'Invalid nonce');
}
if (!wra_check_timestamp($timestamp)) {
    wra_reply(403, 'bad_timestamp', 'Agent clock differs too much from the server clock');
}

$record = wra_load_agent($hostname);
if ($record === null) {
    wra_reply(403, 'not_registered', "Host $hostname is not registered");
}
if ($record['status'] !== 'approved') {
    wra_reply(403, $record['status'], "Host $hostname is " . $record['status']);
}

if (!isset($_FILES['uploadedFile']) || $_FILES['uploadedFile']['error'] !== UPLOAD_ERR_OK) {
    $code = isset($_FILES['uploadedFile']) ? $_FILES['uploadedFile']['error'] : 'missing';
    wra_reply(400, 'error', "File upload failed, error code $code");
}
if ($_FILES['uploadedFile']['size'] > (int)$cfg['upload']['max_upload_bytes']) {
    wra_reply(413, 'error', 'Report is too large');
}
$report = file_get_contents($_FILES['uploadedFile']['tmp_name']);
if ($report === false) {
    wra_reply(500, 'error', 'Could not read the uploaded file');
}

$signed = "upload\n$hostname\n$timestamp\n$nonce\n" . hash('sha256', $report);
if (!wra_verify_signature($record['cert_pem'], $signed, $signature)) {
    wra_reply(403, 'bad_signature', 'Signature check failed');
}
if (!wra_use_nonce($hostname, $nonce)) {
    wra_reply(403, 'replay', 'Request was already used');
}

$reports_dir = $cfg['paths']['reports_dir'];
if (!wra_ensure_dir($reports_dir)) {
    wra_reply(500, 'error', 'Reports directory is missing');
}
if (!wra_write_atomic(wra_report_path($hostname), $report, 0644)) {
    wra_reply(500, 'error', 'Could not save the report');
}

$record['last_seen'] = gmdate('Y-m-d H:i:s') . ' UTC';
$record['last_addr'] = wra_remote_addr();
wra_save_agent($record);

wra_reply(200, 'ok', "Report for $hostname saved");
?>
