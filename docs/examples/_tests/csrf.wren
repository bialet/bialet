// Test CSRF protection endpoint

// GET /csrf shows the form with CSRF token
Test.get("/csrf")
    .status(200)
    .contains("Cross-Site Request Forgery")
    .contains("<form")
    .notContains("The post is valid")

// POST /csrf without valid CSRF token - shows invalid result
Test.post("/csrf", {})
    .status(200)
    .contains("The post come from someone else")
