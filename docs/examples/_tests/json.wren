// Test JSON endpoint responses
// The /json endpoint returns an array of counters from the database

// Basic JSON response validation
Test.apiGet("/json")
    .status(200)
    .header("Content-Type")  // Just check it exists, value includes charset

// Get and validate JSON structure
var data = Test.apiGet("/json").status(200).json()
Test.assert(data is List, "Expected JSON array")

// Test with data - first insert some test counters
`INSERT OR REPLACE INTO counter (name, value) VALUES ('test1', 10)`.query()
`INSERT OR REPLACE INTO counter (name, value) VALUES ('test2', 20)`.query()

// Now test the response contains our data
var counters = Test.apiGet("/json").status(200).json()
Test.assert(counters.count >= 2, "Expected at least 2 counters")

// Find and validate specific counter
// Note: SQLite returns numbers as strings in JSON
var found = false
for (counter in counters) {
  if (counter["name"] == "test1") {
    found = true
    // Value is returned as string "10", not number 10
    Test.assert(counter["value"] == "10", "Expected test1 value to be '10'")
  }
}
Test.assert(found, "Expected to find test1 counter")

// Test CORS OPTIONS request
Test.new().route("/json").method("OPTIONS").status(204)
