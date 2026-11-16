// Test the external HTTP call example.
//
// The page fetches dummyjson.com, so the assertion needs a network. Probe the
// API first and mark the file skipped rather than failed when the machine is
// offline: a docs build must still pass on a sandbox with no egress.

var fiber = Fiber.new {
  return Http.get("https://dummyjson.com/users?limit=1")
}
var probe = fiber.try()

if (probe == false || probe == null) {
  Tests.skip()
} else {
  // The table is rendered from the JSON the API returned
  Test.get("/http")
      .status(200)
      .contains("users from the DummyJSON API")

  // Every fetched user became a table row, so the page carries real data, not
  // just the heading.
  Test.get("/http")
      .status(200)
      .contains("<td>")
}
