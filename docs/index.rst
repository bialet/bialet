Bialet
======

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
        :text-align: center
        :link: https://github.com/bialet/bialet/releases/download/v0.4/bialet-desktop_0.1.0_amd64.deb

        :octicon:`desktop-download;2em;sd-text-info`

        Download Bialet Desktop
        +++++++++++++++++++++++
        *for Ubuntu/Debian*

    .. grid-item-card::
        :text-align: center
        :link: https://github.com/bialet/bialet/archive/refs/tags/v0.4.zip

        :octicon:`file-zip;2em;sd-text-info`

        Download source
        +++++++++++++++
        *use it with Docker or compile it*



.. button-link:: https://github.com/bialet/bialet
   :align: center

    :octicon:`mark-github;2em;sd-text-info`
    View repository in GitHub

.. toctree::
   :hidden:
   :maxdepth: 2

   self
   quickstart
   installation
   structure
