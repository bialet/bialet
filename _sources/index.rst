Bialet
======

.. meta::
    :description: Bialet is a full-stack web framework made to enhance HTML with a native integration to a persistent database

.. raw:: html

   <div style="text-align: center; margin: 2em 0 1em 0">
       <img src="_static/logo.png" alt="Bialet" style="width: 140px; margin-bottom: 0.5em" />
       <h1 style="font-size: 2.5em; font-weight: 700; margin: 0 0 0.25em 0">
           Craft web apps with plain HTML and real simplicity
       </h1>
       <p style="font-size: 1.15em; color: var(--sd-color-secondary); max-width: 32em; margin: 0 auto 1.25em auto">
           Bialet is a full-stack web framework that brings back the joy of web
           development: write HTML, use a built-in SQLite database, and ship fast —
           <strong>no complex toolchains, no drama.</strong>
       </p>
   </div>

.. raw:: html

   <div style="text-align: center; margin-bottom: 2em">
     <a class="sd-sphinx-override sd-btn sd-text-wrap sd-btn-primary sd-shadow-sm" href="getting-started/index.html" style="margin: 0 0.35em">Get Started</a>
     <a class="sd-sphinx-override sd-btn sd-text-wrap sd-btn-outline-secondary sd-shadow-sm" href="https://github.com/bialet/bialet" style="margin: 0 0.35em">
       <svg version="1.1" width="1em" height="1em" class="sd-octicon sd-octicon-mark-github" viewBox="0 0 16 16" aria-hidden="true"><path fill-rule="evenodd" d="M8 0C3.58 0 0 3.58 0 8c0 3.54 2.29 6.53 5.47 7.59.4.07.55-.17.55-.38 0-.19-.01-.82-.01-1.49-2.01.37-2.53-.49-2.69-.94-.09-.23-.48-.94-.82-1.13-.28-.15-.68-.52-.01-.53.63-.01 1.08.58 1.23.82.72 1.21 1.87.87 2.33.66.07-.52.28-.87.51-1.07-1.78-.2-3.64-.89-3.64-3.95 0-.87.31-1.59.82-2.15-.08-.2-.36-1.02.08-2.12 0 0 .67-.21 2.2.82.64-.18 1.32-.27 2-.27.68 0 1.36.09 2 .27 1.53-1.04 2.2-.82 2.2-.82.44 1.1.16 1.92.08 2.12.51.56.82 1.27.82 2.15 0 3.07-1.87 3.75-3.65 3.95.29.25.54.73.54 1.48 0 1.07-.01 1.93-.01 2.2 0 .21.15.46.55.38A8.013 8.013 0 0016 8c0-4.42-3.58-8-8-8z"></path></svg>
       View on GitHub
     </a>
   </div>

----

.. code-block:: wren
   :linenos:

   var users = `SELECT id, name FROM users`.fetch
   var title = "🗂️ Users list"

   return <!doctype html>
     <html>
       <head><title>{{ title }}</title></head>
       <body style="font: 1.5em/2.5 system-ui; text-align:center">
         <h1>{{ title }}</h1>
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

Bialet integrates the object-oriented `Wren language <https://wren.io/>`_ with
a HTTP server and a built-in SQLite database — **in a single binary**.
No configuration files, no dependencies, no build step. Just write ``.wren``
files and run.

----

Features
========

.. grid:: 1 2 2 4
   :gutter: 2

   .. grid-item-card::
      :text-align: center
      :shadow: sm

      :octicon:`zap;2em;sd-text-info`
      ^^^
      **Zero Configuration**

      No YAML, no JSON, no setup files. Run ``bialet`` in any directory.

   .. grid-item-card::
      :text-align: center
      :shadow: sm

      :octicon:`database;2em;sd-text-info`
      ^^^
      **SQLite Built-in**

      Full database engine included. Migrations, queries, and CRUD — ready to go.

   .. grid-item-card::
      :text-align: center
      :shadow: sm

      :octicon:`package;2em;sd-text-info`
      ^^^
      **Single Binary**

      \~2 MB. No runtime, no package manager, no external services needed.

   .. grid-item-card::
      :text-align: center
      :shadow: sm

      :octicon:`rocket;2em;sd-text-info`
      ^^^
      **No Build Step**

      Edit a ``.wren`` file, refresh the browser. That's the entire workflow.

----

Who is Bialet for?
==================

.. grid:: 1 2 3 3
   :gutter: 2

   .. grid-item-card::
      :text-align: center
      :shadow: sm

      :octicon:`beaker;2em;sd-text-warning`
      ^^^
      **Prototyping & Internal Tools**

      Spin up a data-driven dashboard or admin panel in minutes. Bialet's
      all-in-one design means zero infrastructure overhead.

   .. grid-item-card::
      :text-align: center
      :shadow: sm

      :octicon:`mortar-board;2em;sd-text-warning`
      ^^^
      **Learning Web Development**

      Focus on HTML, SQL, and basic logic — not webpack configs, package
      managers, or ORM abstractions.

   .. grid-item-card::
      :text-align: center
      :shadow: sm

      :octicon:`heart;2em;sd-text-warning`
      ^^^
      **Developers Who Value Simplicity**

      If you prefer a single tool that does one thing well over a dozen
      micro-libraries, Bialet is for you.

----

Getting Started
===============

Install Bialet with a single command (macOS ARM, Ubuntu x86_64, Ubuntu ARM):

.. code-block:: shell

   curl -sSL https://get.bialet.dev | sh

Then create an ``index.wren`` file and run it:

.. code-block:: shell

   bialet

Visit `127.0.0.1:7001 <http://127.0.0.1:7001>`_ in your browser.

.. raw:: html

   <div style="text-align: center; margin: 1.5em 0">
     <a class="sd-sphinx-override sd-btn sd-text-wrap sd-btn-primary sd-shadow-sm" href="getting-started/index.html" style="margin: 0 0.35em">📖 Full Tutorial</a>
     <a class="sd-sphinx-override sd-btn sd-text-wrap sd-btn-outline-secondary" href="installation.html" style="margin: 0 0.35em">All Install Options</a>
   </div>

.. toctree::
   :hidden:
   :maxdepth: 2

   self
   getting-started/index
   installation
   structure
   html-strings
   database
   rest-api
   tests
   file
   datetime
   cron
   mcp
   reference
   examples
   faq
   why-bialet

----

.. raw:: html

   <div class="community-banner">
     <p style="font-size: 1.3em; margin: 0 0 0.5em 0">🚲 <strong>Let's build together</strong></p>
     <p style="padding: 0.5em">
       Bialet is maintained by a solo developer, but the community is open to everyone.
       Whether you're stuck on a query, have a feature idea, or want to show off your
       project, GitHub Discussions is the place. Search first, ask second, and be kind.
     </p>
     <a class="sd-sphinx-override sd-btn sd-text-wrap sd-btn-outline-secondary sd-shadow-sm" href="https://github.com/bialet/bialet/discussions">Join the discussion →</a>
   </div>
