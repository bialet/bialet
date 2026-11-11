// The template wraps the sentence across lines; text()/textContains collapse
// the whitespace, so a multi-word assertion does not depend on the layout.
Test.get("/text-wrap").status(200).textContains("One two three")
Test.assert(Test.get("/text-wrap").text() == "One two three", "text() collapses whitespace")
