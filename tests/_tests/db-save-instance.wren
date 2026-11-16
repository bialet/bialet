// save() inspects fields, so a class instance maps to columns in the order its
// fields were first assigned.
`DROP TABLE IF EXISTS t_save_obj`.query
`CREATE TABLE t_save_obj (id INTEGER PRIMARY KEY, name TEXT, tags TEXT)`.query

class SaveThing {
  construct new(data) {
    _id = data["id"]
    _name = data["name"]
    _tags = data["tags"]
  }
  id { _id }
  id=(v) { _id = v }
  name { _name }
  name=(v) { _name = v }
  tags { _tags }
  tags=(v) { _tags = v }
}

var thing = SaveThing.new({"name": "first", "tags": "x"})
var id = `t_save_obj`.save(thing)
Test.assert(id != null, "save(instance) returns an id")
var row = `SELECT * FROM t_save_obj WHERE id = ?`.first(id)
Test.assert(row["name"] == "first", "save(instance) inserted the name")
Test.assert(row["tags"] == "x", "save(instance) inserted the tags")

thing.id = id
thing.name = "second"
thing.tags = "y"
`t_save_obj`.save(thing)
Test.assert(`SELECT COUNT(*) FROM t_save_obj`.toNum == 1, "save(instance) updated, did not duplicate")
row = `SELECT * FROM t_save_obj WHERE id = ?`.first(id)
Test.assert(row["name"] == "second", "save(instance) updated the name")
Test.assert(row["tags"] == "y", "save(instance) updated the tags")

var mapped = `SELECT * FROM t_save_obj WHERE id = ?`.first(id).to(SaveThing)
Test.assert(mapped is SaveThing, "row.to(Class) maps an instance")
Test.assert(mapped.name == "second", "mapped instance reads fields")
`DROP TABLE t_save_obj`.query
