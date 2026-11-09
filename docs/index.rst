One executable: HTTP server, SQLite, and a scripting language.
================================================================

.. meta::
    :description: Bialet is a single-binary web framework: an HTTP server, SQLite, and the Wren scripting language in one executable. File-based routing, no build step.
    :keywords: Bialet, web framework, Wren, SQLite, file-based routing, zero dependencies, server-rendered

.. llms-txt-ignore-start

.. raw:: html

   <p class="hero__lede">
     Build data-driven web apps from <code>.wren</code> files. No package
     manager, no YAML, no separate database server, no build step: one
     executable (3.7&nbsp;MB on Linux x86_64) runs the HTTP server, opens the
     SQLite file, and returns the HTML.
   </p>

.. raw:: html

   <div id="bialet-download" class="install">

     <!-- Unix (Linux / macOS): curl command with copy button -->
     <div id="download-unix" class="install__platform">
       <span id="download-os-label-unix" class="install__label"></span>
       <div class="install__command install__command--primary">
         <code class="install__text">curl -fsSL https://get.bialet.dev | sh</code>
         <button type="button" class="install__copy" data-copy="curl -fsSL https://get.bialet.dev | sh" aria-label="Copy install command">
           <svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" stroke-width="1.5" stroke="currentColor" fill="none" stroke-linecap="round" stroke-linejoin="round" aria-hidden="true"><rect x="8" y="8" width="12" height="12" rx="2"/><path d="M16 8v-2a2 2 0 0 0 -2 -2h-8a2 2 0 0 0 -2 2v8a2 2 0 0 0 2 2h2"/></svg>
           <span class="install__copy-text">Copy</span>
         </button>
       </div>
       <p class="install__note">
         macOS ARM, Ubuntu x86_64, and Ubuntu ARM64.
       </p>
       <details class="install__readfirst">
         <summary>Read the script before running it</summary>
         <div class="install__command">
           <code class="install__text">curl -fsSL https://get.bialet.dev -o install.sh</code>
         </div>
         <p class="install__note">
           Inspect it, then run <code>sh install.sh</code>.
         </p>
       </details>
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
       <a href="installation.html">Other platforms, Homebrew, and Docker</a>
       &middot; MIT licensed &middot; v{{bialet_version}}
     </p>

   </div>

   <div class="hero-actions">
     <a class="hero-actions__link" href="getting-started/index.html">Read the docs</a>
     <a class="hero-actions__link" href="https://github.com/bialet/bialet">
       <svg version="1.1" width="1em" height="1em" class="sd-octicon sd-octicon-mark-github" viewBox="0 0 16 16" aria-hidden="true"><path fill-rule="evenodd" d="M8 0C3.58 0 0 3.58 0 8c0 3.54 2.29 6.53 5.47 7.59.4.07.55-.17.55-.38 0-.19-.01-.82-.01-1.49-2.01.37-2.53-.49-2.69-.94-.09-.23-.48-.94-.82-1.13-.28-.15-.68-.52-.01-.53.63-.01 1.08.58 1.23.82.72 1.21 1.87.87 2.33.66.07-.52.28-.87.51-1.07-1.78-.2-3.64-.89-3.64-3.95 0-.87.31-1.59.82-2.15-.08-.2-.36-1.02.08-2.12 0 0 .67-.21 2.2.82.64-.18 1.32-.27 2-.27.68 0 1.36.09 2 .27 1.53-1.04 2.2-.82 2.2-.82.44 1.1.16 1.92.08 2.12.51.56.82 1.27.82 2.15 0 3.07-1.87 3.75-3.65 3.95.29.25.54.73.54 1.48 0 1.07-.01 1.93-.01 2.2 0 .21.15.46.55.38A8.013 8.013 0 0016 8c0-4.42-3.58-8-8-8z"></path></svg>
       View on GitHub
     </a>
   </div>

   <p class="hero__trust">
     In development since 2023 &middot; running in production at
     <a href="https://home.dev.ar/">home.dev.ar</a>
   </p>

   <!-- One proof: the tutorial app, and the code that renders it. -->
   <div class="proof">
     <figure class="proof__shot">
       <img src="_static/poll-vote.png" width="955" height="508" loading="lazy" decoding="async" alt="A page titled Poll asking &quot;Has web development become overly complex?&quot; with Yes and No options and a Vote button." />
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
     <p class="community-banner__action">
       Or feed the whole site to your tool:
       <a href="llms.txt">llms.txt</a> (index) and
       <a href="llms-full.txt">llms-full.txt</a> (every page, plain text).
     </p>
   </div>

   <div class="copy-status" role="status" aria-live="polite"></div>

.. llms-txt-ignore-end

----

What you get
------------

.. grid:: 1 2 2 2
   :gutter: 2

   .. grid-item-card::

      :octicon:`package;2em`
      ^^^
      **One executable**

      There is no runtime and no ``node_modules`` directory. Copy the binary
      to a server and run it. The Linux x86_64 binary is 3.7 MB.

   .. grid-item-card::

      :octicon:`database;2em`
      ^^^
      **SQLite is included**

      The database is one file. No connection strings, no service to
      provision, no separate process. WAL mode is one flag.

   .. grid-item-card::

      :octicon:`zap;2em`
      ^^^
      **No build step**

      Save a ``.wren`` file and refresh the browser. No compiler, no bundler,
      no watch process.

   .. grid-item-card::

      :octicon:`code;2em`
      ^^^
      **HTML, CSS, and SQL**

      Server-rendered pages, standard HTML and CSS, and SQL you write
      yourself. Add JavaScript only if you want it.

----

A whole app in one file:
------------------------

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
an HTTP server and a built-in SQLite database — **in a single binary**.
No configuration files, no runtime dependencies, no build step. Just write
``.wren`` files and run.

Run ``bialet`` in your project folder and open `127.0.0.1:7001 <http://127.0.0.1:7001>`_.

----

Where it fits
-------------

.. grid:: 1 2 3 3
   :gutter: 2

   .. grid-item-card::

      :octicon:`beaker;2em`
      ^^^
      **Internal tools and admin panels**

      One process, one SQLite file, and a few users on one small server.

   .. grid-item-card::

      :octicon:`mortar-board;2em`
      ^^^
      **Prototypes and small sites**

      One directory to deploy. Add a reverse proxy when the site goes
      public.

   .. grid-item-card::

      :octicon:`heart;2em`
      ^^^
      **Learning and teaching**

      HTML, SQL, and one scripting language. Nothing else to install.

Not a fit if you need horizontal scaling, in-process TLS, WebSockets, or a
package ecosystem. Bialet does not provide them. See :doc:`why-bialet` for the
longer version.

.. raw:: html

   <div class="hero-actions">
     <a class="sd-sphinx-override sd-btn sd-text-wrap sd-btn-primary" href="getting-started/index.html">Get Started</a>
     <a class="hero-actions__link" href="installation.html">Install options</a>
   </div>

----

What it costs
-------------

Bialet is small on purpose. These are the limits that come with that.

.. grid:: 1 2 2 2
   :gutter: 2

   .. grid-item::

      **One connection at a time**

      The server is single-threaded and blocking. Run it behind a reverse
      proxy, which is also where request caps and timeouts belong.

   .. grid-item::

      **HTTP/1.0, no native TLS**

      Bialet serves HTTP. Put nginx, Apache, or Caddy in front for HTTPS,
      HTTP/2, body limits, and timeouts. See :doc:`deployment`.

   .. grid-item::

      **Wren, with basic tooling**

      Apps are written in Wren. The VS Code and Vim plugins provide syntax
      highlighting; autocomplete and an LSP are on the roadmap.

   .. grid-item::

      **No WebSockets**

      Request/response HTTP only. Use polling or an external service for
      realtime features.

----

.. llms-txt-ignore-start

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
     <p class="community-banner__title"><strong>Maintained by one developer</strong></p>
     <p>
       Bialet is stable for personal use. There is no commercial support or
       SLA. Bugs go to GitHub Issues; questions and project showcases go to
       GitHub Discussions. Planned work is in the
       <a href="https://github.com/bialet/bialet/blob/main/ROADMAP.md">roadmap</a>.
     </p>
     <a class="sd-sphinx-override sd-btn sd-text-wrap sd-btn-outline-secondary" href="https://github.com/bialet/bialet/discussions">Join the discussion →</a>
   </div>

   <div class="hero-actions hero-actions--final">
     <a class="sd-sphinx-override sd-btn sd-text-wrap sd-btn-primary" href="getting-started/index.html">Get Started</a>
     <a class="hero-actions__link" href="installation.html">Install options</a>
   </div>
