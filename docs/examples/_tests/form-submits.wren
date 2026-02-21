// Test form submission and cumulative counter endpoint

// GET /form-submits shows the form with initial total
Test.get("/form-submits")
    .status(200)
    .contains("Total sum")
    .contains("<form")

// POST /form-submits increments the running total by the submitted value
Test.post("/form-submits", {"value": "5"})
    .status(200)
    .contains("Total sum")

// POST again with another value
Test.post("/form-submits", {"value": "10"})
    .status(200)
    .contains("Total sum")

// Negative values are accepted (subtraction)
Test.post("/form-submits", {"value": "-3"})
    .status(200)
    .contains("Total sum")
