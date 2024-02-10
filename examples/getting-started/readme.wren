import "bialet" for Response

// The only difference with the README code is the set up of the database
`CREATE TABLE IF NOT EXISTS config (key TEXT PRIMARY KEY, val TEXT)`.query()
`INSERT OR IGNORE INTO config VALUES ("title", "Hello World")`.query()
`INSERT OR IGNORE INTO config VALUES ("description", "This is the description")`.query()

// Look how short and sweet it is
class Config {
  static get(key){ `SELECT val FROM config WHERE key = ?`.first(key)["val"] }
}

// Or maybe you need other functions to interact with the values,
// so it makes sense to create the good ol' class that makes objects.
class ConfigValue {
  construct new(key) {
    _config = `SELECT * FROM config WHERE key = ?`.first(key)
  }
  key { _config["key"].upper }
  val { _config["val"] }
  toString { val }
}

var title = Config.get("title") // This is a string
var description = ConfigValue.new("description") // This is a ConfigValue
// ...they are both objects though 🤔

// Let's log the values in the server output
System.print("🔍 TITLE: %(title)")
System.print("🔍 %(description.key): %(description.val)")

// Remember the times were you write actual HTML?
Response.out('
<!DOCTYPE html>
  <body>
    <h1>%( title )</h1>
    <p>%( description )</p>
  </body>
</html>
')