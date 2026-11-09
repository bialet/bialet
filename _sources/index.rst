Small on purpose.
=================

.. meta::
    :description: Bialet is a small web framework in one executable: an HTTP server, SQLite, and the Wren scripting language. No package manager, no YAML, no build step.
    :keywords: Bialet, web framework, Wren, SQLite, file-based routing, zero dependencies, server-rendered

.. llms-txt-ignore-start

.. raw:: html

   <p class="hero__subline">It should have stayed this simple.</p>

   <p class="hero__lede">
     One executable: an HTTP server, SQLite, and the Wren scripting language.
     Build data-driven web apps from <code>.wren</code> files. No package
     manager, no YAML, no separate database server, no build step. The Linux
     x86_64 binary is 3.7&nbsp;MB.
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
       <a href="installation.html">Other platforms, Homebrew, and Docker</a>
       &middot; MIT licensed &middot; v{{bialet_version}}
     </p>

   </div>

   <div class="hero-actions">
     <a class="sd-sphinx-override sd-btn sd-text-wrap sd-btn-primary" href="getting-started/index.html">Get Started</a>
     <a class="hero-actions__link" href="https://github.com/bialet/bialet">
       <svg version="1.1" width="1em" height="1em" class="sd-octicon sd-octicon-mark-github" viewBox="0 0 16 16" aria-hidden="true"><path fill-rule="evenodd" d="M8 0C3.58 0 0 3.58 0 8c0 3.54 2.29 6.53 5.47 7.59.4.07.55-.17.55-.38 0-.19-.01-.82-.01-1.49-2.01.37-2.53-.49-2.69-.94-.09-.23-.48-.94-.82-1.13-.28-.15-.68-.52-.01-.53.63-.01 1.08.58 1.23.82.72 1.21 1.87.87 2.33.66.07-.52.28-.87.51-1.07-1.78-.2-3.64-.89-3.64-3.95 0-.87.31-1.59.82-2.15-.08-.2-.36-1.02.08-2.12 0 0 .67-.21 2.2.82.64-.18 1.32-.27 2-.27.68 0 1.36.09 2 .27 1.53-1.04 2.2-.82 2.2-.82.44 1.1.16 1.92.08 2.12.51.56.82 1.27.82 2.15 0 3.07-1.87 3.75-3.65 3.95.29.25.54.73.54 1.48 0 1.07-.01 1.93-.01 2.2 0 .21.15.46.55.38A8.013 8.013 0 0016 8c0-4.42-3.58-8-8-8z"></path></svg>
       View on GitHub
     </a>
   </div>

   <!-- One proof: a build from the files to the stylesheet. -->
   <div class="proof">
     <figure class="proof__demo">
       <video id="guestbook-demo" class="proof__video" width="1080" height="1920"
              poster="_static/guestbook-demo-poster.jpg" muted loop playsinline
              preload="metadata" disablepictureinpicture
              aria-label="A guestbook built in Bialet: three files are written, run unstyled, then styled with a stylesheet and a layout class.">
         <source src="_static/guestbook-demo-720.mp4" type="video/mp4">
       </video>
       <figcaption>The guestbook example: built, run, then styled.
         <button type="button" class="proof__toggle" data-demo-toggle>Pause</button>
       </figcaption>
     </figure>
     <div class="proof__notes">
       <p class="proof__heading">Under a minute, start to styled</p>
       <ul class="proof__list">
         <li>A page, a form handler, a migration.</li>
         <li><code>bialet</code> serves the files. No build step.</li>
         <li>Then a stylesheet and a layout class.</li>
       </ul>
       <p class="proof__cta">
         <a class="sd-sphinx-override sd-btn sd-text-wrap sd-btn-outline-secondary" href="quickstart.html">Build this guestbook</a>
       </p>
     </div>
   </div>

   <script>
     (function () {
       var video = document.getElementById("guestbook-demo");
       var toggle = document.querySelector("[data-demo-toggle]");
       if (!video) return;

       function reflect() {
         if (!toggle) return;
         toggle.textContent = video.paused ? "Play" : "Pause";
       }

       if (toggle) {
         toggle.addEventListener("click", function () {
           if (video.paused) video.play().catch(function () {});
           else video.pause();
         });
         video.addEventListener("play", reflect);
         video.addEventListener("pause", reflect);
       }

       if (window.matchMedia("(prefers-reduced-motion: reduce)").matches) {
         reflect();
         return;
       }
       video.autoplay = true;
       var started = video.play();
       if (started && started.catch) started.catch(function () {});
     })();
   </script>

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

One executable does the work: it serves HTTP, opens the SQLite file, and runs
your Wren code. There is no stack to assemble around it.

.. grid:: 1 2 2 2
   :gutter: 2

   .. grid-item-card::

      :octicon:`package;2em`
      ^^^
      **One executable**

      Copy the binary to a server and run it. No runtime to install, no
      ``node_modules``. 3.7 MB on Linux x86_64.

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

What it costs
-------------

.. admonition:: The tradeoffs
   :class: note

   Small means bounded. These are the limits that come with it.

   - **One worker per process, with instant queueing.** Requests are served
     sequentially per worker. For parallel concurrency on multi-core servers,
     spin up multiple local instances behind Caddy or nginx. See
     :doc:`deployment`.
   - **HTTP/1.0, no native TLS.** Bialet serves HTTP. Put nginx, Apache, or
     Caddy in front for HTTPS, HTTP/2, rate limiting, request buffering, and
     static-asset caching. See :doc:`deployment`.
   - **Wren, with basic tooling.** Apps are written in Wren. The VS Code and
     Vim plugins provide syntax highlighting; autocomplete and an LSP are on
     the roadmap.
   - **No WebSockets.** Request/response HTTP only. Use polling or an external
     service for realtime features.

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

Not a fit if you need multi-server horizontal scaling, in-process TLS,
WebSockets, or a package ecosystem. Bialet does not provide them. See
:doc:`why-bialet` for the longer version.

.. raw:: html

   <div class="hero-actions">
     <a class="sd-sphinx-override sd-btn sd-text-wrap sd-btn-outline-secondary" href="getting-started/index.html">Read the docs</a>
     <a class="hero-actions__link" href="installation.html">All install options</a>
   </div>

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

   quickstart
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
