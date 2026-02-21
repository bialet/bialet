// Test external HTTP call endpoint
// Note: This test requires internet access to dummyjson.com

// GET /http fetches users from external API and renders a table
Test.get("/http")
    .status(200)
    .contains("users from the JSONPlaceholder API")
