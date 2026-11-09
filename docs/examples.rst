Examples
========

Simple query list
-----------------

.. include:: examples/simple.wren
  :code: wren

Hello World
-----------

.. include:: examples/hello.wren
  :code: wren

Count the visitors of your website
----------------------------------

.. include:: examples/visits.wren
   :code: wren

Count the clicks
----------------

.. include:: examples/clicks.wren
   :code: wren

Form submission
----------------

.. include:: examples/form-submits.wren
   :code: wren

JSON Response
-------------

.. include:: examples/json.wren
   :code: wren

Form Upload
------------

.. include:: examples/upload.wren
   :code: wren

File Creation
-------------

.. include:: examples/random-emoji.wren
   :code: wren

HTTP API Call
--------------

.. include:: examples/http.wren
   :code: wren

Password utilities
------------------

.. include:: examples/password.wren
   :code: wren

Cross site request forgery
--------------------------

.. include:: examples/csrf.wren
   :code: wren

Markdown
--------

.. include:: examples/markdown.wren
   :code: wren

Path-based route (``users.wren``)
----------------------------------

A single file serving both ``/users`` (list) and ``/users/:id`` (detail) via
``Request.route(0)``. See `Advanced Routing <advanced-routing.html>`_.

.. include:: examples/users.wren
   :code: wren


Guestbook
---------

The smallest complete app: a page, a form handler, and a migration. Run it
with ``bialet`` inside ``docs/examples/guestbook``.

.. include:: examples/guestbook/_migration.wren
   :code: wren

.. include:: examples/guestbook/index.wren
   :code: wren

.. include:: examples/guestbook/message.wren
   :code: wren

The styled version in ``docs/examples/guestbook-styled`` adds ``_app.wren``
(a ``Template`` layout class) and ``css/style.css``. The landing page video
walks through both.
