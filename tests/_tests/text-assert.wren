// Raw markup contains "device" in an attribute and a href; the visible text
// does not. textContains() asserts on the text, not the markup.
var page = Test.get("/text-assert").status(200)
Test.assert(page.text().contains("Visible text"), "text() keeps visible text")
Test.assert(!page.text().contains("device"), "text() drops attribute values")

Test.get("/text-assert").textContains("Visible text")
Test.get("/text-assert").textContains("Link")
