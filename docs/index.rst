Web development became a spaceship. Bialet is a bicycle.
========================================================

.. meta::
    :description: A single-binary web framework. Built-in SQLite, a lightweight embedded scripting language (Wren), a built-in HTTP server, and zero dependencies. Ride light.
    :keywords: Bialet, web framework, Wren, SQLite, file-based routing, zero dependencies, server-rendered

.. llms-txt-ignore-start

.. raw:: html

   <p class="hero__lede">
     Build data-driven web apps from a single file. No NPM, no YAML,
     no separate database servers. Just a <strong>tiny binary</strong> with
     a built-in HTTP server, a scripting language, and SQLite.
   </p>

.. raw:: html

   <div id="bialet-download" class="install">

     <!-- Unix (Linux / macOS): curl command with copy button -->
     <div id="download-unix" class="install__platform">
       <span id="download-os-label-unix" class="install__label"></span>
       <div class="install__command">
         <code class="install__text">curl -fsSL https://get.bialet.dev | sh</code>
         <button type="button" class="install__copy" data-copy="curl -fsSL https://get.bialet.dev | sh" aria-label="Copy install command">
           <svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" stroke-width="1.5" stroke="currentColor" fill="none" stroke-linecap="round" stroke-linejoin="round" aria-hidden="true"><rect x="8" y="8" width="12" height="12" rx="2"/><path d="M16 8v-2a2 2 0 0 0 -2 -2h-8a2 2 0 0 0 -2 2v8a2 2 0 0 0 2 2h2"/></svg>
           <span class="install__copy-text">Copy</span>
         </button>
       </div>
     </div>

     <!-- Windows: download button -->
     <div id="download-win" class="install__platform">
       <span class="install__label">Install for Windows</span>
       <a class="sd-sphinx-override sd-btn sd-text-wrap sd-btn-outline-secondary" href="{{release_windows_url}}">
         Download for Windows (.zip)
       </a>
       <p class="install__note">Zero installation required. Just extract and run.</p>
     </div>

     <!-- Fallback: all platforms (also what visitors see without JS) -->
     <div id="download-other" class="install__platform is-visible">
       <a class="sd-sphinx-override sd-btn sd-text-wrap sd-btn-outline-secondary" href="https://github.com/bialet/bialet/releases/latest">
         Download the binaries (all platforms)
       </a>
       <p class="install__note">Zero installation required. Just extract and run.</p>
     </div>

     <p class="install__more">
       <a href="installation.html">All platforms &amp; install options</a>
     </p>

   </div>

   <div class="hero-actions">
     <a class="sd-sphinx-override sd-btn sd-text-wrap sd-btn-primary" href="getting-started/index.html">Get Started</a>
     <a class="hero-actions__link" href="https://github.com/bialet/bialet">
       <svg version="1.1" width="1em" height="1em" class="sd-octicon sd-octicon-mark-github" viewBox="0 0 16 16" aria-hidden="true"><path fill-rule="evenodd" d="M8 0C3.58 0 0 3.58 0 8c0 3.54 2.29 6.53 5.47 7.59.4.07.55-.17.55-.38 0-.19-.01-.82-.01-1.49-2.01.37-2.53-.49-2.69-.94-.09-.23-.48-.94-.82-1.13-.28-.15-.68-.52-.01-.53.63-.01 1.08.58 1.23.82.72 1.21 1.87.87 2.33.66.07-.52.28-.87.51-1.07-1.78-.2-3.64-.89-3.64-3.95 0-.87.31-1.59.82-2.15-.08-.2-.36-1.02.08-2.12 0 0 .67-.21 2.2.82.64-.18 1.32-.27 2-.27.68 0 1.36.09 2 .27 1.53-1.04 2.2-.82 2.2-.82.44 1.1.16 1.92.08 2.12.51.56.82 1.27.82 2.15 0 3.07-1.87 3.75-3.65 3.95.29.25.54.73.54 1.48 0 1.07-.01 1.93-.01 2.2 0 .21.15.46.55.38A8.013 8.013 0 0016 8c0-4.42-3.58-8-8-8z"></path></svg>
       View on GitHub
     </a>
   </div>

   <!-- One proof: the tutorial app, and the code that renders it. -->
   <div class="proof">
     <figure class="proof__shot">
       <img src="_static/poll-vote.png" alt="A page titled Poll asking &quot;Has web development become overly complex?&quot; with Yes and No options and a Vote button." />
       <figcaption>The tutorial's poll, and the handler behind its Vote button.</figcaption>
     </figure>
     <details class="proof__code" open data-collapse-on-phone>
       <summary>The vote handler, verbatim</summary>

.. literalinclude:: getting-started/3-final/index.wren
   :language: wren
   :lines: 1-12

.. raw:: html

     </details>
   </div>

----

.. raw:: html

   <div class="community-banner">
     <p class="community-banner__title"><strong>Building with AI?</strong></p>
     <p>
       Paste this into Claude, ChatGPT, or Cursor so it writes idiomatic
       <code>.wren</code> code from the first response.
     </p>
     <button type="button" class="sd-sphinx-override sd-btn sd-text-wrap sd-btn-outline-secondary" data-copy-prompt>
       <span data-copy-label>Copy AI Prompt</span>
     </button>
     <p class="community-banner__action">
       <a href="ai-prompt.html">View the full prompt →</a>
     </p>
   </div>

   <div class="copy-status" role="status" aria-live="polite"></div>

.. llms-txt-ignore-end

----

Features
--------

.. grid:: 1 2 2 2
   :gutter: 2

   .. grid-item-card::

      :octicon:`package;2em`
      ^^^
      **A Tiny Single Binary**

      Drop the gigabytes of ``node_modules``. Bialet is a small,
      self-contained executable. Copy it to your server, and your app
      is deployed.

   .. grid-item-card::

      :octicon:`database;2em`
      ^^^
      **SQLite is Built-in**

      Forget about provisioning databases or writing connection strings.
      Real data persistence works completely out of the box.

   .. grid-item-card::

      :octicon:`zap;2em`
      ^^^
      **Zero Build Step**

      Write a ``.wren`` file, save it, and refresh your browser.
      No compilers, no bundlers, no watch processes.

   .. grid-item-card::

      :octicon:`code;2em`
      ^^^
      **HTML is the Real Frontend**

      Use standard HTML and CSS. Bialet lets you build server-rendered
      apps the classic, straightforward way. Add JS only if you want to.

----

A script that stays out of your way:
------------------------------------

.. raw:: html

   <details class="landing-sample" open data-collapse-on-phone>
     <summary>The whole file</summary>
     <div class="landing-sample__body">

.. code-block:: wren

   // 1. Create your database table automatically
   `CREATE TABLE IF NOT EXISTS messages (text TEXT)`.query

   // 2. Handle POST requests and save data
   if (Request.isPost) {
     var msg = Request.post("msg") || ""
     `messages`.save({"text": msg})
   }

   // 3. Fetch data using pure SQL
   var messages = `SELECT * FROM messages`.fetch

   // 4. Return HTML directly
   return <main>
     <h1>Bialet Guestbook</h1>

     <form method="post">
       <input name="msg" placeholder="Write a message...">
       <button>Submit</button>
     </form>
     <hr>
     <ul>
       {{ messages.map {|m| <li>{{ m["text"] }}</li> } }}
     </ul>
   </main>

.. raw:: html

     </div>
   </details>

Bialet integrates the :doc:`Wren language <wren>` (based on
`wren.io <https://wren.io/>`_) with
a HTTP server and a built-in SQLite database — **in a single binary**.
No configuration files, no dependencies, no build step. Just write ``.wren``
files and run.

Run ``bialet`` in your project folder and open `127.0.0.1:7001 <http://127.0.0.1:7001>`_.

----

The Bialet Manifesto — Ride Light 🚲
------------------------------------

.. admonition:: Ride Light
   :class: tip

   1. **Simplicity is a superpower.** Every line of tooling you don't write
      is a line of your app that ships faster.

   2. **Standards, not frameworks.** HTML, SQL, and HTTP have outlived every
      framework. Master them and your knowledge stays relevant.

   3. **One file to deploy.** No containers, no orchestration, no
      ``docker-compose.yml``. Copy the binary — that's it.

   4. **Batteries included.** Server, database, templating — all in one
      small binary. No external services to provision.

   5. **Ride light.** Complexity is a choice. Choose less, and you'll go
      further than you think.

----

Who is Bialet for?
------------------

.. grid:: 1 2 3 3
   :gutter: 2

   .. grid-item-card::

      :octicon:`beaker;2em`
      ^^^
      **Internal Tools & Dashboards**

      Spin up a data-driven admin panel in minutes with zero
      infrastructure overhead.

   .. grid-item-card::

      :octicon:`mortar-board;2em`
      ^^^
      **Learning & Teaching**

      Focus on HTML, SQL, and core logic. Skip the webpack configs
      and ORM abstractions.

   .. grid-item-card::

      :octicon:`heart;2em`
      ^^^
      **Lovers of Simplicity**

      If you prefer one tool that does its job perfectly over a chaotic
      stack of micro-libraries, hop on.

----

.. llms-txt-ignore-start

.. raw:: html

   <div class="hero-actions">
     <a class="sd-sphinx-override sd-btn sd-text-wrap sd-btn-outline-secondary" href="getting-started/index.html">Read the docs</a>
     <a class="hero-actions__link" href="installation.html">All install options</a>
   </div>

.. llms-txt-ignore-end

.. toctree::
   :hidden:
   :maxdepth: 2
   :caption: Before You Start

   self
   why-bialet
   html-first
   installation
   editor-support
   wren

.. toctree::
   :hidden:
   :maxdepth: 2
   :caption: First Steps

   getting-started/index

.. toctree::
   :hidden:
   :maxdepth: 2
   :caption: Data & Storage

   database
   file
   datetime

.. toctree::
   :hidden:
   :maxdepth: 2
   :caption: User Interfaces

   template
   jsx-migration
   forms
   markdown
   errors

.. toctree::
   :hidden:
   :maxdepth: 2
   :caption: Connecting Things

   advanced-routing
   external-modules
   rest-api
   http-calls

.. toctree::
   :hidden:
   :maxdepth: 2
   :caption: Running It

   configuration
   usage
   live-reload
   security
   cron
   tests
   deployment

.. toctree::
   :hidden:
   :maxdepth: 2
   :caption: Help & Reference

   reference
   examples
   faq
   ai-prompt

----

.. raw:: html

   <div class="community-banner">
     <p class="community-banner__title"><strong>Let's build together</strong></p>
     <p>
       Bialet is maintained by a solo developer, but the community is open to everyone.
       Whether you're stuck on a query, have a feature idea, or want to show off your
       project, GitHub Discussions is the place. Search first, ask second, and be kind.
     </p>
     <a class="sd-sphinx-override sd-btn sd-text-wrap sd-btn-outline-secondary" href="https://github.com/bialet/bialet/discussions">Join the discussion →</a>
   </div>
