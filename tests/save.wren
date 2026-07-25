
`DROP TABLE IF EXISTS save_test`.query
`CREATE TABLE save_test (id INTEGER PRIMARY KEY, name text, value text, createdAt text)`.query
var now = Date.new(2024, 9, 13, 10, 30, 0).toString
var id = `save_test`.save({"name": "hello", "value": "world", "createdAt": now})
`save_test`.save({"id": id, "name": "hello", "value": "updated", "createdAt": now})
var result = `SELECT * FROM save_test WHERE id = ?`.first(id)
return "%(result['name']) %(result['value']) %(result['createdAt'])"
