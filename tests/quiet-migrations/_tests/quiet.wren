// One passing test, plus a log line so -q has test-side output to drop too.
System.log("quiet-test-log")
Test.get("/quiet").status(200).contains("quiet-ok")
