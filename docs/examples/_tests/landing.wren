// Test the CSS Grid landing page showcase
// Page renders a full landing with features, pricing, testimonials, and FAQ

// GET /landing returns the complete landing page
Test.get("/landing")
    .status(200)
    .contains("Bialet — Modern Framework")
    .contains("Everything you need to ship")
    .contains("Instant Hot Reload")
    .contains("Component Architecture")
    .contains("Zero-Config Database")
    .contains("Security Built-In")
    .contains("Single Binary Deploy")

// Stats section
Test.get("/landing")
    .status(200)
    .contains("20K+")
    .contains("Lines of Code")
    .contains("Tests Passed")

// Pricing section
Test.get("/landing")
    .status(200)
    .contains("Starter")
    .contains("Pro")
    .contains("Enterprise")
    .contains("Simple, transparent pricing")
    .contains("Most Popular")

// Testimonials section
Test.get("/landing")
    .status(200)
    .contains("Sarah Chen")
    .contains("Marcus Rivera")
    .contains("Aiko Tanaka")
    .contains("James Okonkwo")

// FAQ section
Test.get("/landing")
    .status(200)
    .contains("Frequently asked questions")
    .contains("Do I need to install a database separately")
    .contains("How does deployment work")

// Conditional: the promo banner is shown by default
Test.get("/landing")
    .status(200)
    .contains("Limited-time offer")

// Conditional: dismissing the banner hides it
Test.get("/landing?showBanner=no")
    .status(200)
    .notContains("Limited-time offer")

// Conditional: monthly pricing is the default, annual is opt-in
Test.get("/landing?isAnnual=yes")
    .status(200)
    .contains("save 17%")

// Page is valid HTML
Test.get("/landing")
    .status(200)
    .contains("<!doctype html>")
    .contains("</html>")
