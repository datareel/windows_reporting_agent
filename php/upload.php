<?php
// upload.php
// Check if the form was submitted

$DEBUG_MODE="true";
// $DEBUG_MODE="false";

if (isset($_POST['submit'])) {
    // Define the target directory where files will be stored
    $target_dir = "/var/www/html/windows/reports/";

    // Create the uploads folder if it doesn't exist (optional)
    if (!is_dir($target_dir)) {
        mkdir($target_dir, 3777, true);
    }

    // Get the file details from the $_FILES superglobal
    $fileTmpPath  = $_FILES['uploadedFile']['tmp_name'];
    $fileName     = $_FILES['uploadedFile']['name'];
    $fileSize     = $_FILES['uploadedFile']['size'];
    $fileType     = $_FILES['uploadedFile']['type'];
    $fileError    = $_FILES['uploadedFile']['error'];

    // Specify the target file path
    $target_file = $target_dir . basename($fileName);

    if($DEBUG_MODE == "true") {
      echo "DEBUG output<p>\n";
      echo "fileTmpPath = $fileTmpPath<br>\n";
      echo "fileName = $fileName<br>\n";
      echo "fileSize = $fileSize<br>\n";
      echo "fileType = $fileType<br>\n";
      echo "fileError = $fileError<br>\n";
      echo "</p>\n";
    }

    // Basic server-side validation and error checking
    if ($fileError === UPLOAD_ERR_OK) {
        // You should implement further validation here (file type, size, etc.)
        // Example: Limit file size to 2MB
        if ($fileSize > 2000000) {
            echo "<p>ERROR - Sorry, your file is too large.</p>\n";
        } else {
            // Move the file from temporary storage to the permanent destination
            if (move_uploaded_file($fileTmpPath, $target_file)) {
                chmod($target_file, 0775);
	        echo "<p>The file ". htmlspecialchars( basename( $fileName)). " has been uploaded.</p>\n";
            } else {
                echo "<p>ERROR - Sorry, there was an error uploading your file.</p>\n";
            }
        }
    } else {
        echo "<p>ERROR - An error occurred during the file upload. Error code: " . $fileError . "</p>\n";
    }
}
?>
