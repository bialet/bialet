// Test product sorting endpoint with safe .order() method

// GET /sorting shows the product list with default sort (name asc)
Test.get("/sorting")
    .status(200)
    .contains("Product List")
    .contains("Laptop")
    .contains("Mouse")

// Sort by price ascending - cheapest first
Test.get("/sorting?sort=price&order=asc")
    .status(200)
    .contains("Mouse")
    .contains("29.99")

// Sort by name descending
Test.get("/sorting?sort=name&order=desc")
    .status(200)
    .contains("Webcam")

// Filter by category
Test.get("/sorting?category=Furniture")
    .status(200)
    .contains("Desk Chair")
    .notContains("Laptop")

// Invalid sort column falls back to default (name)
Test.get("/sorting?sort=evil_injection&order=asc")
    .status(200)
    .contains("Product List")
