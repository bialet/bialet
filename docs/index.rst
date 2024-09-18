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
      _title = "🚲 Welcome to Bialet"
    }
    user(id) { `SELECT * FROM users WHERE id = ?`.first(id) }
    name(id) { user(id)["name"] || "World" }
    html(content) {
      return <!doctype html>
      <html>
        <head>
          <title>{{ _title }}</title>
        </head>
        <body>
          <h1>{{ _title }}</h1>
          {{ content }}
        </body>
      </html>
    }
  }

  var idUrlParam = Request.get("id")
  var app = App.new()
  var html = app.html(
    <p>👋 Hello <b>{{ app.name(idUrlParam) }}</b></p>
  )
  Response.out(html) // Serve the HTML


Bialet is a full-stack web framework that integrates the object-oriented `Wren language <https://wren.io/>`_ with a HTTP server and a built-in SQLite database **in a single app**, creating a unified environment for web development.

----------
Quickstart
----------

Clone or download the `Bialet Skeleton <https://github.com/bialet/skeleton>`_ repository and use `Docker Compose <https://docs.docker.com/compose/>`_ to start the app.

.. code-block:: shell

  git clone --depth 1 https://github.com/bialet/skeleton.git mywebapp
  cd mywebapp
  docker compose up

Visit `localhost:7000 <http://localhost:7000>`_ in your browser.

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
   html-strings
   database
   datetime
   reference
   examples
