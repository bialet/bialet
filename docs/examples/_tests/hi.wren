Test.get("/hi").status(200).contains("Hello World")

Test.get("/hi")
    .status(200)
    .equals("<h1>👋 Hello World</h1>")

Test.get("/hi")
    .status(200)
    .contains("Hello")
    .contains("World")
    .notContains("Goodbye")
