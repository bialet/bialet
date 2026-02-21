// Test date and time functions endpoint

// GET /date shows the date page with default date
Test.get("/date")
    .status(200)
    .contains("Date and Time functions")
    .contains("Year:")
    .contains("Month:")
    .contains("Day:")
    .contains("Timezone:")

// POST /date with a known date updates the display
Test.post("/date", {"date": "2024-01-15", "time": "10:30:00", "tz": "UTC"})
    .status(200)
    .contains("2024")
    .contains("01")
    .contains("15")
