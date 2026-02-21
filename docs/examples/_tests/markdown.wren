// Test Markdown rendering endpoint

// GET /markdown renders Markdown content as HTML
Test.get("/markdown")
    .status(200)
    .contains("Markdown Example")

// The Markdown is converted to HTML headings
Test.get("/markdown")
    .status(200)
    .contains("<h1>")
