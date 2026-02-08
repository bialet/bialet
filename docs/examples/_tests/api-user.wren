// Test fluent JSON assertion methods
// Demonstrates jsonContains, jsonEquals, jsonNotContains

// Test JSON object response with fluent assertions
Test.apiGet("/api-user")
    .status(200)
    .jsonContains("id")
    .jsonContains("name")
    .jsonContains("email")
    .jsonContains("active")

// Test specific JSON values
Test.apiGet("/api-user")
    .status(200)
    .jsonEquals("id", 1)
    .jsonEquals("name", "Test User")
    .jsonEquals("email", "test@example.com")
    .jsonEquals("active", true)

// Test that sensitive fields are NOT present
Test.apiGet("/api-user")
    .status(200)
    .jsonNotContains("password")
    .jsonNotContains("ssn")
    .jsonNotContains("api_key")

// Can chain all types of JSON assertions together
Test.apiGet("/api-user")
    .status(200)
    .jsonContains("id")
    .jsonEquals("name", "Test User")
    .jsonNotContains("password")
