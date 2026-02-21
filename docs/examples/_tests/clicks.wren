// Test click counter endpoint
// The counter is reset, then incremented, and the result is verified

// GET /clicks shows the counter page
Test.get("/clicks")
    .status(200)
    .contains("Total times clicked")

// Reset the counter to 0
Test.get("/clicks?reset")
    .status(200)
    .contains("Total times clicked")
    .contains("0")

// Increment the counter from 0 to 1
Test.get("/clicks?count")
    .status(200)
    .contains("Total times clicked")
    .contains("1")

// Reset brings it back to 0
Test.get("/clicks?reset")
    .status(200)
    .contains("0")

