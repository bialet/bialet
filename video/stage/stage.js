/* Deterministic timeline for the Bialet guestbook demo.
   Everything is a pure function of `t` (ms), so frames can be captured
   out of order and re-rendered identically. */

(function () {
  const D = window.BIALET_DATA;

  const el = {
    deck: document.getElementById("deck"),
    title: document.getElementById("title"),
    sub: document.getElementById("sub"),
    tabs: document.getElementById("tabs"),
    codeBody: document.getElementById("code-body"),
    codePanel: document.getElementById("code-panel"),
    browser: document.getElementById("browser-panel"),
    term: document.getElementById("term-panel"),
    termBody: document.getElementById("term-body"),
    screen: document.getElementById("screen"),
    shots: document.getElementById("shots"),
    ghost: document.getElementById("ghost"),
    ghostText: document.getElementById("ghost-text"),
    caret: document.getElementById("caret"),
    ripple: document.getElementById("ripple"),
    url: document.getElementById("url"),
    endcard: document.getElementById("endcard"),
    bar: document.getElementById("bar"),
  };

  const clamp01 = (x) => Math.max(0, Math.min(1, x));
  const fade = (t, t0, dur) => clamp01((t - t0) / dur);

  const fileById = (id) => D.files.find((f) => f.id === id);

  /* ------------------------------------------------------------- shots */

  const shotEls = {};
  for (const [key, shot] of Object.entries(D.shots)) {
    const img = document.createElement("img");
    img.src = shot.src;
    img.alt = "";
    shotEls[key] = img;
    el.shots.appendChild(img);
  }

  const CHROME = 62;

  function setShot(key) {
    for (const [k, img] of Object.entries(shotEls)) {
      img.style.opacity = k === key ? "1" : "0";
    }
    const shot = D.shots[key];
    const screenH = Math.round((878 * shot.h) / shot.w);
    el.screen.style.height = screenH + "px";
    el.browser.style.height = screenH + CHROME + "px";
  }

  function setURL(text) {
    el.url.textContent = text;
  }

  /* ------------------------------------------------------------ panels */

  function hidePanels() {
    el.codePanel.style.display = "none";
    el.browser.style.display = "none";
    el.term.style.display = "none";
    el.ghost.style.display = "none";
    el.ripple.style.display = "none";
    el.endcard.style.opacity = "0";
  }

  function showCode() {
    el.codePanel.style.display = "block";
  }

  function showBrowser(mode) {
    el.browser.style.display = "block";
    el.browser.classList.toggle("is-small", mode === "small");
  }

  function showTerm() {
    el.term.style.display = "block";
  }

  /* -------------------------------------------------------------- code */

  const MAX_LINES = 29;

  function renderCode(tabIds, activeId, t, start, perLine) {
    el.tabs.innerHTML = tabIds
      .map((id) => {
        const f = fileById(id);
        const cls = id === activeId ? "tab is-active" : "tab";
        return `<span class="${cls}">${f.label}</span>`;
      })
      .join("");

    const file = fileById(activeId);
    const shown = Math.max(0, Math.min(file.lines.length, Math.floor((t - start) / perLine) + 1));
    const first = Math.max(0, shown - MAX_LINES);

    let html = "";
    for (let i = first; i < shown; i++) {
      const op = fade(t, start + i * perLine, 130);
      html += `<span class="line" style="opacity:${op.toFixed(3)}">${file.lines[i]}</span>`;
    }
    el.codeBody.innerHTML = html || '<span class="line">&nbsp;</span>';
  }

  /* ------------------------------------------------------- ghost input */

  function ghostTyping(key, t, tStart, tEnd, text, hideAt) {
    const app = D.rects[key];
    const box = app && app.input;
    if (!box) return;
    const stop = hideAt === undefined ? tEnd + 120 : hideAt;
    if (t < tStart - 300 || t > stop) return;

    el.ghost.style.display = "flex";
    el.ghost.style.left = (box.x / app.w) * 100 + "%";
    el.ghost.style.top = (box.y / app.h) * 100 + "%";
    el.ghost.style.width = (box.w / app.w) * 100 + "%";
    el.ghost.style.height = (box.h / app.h) * 100 + "%";

    const progress = clamp01((t - tStart) / (tEnd - tStart));
    const typed = text.slice(0, Math.round(progress * text.length));
    const empty = typed.length === 0;

    el.ghost.classList.toggle("is-empty", empty);
    el.ghostText.textContent = empty ? D.placeholder : typed;
    el.caret.style.display = Math.floor(t / 520) % 2 === 0 ? "inline-block" : "none";
  }

  function rippleAt(key, t, t0) {
    const app = D.rects[key];
    const box = app && app.button;
    if (!box || t < t0 || t > t0 + 340) return;

    const p = (t - t0) / 340;
    const size = box.h * (1 + p * 1.5);
    el.ripple.style.display = "block";
    el.ripple.style.left = ((box.x + box.w / 2) / app.w) * 100 + "%";
    el.ripple.style.top = ((box.y + box.h / 2) / app.h) * 100 + "%";
    el.ripple.style.width = (size / app.w) * 100 + "%";
    el.ripple.style.height = (size / app.h) * 100 + "%";
    el.ripple.style.opacity = (0.55 * (1 - p)).toFixed(3);
  }

  /* ------------------------------------------------------------ scenes */

  function scene0(t) {
    showBrowser("solo");
    setShot(t < 5180 ? "styledBefore" : t < 6080 ? "styledSaved" : "styledAfter");
    setURL(
      t < 5180
        ? "127.0.0.1:7001/"
        : t < 6080
        ? "127.0.0.1:7001/message"
        : "127.0.0.1:7001/?new=1"
    );
    ghostTyping("styled", t, 1400, 4600, "shipped in an afternoon", 5120);
    rippleAt("styled", t, 5000);
  }

  function scene1(t) {
    showCode();
    const tabs = ["mig1", "idx1", "msg1"];
    if (t < 11200) renderCode(tabs, "mig1", t, 8500, 400);
    else if (t < 16800) renderCode(tabs, "idx1", t, 11400, 330);
    else renderCode(tabs, "msg1", t, 17000, 400);
  }

  function scene2(t) {
    showTerm();
    showBrowser("small");

    const saved = 29750;
    const after = 30500;

    setShot(t < saved ? "plainBefore" : t < after ? "plainSaved" : "plainAfter");
    setURL(
      t < saved
        ? "127.0.0.1:7001/"
        : t < after
        ? "127.0.0.1:7001/message"
        : "127.0.0.1:7001/"
    );

    ghostTyping("plain", t, 26200, 29000, "does it work?", 29300);
    rippleAt("plain", t, 29600);

    let html = "";
    const cmdStart = 22250;
    if (t >= cmdStart) {
      const typed = Math.round(clamp01((t - cmdStart) / 480) * 10);
      html += `<span class="term__line"><span class="term__prompt">$ </span>${"bialet dev".slice(0, typed)}</span>`;
    }
    for (const line of D.terminal) {
      if (t < line.at) continue;
      const op = fade(t, line.at, 140);
      html += `<span class="term__line" style="opacity:${op.toFixed(3)}">${line.html}</span>`;
    }
    el.termBody.innerHTML = html;
  }

  function scene3(t) {
    showCode();
    const tabs = ["css", "app"];
    if (t < 36500) renderCode(tabs, "css", t, 31300, 170);
    else renderCode(tabs, "app", t, 36600, 195);
  }

  function scene4(t) {
    showCode();
    const tabs = ["app", "idx2", "msg2"];
    if (t < 44300) renderCode(tabs, "idx2", t, 40300, 190);
    else renderCode(tabs, "msg2", t, 44400, 200);
  }

  function scene5(t) {
    showBrowser("solo");
    setShot("styledAfter");
    setURL("127.0.0.1:7001/?new=1");
    el.endcard.style.opacity = fade(t, 50000, 600).toFixed(3);
  }

  const SCENES = [
    { t0: 0, draw: scene0 },
    { t0: 8000, draw: scene1 },
    { t0: 22000, draw: scene2 },
    { t0: 31000, draw: scene3 },
    { t0: 40000, draw: scene4 },
    { t0: 48000, draw: scene5 },
  ];

  const BOUNDARIES = SCENES.slice(1).map((s) => s.t0);

  const CAPTIONS = [
    [0, "this is the whole app", "one binary · no build step · SQLite inside"],
    [7000, "and that is all of it", "no ORM, no bundler, no config"],
    [8200, "every line of it", "_migration.wren · index.wren · message.wren"],
    [17400, "every line of it", "there is nothing else in the project"],
    [22300, "$ bialet dev", "live reload, errors in the browser, no build step"],
    [27400, "it already works", "it is just ugly"],
    [31200, "one stylesheet", "css/style.css"],
    [36500, "and one layout class", "_app.wren → Template.layout"],
    [40200, "two imports", "index.wren · message.wren"],
    [48000, "same app, styled", "one stylesheet · one layout class"],
  ];

  function drawCaption(t) {
    let i = 0;
    for (let n = 0; n < CAPTIONS.length; n++) if (t >= CAPTIONS[n][0]) i = n;
    const [t0, title, sub] = CAPTIONS[i];
    el.title.textContent = title;
    el.sub.textContent = sub;

    const p = fade(t, t0, 340);
    const shift = (1 - p) * 16;
    for (const node of [el.title, el.sub]) {
      node.style.opacity = (0.15 + 0.85 * p).toFixed(3);
      node.style.transform = `translateY(${shift.toFixed(2)}px)`;
    }
  }

  function dip(t) {
    let o = 1;
    for (const b of BOUNDARIES) {
      if (t >= b - 200 && t < b) {
        o = Math.min(o, 0.12 + 0.88 * ((b - t) / 200));
      } else if (t >= b && t < b + 220) {
        o = Math.min(o, 0.12 + 0.88 * ((t - b) / 220));
      }
    }
    return o;
  }

  function seek(t) {
    t = Math.max(0, Math.min(D.duration, t));

    hidePanels();
    let scene = SCENES[0];
    for (const s of SCENES) if (t >= s.t0) scene = s;
    scene.draw(t);

    drawCaption(t);
    el.deck.style.opacity = dip(t).toFixed(3);
    el.bar.style.width = ((t / D.duration) * 100).toFixed(3) + "%";
  }

  window.seek = seek;
  window.seek(0);
})();
