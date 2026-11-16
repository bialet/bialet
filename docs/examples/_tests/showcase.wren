// Test the "showcase" example: the smallest complete Bialet app
// It creates its table, renders a form, and saves a posted item.

Test.get("/showcase")
    .status(200)
    .contains("Welcome to Bialet showcase")
    .contains("<form method=\"post\"")
    .contains("name=\"input\"")

// An item posted through the form shows up on the next render
Test.post("/showcase", {"input": "a leftover from the test suite"})
    .status(200)
    .contains("a leftover from the test suite")

Test.get("/showcase")
    .status(200)
    .contains("<li>a leftover from the test suite</li>")

// The template escapes the posted value instead of injecting it
Test.post("/showcase", {"input": "<script>alert(1)</script>"})
    .status(200)
    .notContains("<script>alert(1)</script>")
    .contains("&lt;script&gt;")
