// Test visit counter endpoint
// Each GET request increments the counter

// GET /visits returns 200 with the counter page
Test.get("/visits")
    .status(200)
    .contains("Total times visited")
    .contains("times visited")

// Subsequent visits keep incrementing (counter > 0)
Test.get("/visits")
    .status(200)
    .contains("Total times visited")

