
`DROP TABLE IF EXISTS qparam_test`.query
`CREATE TABLE qparam_test (id INTEGER PRIMARY KEY, name TEXT, value TEXT)`.query

// Non-primitive params (HtmlNode, Date, ...) must be stringified via toString
// before binding. Dropping them would shift every later `?` placeholder left
// and silently corrupt the row.
var node = HtmlNode.new("node-name")
`INSERT INTO qparam_test (name, value) VALUES (?, ?)`.query([node, "tail"])
var row = `SELECT name, value FROM qparam_test WHERE id = ?`.first([1])
var fetched = `SELECT name FROM qparam_test WHERE name = ?`.fetch([node])
return "%(row['name'])|%(row['value'])|%(fetched[0]['name'])"
