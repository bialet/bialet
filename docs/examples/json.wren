// Response and Db are built-in classes — no imports needed.

// Use the `query()` method to execute SQL statements.
// This creates a table named `counter` with two columns: `name` and `value`.
// In production this should be done with migrations; it's included here
// so the example is self-contained.
`CREATE TABLE IF NOT EXISTS counter (name TEXT PRIMARY KEY, value INTEGER)`.query()

// We use the `fetch()` method to get the query result as an array of objects.
var counters = `SELECT * FROM counter ORDER BY name ASC`.fetch()

// We use the `cors` method to enable Cross-Origin Resource Sharing (CORS).
// When the request method is OPTIONS, it will respond with the appropriate headers
// and a 204 No Content status, then return early.
if (Response.cors) return

// We use the `json()` method to send the query result as a JSON.
// This will add the header `Content-Type: application/json`.
Response.json(counters)
