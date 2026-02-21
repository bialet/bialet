// Test file upload endpoint

// GET /upload shows the upload form
Test.get("/upload")
    .status(200)
    .contains("Upload File")
    .contains("<form")
    .contains("multipart/form-data")
