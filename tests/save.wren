
`CREATE TABLE IF NOT EXISTS save_test (id INTEGER PRIMARY KEY, name text, value text)`.query
var id = `save_test`.save({"name": "hello", "value": "world"})
`save_test`.save({"id": id, "name": "hello", "value": "updated"})
var result = `SELECT * FROM save_test WHERE id = ?`.first(id)
return "%(result['name']) %(result['value'])"
