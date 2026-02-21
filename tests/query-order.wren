`CREATE TABLE IF NOT EXISTS test_items (id INTEGER PRIMARY KEY, name TEXT, score INTEGER)`.query
`DELETE FROM test_items`.query
`INSERT INTO test_items (name, score) VALUES ('item1', 10)`.query
`INSERT INTO test_items (name, score) VALUES ('item2', 20)`.query
`INSERT INTO test_items (name, score) VALUES ('item3', 15)`.query

var items = `SELECT * FROM test_items`.order("score", "desc", ["id", "name", "score"]).fetch
Response.out(items[0]["name"] + "," + items[1]["name"] + "," + items[2]["name"])
