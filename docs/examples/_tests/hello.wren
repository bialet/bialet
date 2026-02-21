// Test hello endpoint with database user lookup
// Migration inserts users: Alice (id=1), Bob (id=2), Charlie (id=3)

// GET /hello without id falls back to "World"
Test.get("/hello")
    .status(200)
    .contains("Hello")
    .contains("World")

// GET /hello?id=1 fetches Alice from the database
Test.get("/hello?id=1")
    .status(200)
    .contains("Hello")
    .contains("Alice")

// GET /hello?id=2 fetches Bob from the database
Test.get("/hello?id=2")
    .status(200)
    .contains("Hello")
    .contains("Bob")

// GET /hello?id=3 fetches Charlie from the database
Test.get("/hello?id=3")
    .status(200)
    .contains("Charlie")

// Non-existent id falls back to "World"
Test.get("/hello?id=999")
    .status(200)
    .contains("Hello")
    .contains("World")
