// Test users list endpoint
// Migration inserts users: Alice (id=1), Bob (id=2), Charlie (id=3)

// GET /simple shows all users from the database
Test.get("/simple")
    .status(200)
    .contains("Users list")
    .contains("Alice")
    .contains("Bob")
    .contains("Charlie")

// Page links to /hello with user ids
Test.get("/simple")
    .status(200)
    .contains("/hello?id=1")
    .contains("/hello?id=2")
    .contains("/hello?id=3")
