// Test comes from the -T prelude, so this file cannot compile in a plain tree
// check: Test is "used but not defined". The walk must skip _tests.
Test.get("/").status(200).contains("ok")
