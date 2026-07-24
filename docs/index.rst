Bialet
======

.. meta::
    :description: Bialet is a web framework that fits in a tiny binary: Wren scripting, HTTP server, and SQLite — all in one file.

.. raw:: html

   <div style="text-align: center; margin: 2em 0 1em 0">
       <img src="_static/logo.png" alt="Bialet" style="width: 140px; margin-bottom: 0.5em" />
       <h1 style="font-size: 2.5em; font-weight: 700; margin: 0 0 0.25em 0">
           Web development became a spaceship. Bialet is a bicycle. 🚲
       </h1>
       <p style="font-size: 1.15em; color: var(--sd-color-secondary); max-width: 34em; margin: 0 auto 1.25em auto">
           Build data-driven web apps from a single file. No NPM, no YAML,
           no separate database servers. Just a <strong>tiny binary</strong> with
           a built-in HTTP server, Wren scripting, and SQLite.
       </p>
   </div>

.. raw:: html

   <div id="bialet-download" style="text-align: center; margin: 1.5em 0 2em 0">

     <!-- Unix (Linux / macOS): curl command with copy button -->
     <div id="download-unix" style="display: none">
       <span id="download-os-label-unix" style="display: block; font-size: 0.85em; color: var(--sd-color-secondary); margin-bottom: 0.5em"></span>
       <div style="background: var(--pst-color-on-background); border: 1px solid var(--pst-color-border); border-radius: 0.75rem; padding: 1em 1.25em; display: inline-flex; align-items: center; gap: 0.6em">
         <code style="font-size: 1.05em; background: none; word-break: break-all">curl -fsSL https://get.bialet.dev | sh</code>
         <button onclick="navigator.clipboard.writeText('curl -fsSL https://get.bialet.dev | sh')" style="background: none; border: 1px solid var(--pst-color-border); border-radius: 0.4rem; padding: 0.3em 0.55em; cursor: pointer; color: var(--pst-color-text-muted); flex-shrink: 0" title="Copy to clipboard" aria-label="Copy to clipboard">
           <svg width="14" height="14" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><rect x="9" y="9" width="13" height="13" rx="2" ry="2"/><path d="M5 15H4a2 2 0 0 1-2-2V4a2 2 0 0 1 2-2h9a2 2 0 0 1 2 2v1"/></svg>
         </button>
       </div>
       <p style="margin-top: 0.6em; font-size: 0.9em; color: var(--sd-color-secondary)">
         Prefer a manual download? <a href="https://github.com/bialet/bialet/releases">Get the tarball</a> &middot; <a href="installation.html">All platforms</a>
       </p>
     </div>

     <!-- Windows: download button -->
     <div id="download-win" style="display: none">
       <span style="display: block; font-size: 0.85em; color: var(--sd-color-secondary); margin-bottom: 0.5em">Detected: Windows</span>
       <a class="sd-sphinx-override sd-btn sd-text-wrap sd-btn-primary sd-shadow-sm" href="https://github.com/bialet/bialet/releases/latest" style="font-size: 1.05em; padding: 0.55em 1.5em">
         Download for Windows (.zip)
       </a>
       <p style="margin-top: 0.6em; font-size: 0.9em; color: var(--sd-color-secondary)">
         Zero installation required. Just extract and run.<br/>
         <a href="installation.html">See all platforms</a>
       </p>
     </div>

     <!-- Fallback: curl + all platforms -->
     <div id="download-other" style="display: block">
       <div style="background: var(--pst-color-on-background); border: 1px solid var(--pst-color-border); border-radius: 0.75rem; padding: 1em 1.25em; display: inline-flex; align-items: center; gap: 0.6em">
         <code style="font-size: 1.05em; background: none; word-break: break-all">curl -fsSL https://get.bialet.dev | sh</code>
         <button onclick="navigator.clipboard.writeText('curl -fsSL https://get.bialet.dev | sh')" style="background: none; border: 1px solid var(--pst-color-border); border-radius: 0.4rem; padding: 0.3em 0.55em; cursor: pointer; color: var(--pst-color-text-muted); flex-shrink: 0" title="Copy to clipboard" aria-label="Copy to clipboard">
           <svg width="14" height="14" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><rect x="9" y="9" width="13" height="13" rx="2" ry="2"/><path d="M5 15H4a2 2 0 0 1-2-2V4a2 2 0 0 1 2-2h9a2 2 0 0 1 2 2v1"/></svg>
         </button>
       </div>
       <p style="margin-top: 0.5em; margin-bottom: 0.5em; font-size: 0.85em; color: var(--sd-color-secondary)">
         macOS ARM &bull; Ubuntu x86_64 &bull; Ubuntu ARM
       </p>
       <a class="sd-sphinx-override sd-btn sd-text-wrap sd-btn-outline-secondary sd-shadow-sm" href="installation.html">Other platforms</a>
     </div>

   </div>

   <script>
     (function() {
       var unix = document.getElementById('download-unix');
       var win = document.getElementById('download-win');
       var other = document.getElementById('download-other');
       var label = document.getElementById('download-os-label-unix');
       if (!unix || !win || !other) return;
       var ua = navigator.userAgent || '';
       if (/windows/i.test(ua)) {
         win.style.display = 'block';
         other.style.display = 'none';
       } else if (/linux|mac/i.test(ua)) {
         var osName = /mac/i.test(ua) ? 'Detected: macOS' : 'Detected: Linux';
         if (label) label.textContent = osName;
         unix.style.display = 'block';
         other.style.display = 'none';
       }
     })();
   </script>

   <div style="text-align: center; margin: 1.5em 0 1.5em 0">
     <a class="sd-sphinx-override sd-btn sd-text-wrap sd-btn-primary sd-shadow-sm" href="getting-started/index.html" style="margin: 0 0.35em">Get Started</a>
     <a class="sd-sphinx-override sd-btn sd-text-wrap sd-btn-outline-secondary sd-shadow-sm" href="https://github.com/bialet/bialet" style="margin: 0 0.35em">
       <svg version="1.1" width="1em" height="1em" class="sd-octicon sd-octicon-mark-github" viewBox="0 0 16 16" aria-hidden="true"><path fill-rule="evenodd" d="M8 0C3.58 0 0 3.58 0 8c0 3.54 2.29 6.53 5.47 7.59.4.07.55-.17.55-.38 0-.19-.01-.82-.01-1.49-2.01.37-2.53-.49-2.69-.94-.09-.23-.48-.94-.82-1.13-.28-.15-.68-.52-.01-.53.63-.01 1.08.58 1.23.82.72 1.21 1.87.87 2.33.66.07-.52.28-.87.51-1.07-1.78-.2-3.64-.89-3.64-3.95 0-.87.31-1.59.82-2.15-.08-.2-.36-1.02.08-2.12 0 0 .67-.21 2.2.82.64-.18 1.32-.27 2-.27.68 0 1.36.09 2 .27 1.53-1.04 2.2-.82 2.2-.82.44 1.1.16 1.92.08 2.12.51.56.82 1.27.82 2.15 0 3.07-1.87 3.75-3.65 3.95.29.25.54.73.54 1.48 0 1.07-.01 1.93-.01 2.2 0 .21.15.46.55.38A8.013 8.013 0 0016 8c0-4.42-3.58-8-8-8z"></path></svg>
       View on GitHub
     </a>
   </div>

----

Features
========

.. grid:: 1 2 2 4
   :gutter: 2

   .. grid-item-card::
      :text-align: center
      :shadow: sm

      :octicon:`package;2em;sd-text-info`
      ^^^
      **A Tiny Single Binary**

      Drop the gigabytes of ``node_modules``. Bialet is a small,
      self-contained executable. Copy it to your server, and your app
      is deployed.

   .. grid-item-card::
      :text-align: center
      :shadow: sm

      :octicon:`database;2em;sd-text-info`
      ^^^
      **SQLite is Built-in**

      Forget about provisioning databases or writing connection strings.
      Real data persistence works completely out of the box.

   .. grid-item-card::
      :text-align: center
      :shadow: sm

      :octicon:`rocket;2em;sd-text-info`
      ^^^
      **Zero Build Step**

      Write a ``.wren`` file, save it, and refresh your browser.
      No compilers, no bundlers, no watch processes.

   .. grid-item-card::
      :text-align: center
      :shadow: sm

      :octicon:`code;2em;sd-text-info`
      ^^^
      **HTML is the Real Frontend**

      Use standard HTML and CSS. Bialet lets you build server-rendered
      apps the classic, straightforward way. Add JS only if you want to.

----

Your whole app in one file (``app.wren``)
=========================================

.. code-block:: wren

   // 1. Create your database table automatically
   `CREATE TABLE IF NOT EXISTS messages (text TEXT)`.query

   // 2. Handle POST requests and save data
   if (Request.isPost) {
     Db.save("messages", {"text": Request.post("msg")})
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

Bialet integrates the `Wren language <https://wren.io/>`_ with
a HTTP server and a built-in SQLite database — **in a single binary**.
No configuration files, no dependencies, no build step. Just write ``.wren``
files and run.

Run ``bialet`` in your project folder and open `127.0.0.1:7001 <http://127.0.0.1:7001>`_.

----

The Bialet Manifesto — Ride Light 🚲
====================================

.. admonition:: Ride Light
   :class: tip

   1. **Simplicity is a superpower.** Every line of tooling you don't write
      is a line of your app that ships faster.

   2. **Standards, not frameworks.** HTML, SQL, and HTTP have outlived every
      framework. Master them and your knowledge stays relevant.

   3. **One file to deploy.** No containers, no orchestration, no
      ``docker-compose.yml``. Copy the binary — that's it.

   4. **Batteries included.** Server, database, templating — all in one
      binary under 1 MB. No external services to provision.

   5. **Ride light.** Complexity is a choice. Choose less, and you'll go
      further than you think.

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
      **Internal Tools & Dashboards**

      Spin up a data-driven admin panel in minutes with zero
      infrastructure overhead.

   .. grid-item-card::
      :text-align: center
      :shadow: sm

      :octicon:`mortar-board;2em;sd-text-warning`
      ^^^
      **Learning & Teaching**

      Focus on HTML, SQL, and core logic. Skip the webpack configs
      and ORM abstractions.

   .. grid-item-card::
      :text-align: center
      :shadow: sm

      :octicon:`heart;2em;sd-text-warning`
      ^^^
      **Lovers of Simplicity**

      If you prefer one tool that does its job perfectly over a chaotic
      stack of micro-libraries, hop on.

----

.. raw:: html

   <div style="text-align: center; margin: 1.5em 0">
     <a class="sd-sphinx-override sd-btn sd-text-wrap sd-btn-primary sd-shadow-sm" href="getting-started/index.html" style="margin: 0 0.35em">Documentation</a>
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
