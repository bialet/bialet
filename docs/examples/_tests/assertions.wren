// Test general assertion methods
// Demonstrates equals(), notContains(), and assertion chaining

// Test exact body match with /hi endpoint (returns HTML)
Test.get("/hi")
    .status(200)
    .equals("<h1>👋 Hello World</h1>")

// Test password hashing endpoint
Test.post("/password", {"password": "test123", "password-check": "test123"})
    .status(200)
    .contains("The passwords are the same")
    .contains("Hash:")

// Test negative case - passwords differ
Test.post("/password", {"password": "abc", "password-check": "xyz"})
    .status(200)
    .contains("The passwords are different")

// Test multiple assertions chained together
Test.get("/hi")
    .status(200)
    .contains("Hello")
    .contains("World")
    .notContains("Goodbye")

// Test notContains for things that should NOT be in response
Test.get("/hi")
    .status(200)
    .notContains("Error")
    .notContains("Failed")
