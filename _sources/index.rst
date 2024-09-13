Bialet
======

.. meta::
    :description: Bialet is a full-stack web framework made to enhance HTML with a native integration to a persistent database

.. raw:: html

   <p style="text-align: center; font-size: 1.5em; margin: 1em 0">Enhance HTML with a native integration to a persistent database</p>

.. code-block::

  import "bialet" for Request, Response

  class App {
    construct new() {
      _title = "Welcome to Bialet"
      _default = "World"
    }
    // Get the user ID from the URL
    idUrlParam() { Request.get("id") }
    // Fetch the user from the database using plain SQL
    getUser(id) { `SELECT * FROM users WHERE id = ?`.first(id) }
    // Get the name from the current user if it exists or the default
    name() {
      var user = getUser(idUrlParam())
      return user ? user["name"] : _default
    }
    // Build the HTML
    html() { '
      <html>
        <head>
          <title>%( _title )</title>
        </head>
        <body>
          <h1>%( _title )</h1>
          <p>Hello, <em>%( name() )</em>!</p>
        </body>
      </html>
    ' }
  }

  var app = App.new()
  Response.out(app.html()) // Serve the HTML


Bialet is a full-stack web framework that integrates the object-oriented `Wren language <https://wren.io/>`_ with a HTTP server and a built-in SQLite database **in a single app**, creating a unified environment for web development.

.. grid:: 2

    .. grid-item-card::
        :text-align: center
        :link: https://github.com/bialet/bialet/

        :octicon:`mark-github;2em;sd-text-info`

        View repository
        +++++++++++++++++++++++
        *In GitHub*

    .. grid-item-card::
        :text-align: center
        :link: https://github.com/bialet/bialet/archive/refs/tags/v0.5.zip

        :octicon:`file-zip;2em;sd-text-info`

        Download source
        +++++++++++++++
        *use it with Docker or compile it*

.. toctree::
   :hidden:
   :maxdepth: 2

   self
   getting-started
   installation
   structure
   database
   datetime
   reference
   examples
