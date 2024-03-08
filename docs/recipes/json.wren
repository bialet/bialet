// This line imports the Response class for managing HTTP interactions.
// The import lines are write at the top of the script.
import "bialet" for Response

// We use the `query()` method to execute SQL statements.
// In this case we create a table named `counter` with two columns: `name` and `value`.
// This should be do it with migrations, here it is added to the script to be self-contained.
`CREATE TABLE IF NOT EXISTS counter (name TEXT PRIMARY KEY, value INTEGER)`.query()

// We use the `fetch()` method to get the query result as an array of objects.
var counters = `SELECT * FROM counter ORDER BY name ASC`.fetch()

// We use the `json()` method to send the query result as a JSON.
// This will add the header `Content-Type: application/json`.
Response.json(counters)
