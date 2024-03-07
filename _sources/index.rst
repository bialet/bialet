🚲 Bialet
=========

Bialet is the `worst <https://en.wikipedia.org/wiki/Worse_is_better>`_ web
development framework ever.

Would you like a strongly typed programming language with strong unit testing support?
Well, this is not the framework you are looking for, here we want to write bad
code and **ship features**.

Here is an example of how to use Bialet:

.. code-block::

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

The code is written in `Wren <https://wren.io>`_, though a custom heavily modified
version. See `more examples <https://github.com/bialet/bialet/tree/master/examples/run.md>`_ for usage.

.. toctree::
   :maxdepth: 2

   installation
   structure
   running
