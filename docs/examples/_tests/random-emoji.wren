// Test random emoji SVG generator endpoint

// GET /random-emoji generates an SVG file and returns it
Test.get("/random-emoji")
    .status(200)
    .contains("<svg")
    .contains("</svg>")
