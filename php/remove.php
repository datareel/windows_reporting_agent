<?php
// upload.php
// Check if the form was submitted

$DEBUG_MODE="true";
// $DEBUG_MODE="false";

if (isset($_POST['submit'])) {
    $hostname = $_POST['hostname'] ?? '';
    $hostname = strtolower($hostname);
    echo "Removing Windows report for hostname $hostname<br />";

    $filename = "/var/www/html/windows/reports/${hostname}_windows_report_agent.txt";

    if (file_exists($filename)) {
        unlink($filename);
       echo "Windows report file for hostname ${hostname} deleted successfully.<br />";
    }
       else 	    {
          echo "ERROR - Windows report file for hostname ${hostname} does not exist.<br />";
    }

}
?>
