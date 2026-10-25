<!-- index.html -->
<!DOCTYPE html>
<html>
<head>
    <title>Upload Windows Report File</title>
</head>
<body>
    <h2>Upload a Windows Report File</h2>
    <form action="upload.php" method="post" enctype="multipart/form-data">
        <label>Select file to upload:</label>
        <!-- The 'name' attribute value ('uploadedFile') is the key for the $_FILES array in PHP -->
        <input type="file" name="uploadedFile" id="uploadedFile" required>
        <br><br>
        <input type="submit" value="Upload File" name="submit">
    </form>
</body>
</html>
