`CREATE TABLE IF NOT EXISTS class_test (id INTEGER PRIMARY KEY, name TEXT, value INTEGER)`.query
`DELETE FROM class_test`.query
`INSERT INTO class_test (id, name, value) VALUES (1, 'alpha', 10)`.query
`INSERT INTO class_test (id, name, value) VALUES (2, 'beta', 20)`.query

class Item {
  construct new(row) {
    _name = row["name"]
    _value = row["value"]
  }
  name { _name }
  value { _value }
}

var items = `SELECT * FROM class_test ORDER BY id`.fetch.to(Item).toList
return items[0].name + ":" + items[0].value.toString + "," + items[1].name + ":" + items[1].value.toString
