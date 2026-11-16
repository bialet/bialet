// Out-of-range status values must not reach the status line: a non-finite or
// non-integral double is a runtime error, answered with the 500 page.
Response.status(1e300)
return "should-not-be-served"
