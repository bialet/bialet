Bialet
======

**Bialet is a full-stack web framework that integrates the object-oriented Wren language with a single HTTP server and a built-in database, creating a unified environment for web development**

.. figure:: _static/make-web-dev-great-again.png

  Make Web Development Great Again

Quickstart
----------

1. Install Bialet using Docker Compose.

.. code-block:: bash

  git clone https://github.com/bialet/bialet.git
  cd bialet
  docker compose up

2. Visit `localhost:7000 <http://localhost:7000>`_ in your browser.

See :doc:`installation </installation>` for details on building and running the project.

Hello World
-----------

The code is written in `Wren <https://wren.io>`_, though a custom heavily modified
version.

.. code-block:: wren

   import "bialet" for Request, Response

   var name = Request.get("name")

   Response.out('<p>Hello %( name || "World" )!</p>')

See more
--------

.. toctree::
   :maxdepth: 2

   self
   tutorial
   installation
   structure
