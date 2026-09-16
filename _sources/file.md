# File Handling

In Bialet, files are **always saved in the database**. Whether you are uploading
a file or creating one dynamically, Bialet provides an easy and efficient way to
manage file storage. Below are the three main parts of file handling in Bialet.

## File Uploading

When a file is uploaded through Bialet, it is automatically saved in the
database. By default, files handled through `Request.file()` are marked as
permanent. However, any file that is uploaded but not processed remains
temporary, meaning it will be deleted within a day or less.

**File Size Limits**: Bialet enforces a maximum file size per upload. Files
over the limit are skipped and the server logs the rejection with the maximum
it accepts. The per-file cap defaults to **4 MB** and is set at startup with
`-u` / `--max-upload-size`:

```bash
bialet -u 8MB /path/to/app
bialet --max-upload-size 2MB /path/to/app
```

The value is a byte count; a `K`, `M`, or `G` suffix (KiB/MiB/GiB) scales it.
`-u 4MB`, `-u 4096KB`, and `-u 4194304` are the same limit.

> ⚠️ Pitfall: an upload travels inside a multipart request body, and the whole
> body is capped by `-b` (default 128 KB) plus a memory-safe ceiling derived
> from `-m` (soft limit / 512). The *effective* per-file limit is the smaller
> of `-u` and that body cap minus ~8 KB of multipart framing. With the
> defaults that is about **120 KB**, not 4 MB, and raising `-u` alone does
> nothing until the body cap also goes up. Two rules of thumb:
>
> - effective body cap (bytes) ≈ `min(-b KB × 1024, -m × 2048)`
> - effective file limit ≈ `min(-u, body cap − 8 KB)`
>
> Raise `-b` (and `-m`/`-M`, which set the memory ceiling) to accept larger
> files:

```bash
# ~1 MB uploads
bialet -m 1024 -M 2048 -b 1100 -u 1MB /path/to/app

# ~4 MB uploads
bialet -m 2100 -M 4200 -b 4200 -u 4MB /path/to/app
```

A file larger than the body cap never reaches your code — the request is
answered with `413` before parsing and the server logs both sizes. A file over
the effective per-file limit is skipped and logged with the maximum it will
accept. Every accepted upload is stored in SQLite, so raising the limit also
grows the database and backups.

### Example 1: Permanent File Upload

```wren

if (Request.isPost) {
  // Check if a file was uploaded
  var file = Request.file("upload_file")
  if (!file) {
    return <p>No file was uploaded.</p>
  }

  // Return a message indicating the file was uploaded
  return <p>File uploaded and saved: {{ file.name }}</p>
}

return <!doctype html>
  <html>
    <body>
      <h1>Upload a File</h1>
      <form method="post" enctype="multipart/form-data">
        <input type="file" name="upload_file">
        <input type="submit" value="Upload">
      </form>
    </body>
  </html>
```

### Example 2: Temporary File Upload

There are times that the file is used just to process it. For that, you can mark
it as temporary and it will be deleted after a short period of time.

```wren
// Get the uploaded file and mark it as temporary
var file = Request.file("upload_file")
file.temp
System.print("Uploaded temporary file: %(file.name)")
```

> **Note:** Each file processed with `Request.file()` is saved as permanent by
> default. Files that are uploaded but not processed remain temporary and will
> be deleted within a day or less.

## File Creation

You can also dynamically create files and save them in the database. Created
files, like uploaded files, can be marked as temporary or permanent. Temporary
files will be automatically deleted after a short period of time. To manually
delete a file, use the `file.destroy` getter.

### Example 1: Creating a Permanent File

```wren

// Create a text file with dynamic content
var content = "This is a dynamically generated text file."
var file = File.create("example.txt", "text/plain", content)

// Send the file as a response
Response.file(file.id)
```

### Example 2: Deleting a File Manually

You can also delete files manually using the `file.destroy` getter. It
works for uploaded and dynamically created files.

```wren

// Create a text file with dynamic content
var content = "This is a temporary file that will be deleted."
var file = File.create("temporary.txt", "text/plain", content)

// Delete the file manually
file.destroy

return "<p>File was created and then deleted.</p>"
```

## Showing a File in the Browser

Once a file is saved in the database, the only way to show it in the browser is
by using `Response.file(id)`. This will take the file ID and return the file to
the browser.

You can also use a path-based route file (`<folder>.wren`) to dynamically
fetch files from your database and display them based on user input, such
as a filename.

### Example: Serving Files by Name from a Custom Table

In this example, we have a custom table `images` with fields `id`, `file_id`,
and `name`. We will fetch the file based on its name and display it in the
browser.

For more information on routing and dynamic names, see the
[advanced routing section](advanced-routing.md).

```wren

// Assume we have a route that takes the name as a parameter
var imageName = Request.route(0)

// Query the file_id from the custom table
var imageData = `SELECT file_id FROM images WHERE name = ?`.first(imageName)

if (imageData) {
  // Serve the file by its file_id
  Response.file(imageData["file_id"])
} else {
  // Return a 404 error if the image is not found
  Response.status(404)
  return "<h1>404 - File Not Found</h1>"
}
```
