/* Small behaviour for the landing page: copy buttons and install hints.
   Kept separate from the markup so the page ships no inline handlers. */
(function () {
  "use strict";

  function init() {
    var status = document.querySelector(".copy-status");

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
    if (/windows/i.test(ua)) {
      win.classList.add("is-visible");
      other.classList.remove("is-visible");
    } else if (/linux|mac/i.test(ua)) {
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
