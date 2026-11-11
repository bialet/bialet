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

A small authenticated app
-------------------------

The inventory app in ``docs/examples/inventory`` is a complete internal tool:
session login with CSRF and password hashing, a filtered and paginated list,
create/edit/delete, a file upload, and migrations.

.. code-block:: bash

   bialet docs/examples/inventory

Sign in with ``admin@example.com`` / ``changeme``.

- ``_migration.wren`` — the tables and the demo account
- ``_app.wren`` — the layout and the session lookup
- ``login.wren`` — CSRF check, ``Util.verify``, ``Session.rotate``
- ``items.wren`` — list, search, pagination, and the detail page in one file
- ``item.wren`` — create/edit with validation and the upload
- ``photo.wren`` — serves only photos attached to an item

It is under 300 lines of Wren plus one small stylesheet, which is the honest
size of an authenticated CRUD app in Bialet.
