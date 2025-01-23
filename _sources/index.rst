Bialet
======

.. meta::
    :description: Bialet is a full-stack web framework made to enhance HTML with a native integration to a persistent database

.. raw:: html

   <p style="text-align: center; font-size: 1.5em; margin: 1em 0">Enhance HTML with a native integration to a persistent database</p>

.. code-block::

  import "bialet" for Response

  var users = `SELECT id, name FROM users`.fetch
  var TITLE = "🗂️ Users list"

  Response.out(
    <!doctype html>
    <html>
      <head><title>{{ TITLE }}</title></head>
      <body style="font: 1.5em/2.5 system-ui; text-align:center">
        <h1>{{ TITLE }}</h1>
        {{ users.count > 0 ?
          <ul style="list-style-type:none">
            {{ users.map{|user| <li>
              <a href="/hello?id={{ user["id"] }}">
                👋 {{ user["name"] }}
              </a>
            </li> } }}
          </ul> :
          /* Users table is empty */
          <p>No users, go to <a href="/hello">hello</a>.</p>
        }}
      </body>
    </html>
  )

Bialet is a full-stack web framework that integrates the object-oriented `Wren language <https://wren.io/>`_ with a HTTP server and a built-in SQLite database **in a single app**, creating a unified environment for web development.

----------
Quickstart
----------

Clone or download the `Bialet Skeleton <https://github.com/bialet/skeleton>`_ repository and use `Docker Compose <https://docs.docker.com/compose/>`_ to start the app.

.. code-block:: shell

  git clone --depth 1 https://github.com/bialet/skeleton.git mywebapp
  cd mywebapp
  docker compose up

Visit `127.0.0.1:7000 <http://127.0.0.1:7000>`_ in your browser.

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
        :link: https://github.com/bialet/bialet/archive/refs/tags/v0.7.zip

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
   file
   datetime
   reference
   examples
