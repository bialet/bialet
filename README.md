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
Well, this is not the framework you are looking for, here we want to write bad
code and **ship features**.

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

The code is written in [Wren](https://wren.io), though a custom heavily modified
version. See [more examples](examples/run.md) for usage.

## Project Structure and Routing

**The best way to think a project with Bialet is do it first like static old school HTML,
then replace the logic and template duplication with Wren code.**

The scripts will be load as if they were HTML files, so the file `contact-us.wren` can be open with the URL [localhost:7000/contact-us.wren](http://localhost:7000/contact-us.wren) or even without the wren suffix [localhost:7000/contact-us](http://localhost:7000/contact-us).

This will work with each folder, for example the URL [localhost:7000/landing/newsletter/cool-campaign](http://localhost:7000/landing/newsletter/cool-campaign) will run the script `landing/newsletter/cool-campaign.wren`. Like if there was an HMTL file.

That mean that each file is public and will be executed **except** when it start with a `_` or `.`. The `_app.wren` file won't be load with the URL [localhost:7000/_app](http://localhost:7000/_app) or with any other URL. Any file or any file under a folder that start with `_` or `.` won't be open.

For dynamic routing, add a `_route.wren` file in the folder and use the `Request.route(N)` function to get the value. **N** is the position of the route parameter. For the file `api/_route.wren` when called from [localhost:7000/api/users/1?fields=name,email&sortType=ASC](http://localhost:7000/api/users/1?fields=name,email&sortType=ASC) those will be the values:

```wren
import "bialet" for Request

Request.route(0) // users
Request.route(1) // 1
Request.param("fields") // name,email
Request.param("sortType") // ASC
```

⚠️ There is no need to add a single `_route.wren` file to handle all the routing.

## Installation

The easiest way to use it is with [Docker Compose](https://docs.docker.com/compose/).
Clone the repository and run:

```bash
BIALET_DIR=/path/to/app docker compose up
```

The default port is 7000. Use the environment variable **BIALET_PORT** to change the port value.

## Building

Install the dependencies and run make. See the details in the [build](docs/source/build.md#Building) file.

## License

Bialet is released under the GPL 2 license, ensuring that it remains free and open-source, allowing users to modify and share the software under the same license terms.

## Credits

~I copy a lot of code from all over the web~
Bialet incorporates the work of several open-source projects and contributors. We extend our gratitude to:

- The [Wren programming language](https://wren.io), for its lightweight, flexible, and high-performance capabilities.
- The [Mongoose library](https://github.com/expressjs/mongoose), for providing an easy-to-use web server solution.
- Matthew Brandly, for his invaluable contributions to JSON parsing and utility functions in Wren. Check out his work at [Matthew Brandly's GitHub](https://github.com/brandly/wren-json).
- @PureFox48 for the upper and lower functions.
- [Codeium](https://github.com/codeium) for the [Codeium](https://codeium.com) plugin, ChatGPT and a lot of coffee.

We encourage users to explore these projects and recognize the efforts of their creators.
