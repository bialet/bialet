// Test general assertion methods
// Demonstrates equals(), notContains(), and assertion chaining

// Test exact body match with /hi endpoint (returns HTML)
Test.get("/hi")
    .status(200)
    .equals("<h1>👋 Hello World</h1>")

// Test password hashing endpoint
Test.post("/password", {"password": "secret", "password-check": "secret"})
    .status(200)
    .contains("The passwords are the same")

// Test multiple assertions chained together
Test.get("/hi")
    .status(200)
    .contains("Hello")
    .contains("World")
    .contains("👋")

// Test notContains for things that should NOT be in response
Test.get("/hi")
    .status(200)
    .notContains("Goodbye")
    .notContains("Error")
    .notContains("404")
