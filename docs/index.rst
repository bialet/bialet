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

.. container:: sd-d-flex-row sd-align-minor-center sd-gap-2 sd-mb-4

   .. button-link:: getting-started.html
      :color: primary
      :shadow:
      :class: sd-px-4 sd-py-2

      Get Started

   .. button-link:: https://github.com/bialet/bialet
      :color: secondary
      :shadow:
      :outline:
      :class: sd-px-4 sd-py-2

      :octicon:`mark-github` View on GitHub

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

🚲 The Bialet Manifesto
========================

.. grid:: 1 2 3 3
   :gutter: 2

   .. grid-item-card::
      :shadow: sm

      **1. Web development should be fun again**
      ^^^
      No build steps, no SDKs, no hundreds of dependencies. If you can write
      HTML, you're already building.

   .. grid-item-card::
      :shadow: sm

      **2. HTML and CSS are the real frontend**
      ^^^
      JS is welcome, but never required. Use HTML and CSS directly — they're
      all you need to build something beautiful.

   .. grid-item-card::
      :shadow: sm

      **3. A database and zero configuration are the default**
      ^^^
      One binary. No YAMLs, no JSONs, no mysteries. SQLite is already there,
      and it works out of the box.

   .. grid-item-card::
      :shadow: sm

      **4. Deploy it means copying the files**
      ^^^
      Your app is ready as soon as your files are uploaded. No need for
      complex CI tools.

   .. grid-item-card::
      :shadow: sm

      **5. Do you really need a spaceship?**
      ^^^
      Bialet is a bicycle — simple, fast, and under your control. Build real
      apps without the weight.

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
      ^^^
      No YAML, no JSON, no setup files. Run ``bialet`` in any directory.

   .. grid-item-card::
      :text-align: center
      :shadow: sm

      :octicon:`database;2em;sd-text-info`
      ^^^
      **SQLite Built-in**
      ^^^
      Full database engine included. Migrations, queries, and CRUD — ready to go.

   .. grid-item-card::
      :text-align: center
      :shadow: sm

      :octicon:`package;2em;sd-text-info`
      ^^^
      **Single Binary**
      ^^^
      \~2 MB. No runtime, no package manager, no external services needed.

   .. grid-item-card::
      :text-align: center
      :shadow: sm

      :octicon:`rocket;2em;sd-text-info`
      ^^^
      **No Build Step**
      ^^^
      Edit a ``.wren`` file, refresh the browser. That's the entire workflow.

----

Who is Bialet for?
==================

.. grid:: 1 2 3 3
   :gutter: 2

   .. grid-item-card::
      :shadow: sm

      :octicon:`beaker;2em;sd-text-warning`
      ^^^
      **Prototyping & Internal Tools**
      ^^^
      Spin up a data-driven dashboard or admin panel in minutes. Bialet's
      all-in-one design means zero infrastructure overhead.

   .. grid-item-card::
      :shadow: sm

      :octicon:`mortar-board;2em;sd-text-warning`
      ^^^
      **Learning Web Development**
      ^^^
      Focus on HTML, SQL, and basic logic — not webpack configs, package
      managers, or ORM abstractions.

   .. grid-item-card::
      :shadow: sm

      :octicon:`heart;2em;sd-text-warning`
      ^^^
      **Developers Who Value Simplicity**
      ^^^
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

.. container:: sd-d-flex-row sd-gap-2 sd-mt-3

   .. button-link:: getting-started.html
      :color: primary
      :shadow:

      📖 Full Tutorial

   .. button-link:: installation.html
      :color: secondary
      :outline:

      All Install Options

----

.. grid:: 3

    .. grid-item-card::
        :text-align: center
        :link: https://github.com/bialet/bialet/

        :octicon:`mark-github;2em;sd-text-info`

        View repository
        +++++++++++++++++++++++
        *In GitHub*

    .. grid-item-card::
        :text-align: center
        :link: https://github.com/bialet/bialet/archive/refs/tags/v0.9.zip

        :octicon:`file-zip;2em;sd-text-info`

        Download source
        +++++++++++++++
        *use it with Docker or compile it*

    .. grid-item-card::
        :text-align: center
        :link: https://raw.githubusercontent.com/bialet/bialet/main/prompt.md

        :octicon:`download;2em;sd-text-info`

        AI Development Guide
        ++++++++++++++++++++
        *prompt.md for AI assistants*

.. toctree::
   :hidden:
   :maxdepth: 2

   self
   getting-started
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
