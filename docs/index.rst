Bialet
======

.. meta::
    :description: Bialet is a full-stack web framework made to enhance HTML with a native integration to a persistent database

.. raw:: html

   <p style="text-align: center; font-size: 1.5em; margin: 1em 0">Enhance HTML with a native integration to a persistent database</p>

.. code-block::

   import "bialet" for Response
   // String for the template
   var title = "Welcome to Bialet"
   // Query object, not a string
   var user = `SELECT * FROM users WHERE id = 1`.first()
   // Output the template
   Response.out('
     <html>
       <head>
         <title>%( title )</title>
       </head>
       <body>
         <h1>%( title )</h1>
         <p>Hello <em>%( user["name"] )</em></p>
       </body>
     </html>
   ')

Bialet is a full-stack web framework that integrates the object-oriented `Wren language <https://wren.io/>`_ with a HTTP server and a built-in SQLite database **in a single app**, creating a unified environment for web development.

.. grid:: 2

    .. grid-item-card::
    .. button-link:: https://github.com/bialet/bialet
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
   reference
   examples
