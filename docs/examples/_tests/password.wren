Test.post("/password", {"password":"123", "password-check":"123"}).status(200).contains("The passwords are the same")
Test.post("/password", {"password":"123", "password-check":"321"}).status(200).contains("The passwords are different")
