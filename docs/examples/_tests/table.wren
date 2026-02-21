// Test counter table display endpoint

// GET /table shows all counters from the database
Test.get("/table")
    .status(200)
    .contains("Show all the counters")

// Insert a counter and verify it appears in the table
`INSERT OR REPLACE INTO counter (name, value) VALUES ('alpha', 42)`.query()

Test.get("/table")
    .status(200)
    .contains("alpha")
    .contains("42")

