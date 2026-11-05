/* Small behaviour for the landing page: copy buttons and install hints.
   Kept separate from the markup so the page ships no inline handlers. */
(function () {
  "use strict";

  var desktop = window.matchMedia("(min-width: 641px)");

  // The theme toggle only says "Color mode", which states nothing about what
  // the button will do. Name the mode it switches to instead.
  function labelThemeSwitches() {
    // The theme cycles auto → dark → light with a light OS preference and
    // auto → light → dark with a dark one. Mirror that order.
    var dark = window.matchMedia("(prefers-color-scheme: dark)").matches;
    var order = dark ? ["auto", "light", "dark"] : ["auto", "dark", "light"];
    document.querySelectorAll(".theme-switch-button").forEach(function (button) {
      var mode = document.documentElement.dataset.mode || "auto";
      var next = order[(order.indexOf(mode) + 1) % order.length];
      var text = next === "auto" ? "Switch to the system theme" : "Switch to " + next + " mode";
      button.setAttribute("aria-label", text);
      button.setAttribute("title", text);
      button.setAttribute("data-bs-title", text);
    });
  }

  function init() {
    var status = document.querySelector(".copy-status");

    labelThemeSwitches();
    document.querySelectorAll(".theme-switch-button").forEach(function (button) {
      // The theme's own handler cycles the mode; re-read it afterwards.
      button.addEventListener("click", function () {
        window.setTimeout(labelThemeSwitches, 0);
      });
    });

    // On a phone the screenshot is the proof and the code samples are long;
    // leave both behind a summary so the page stays scannable.
    document.querySelectorAll("[data-collapse-on-phone]").forEach(function (el) {
      if (!desktop.matches) el.removeAttribute("open");
    });

    function announce(message) {
      if (status) status.textContent = message;
    }

    function copy(text, onDone) {
      if (!navigator.clipboard || !text) return;
      navigator.clipboard.writeText(text).then(onDone, function () {});
    }

    function flash(el, done) {
      el.dataset.copied = "true";
      announce("Copied to clipboard");
      window.setTimeout(function () {
        delete el.dataset.copied;
        announce("");
        if (done) done();
      }, 2000);
    }

    document.querySelectorAll("[data-copy]").forEach(function (button) {
      button.addEventListener("click", function () {
        copy(button.getAttribute("data-copy"), function () {
          flash(button);
        });
      });
    });

    document.querySelectorAll("[data-copy-prompt]").forEach(function (button) {
      var label = button.querySelector("[data-copy-label]");
      var original = label ? label.textContent : "";
      button.addEventListener("click", function () {
        copy(window.BIALET_AI_PROMPT, function () {
          if (label) label.textContent = "Copied";
          flash(button, function () {
            if (label) label.textContent = original;
          });
        });
      });
    });

    // Offer the install path that matches the visitor's system.
    var unix = document.querySelector("#download-unix");
    var win = document.querySelector("#download-win");
    var other = document.querySelector("#download-other");
    if (!unix || !win || !other) return;

    var ua = navigator.userAgent || "";
    // Phones and tablets contain the desktop names in their UA string: an
    // iPhone reports "like Mac OS X", Android reports "Linux", and iPadOS 13+
    // reports a desktop Mac UA with touch. None can run a shell command, so
    // they keep the download fallback.
    var handheld =
      /iphone|ipad|ipod|android/i.test(ua) ||
      (/macintosh/i.test(ua) && navigator.maxTouchPoints > 1);
    if (/windows/i.test(ua) && !handheld) {
      win.classList.add("is-visible");
      other.classList.remove("is-visible");
    } else if (/linux|mac/i.test(ua) && !handheld) {
      var label = document.querySelector("#download-os-label-unix");
      if (label) label.textContent = /mac/i.test(ua) ? "Install for macOS" : "Install for Linux";
      unix.classList.add("is-visible");
      other.classList.remove("is-visible");
    }
  }

  if (document.readyState === "loading") {
    document.addEventListener("DOMContentLoaded", init);
  } else {
    init();
  }
})();
