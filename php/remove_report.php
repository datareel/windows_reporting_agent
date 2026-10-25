<!-- index.html -->
<!DOCTYPE html>
<html>
<head>
    <title>Remove Report</title>
</head>
<body>
    <h2>Remove a Windows Report</h2>
    <form action="remove.php" method="post" enctype="multipart/form-data">
        <label>Enter the hostname of the report to remove</label>
        <input type="text" name="hostname" id="hostname" required>
        <br><br>
        <input type="submit" value="Remove Host" name="submit">
    </form>
</body>
</html>
