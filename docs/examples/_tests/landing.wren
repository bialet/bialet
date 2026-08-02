// Test CSS Grid landing page showcase
// Page renders a full landing with features, pricing, testimonials, and FAQ

// GET /css returns the complete landing page
Test.get("/css")
    .status(200)
    .contains("Nexus")
    .contains("Everything you need to ship")
    .contains("Instant Hot Reload")
    .contains("Component Architecture")
    .contains("Zero-Config Database")
    .contains("CSS Grid First")
    .contains("Security Built-In")
    .contains("Single Binary Deploy")

// Stats section
Test.get("/css")
    .status(200)
    .contains("12K")
    .contains("99.9")
    .contains("Active Developers")
    .contains("Avg Response")

// Pricing section
Test.get("/css")
    .status(200)
    .contains("Starter")
    .contains("Pro")
    .contains("Enterprise")
    .contains("Simple, transparent pricing")
    .contains("Most Popular")

// Testimonials section
Test.get("/css")
    .status(200)
    .contains("Sarah Chen")
    .contains("Marcus Rivera")
    .contains("Aiko Tanaka")
    .contains("James Okonkwo")

// FAQ section
Test.get("/css")
    .status(200)
    .contains("Frequently asked questions")
    .contains("Do I need to install a database separately")
    .contains("How does deployment work")

// Conditional: promo banner is shown (showBanner = true)
Test.get("/css")
    .status(200)
    .contains("Limited-time offer")

// Conditional: monthly pricing is active by default (isAnnual = false)
Test.get("/css")
    .status(200)
    .contains("$29")

// Page is valid HTML
Test.get("/css")
    .status(200)
    .contains("<!doctype html>")
    .contains("</html>")
