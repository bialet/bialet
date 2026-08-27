// Test path-based route (folder.wren)
// Migration inserts users: Alice (id=1), Bob (id=2), Charlie (id=3)

// GET /users renders the list at the bare folder URL
Test.get("/users")
    .status(200)
    .contains("Users")
    .contains("Alice")
    .contains("Bob")
    .contains("Charlie")

// GET /users/:id renders the detail page for that segment
Test.get("/users/1")
    .status(200)
    .contains("Alice")
    .contains("User id: 1")

Test.get("/users/2")
    .status(200)
    .contains("Bob")

// Unknown id returns 404
Test.get("/users/999")
    .status(404)
