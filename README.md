# 🚲 bialet

<p align="center">
  <img src="https://github.com/bialet/bialet/assets/142173/af827692-0e0d-4805-a478-77d07bd62e18" alt="Make Web Development Great Again hat" width="200" />
</p>
<p align="center">
  <strong>Make Web Development Great Again</strong>
</p>


Bialet is the [worst](https://en.wikipedia.org/wiki/Worse_is_better) web
development framework ever.

Would you like a strongly typed programming language with strong unit testing support?
Well, this is not the framework you are looking for, here we want to **ship features**.

```wren
import "bialet" for Response

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
```

See [more examples](examples/run.md) for usage.

## Installation

The best way to use it is with Docker. In the root of the project run:

```bash
docker compose up
```

Use the environment variables **BIALET_DIR** and **BIALET_PORT** to change the
default values.

## Building

Install SQLite3, OpenSSL 3 and curl for dev dependencies and run the build.

In Debian based

```bash
sudo apt install -y libsqlite3-dev libssl-dev libcurl4-openssl-dev
```

In MacOS

```bash
brew install sqlite3 openssl curl
```

Then run the build

```bash
make clean && make
```

## Running with binary

```bash
bialet -p 7000 /path/to/root
```

Available Configuration

| Description | Argument | Default value |
| --- | --- | --- |
| Port | -p | 7000 |
| Host | -h | localhost |
| Log file | -l | stdout |
| Memory limit | -m | 50 (in Megabytes) |
| Hard memory limit | -M | 100 (in Megabytes) |
| CPU limit | -c | 15 |
| Hard CPU limit | -C | 30 |
