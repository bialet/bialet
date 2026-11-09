#!/usr/bin/env python3
"""Render the guestbook demo video from the real app.

Pipeline
  1. copy video/app-v1 and video/app-v2 into a temp dir, run ``bialet`` for each
  2. drive a real headless browser: post messages, screenshot before/saved/after
  3. highlight the app sources with Pygments and write stage/data.js
  4. serve video/stage, step the timeline frame by frame, screenshot each frame
  5. encode H.264 (1080x1920 + 720x1280) and a poster with ffmpeg

Usage
  .venv/bin/python video/build.py            # full build -> video/out/
  .venv/bin/python video/build.py --fast     # quicker encode, same output
"""

from __future__ import annotations

import argparse
import html
import json
import shutil
import socket
import subprocess
import sys
import tempfile
import threading
import time
from functools import partial
from http.server import SimpleHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path

from playwright.sync_api import sync_playwright
from pygments import lex
from pygments.lexers import CssLexer, WrenLexer
from pygments.token import STANDARD_TYPES, Token

ROOT = Path(__file__).resolve().parent
REPO = ROOT.parent
EXAMPLES = REPO / "docs" / "examples"
OUT = REPO / "docs" / "_static"
STILLS = ROOT / "out"

DURATION = 52_000
FPS = 30
FPS_FAST = 24
VIEWPORT = {"width": 720, "height": 860}
DEVICE_SCALE = 2

MSG_1 = "hello from a .wren file"
MSG_2 = "does it work?"
MSG_3 = "shipped in an afternoon"

TERMINAL_AT = [23_000, 23_600, 24_400, 29_850]

CSS_EXCERPT = 29  # lines of style.css shown, plus an ellipsis line


# --------------------------------------------------------------- helpers


def log(message: str) -> None:
    print(f"  {message}", flush=True)


def free_port() -> int:
    with socket.socket() as sock:
        sock.bind(("127.0.0.1", 0))
        return sock.getsockname()[1]


def wait_for_port(port: int, timeout: float = 15.0) -> None:
    deadline = time.time() + timeout
    while time.time() < deadline:
        try:
            with socket.create_connection(("127.0.0.1", port), timeout=0.2):
                return
        except OSError:
            time.sleep(0.05)
    raise RuntimeError(f"bialet did not open port {port}")


def bialet_binary() -> str:
    local = REPO / "build" / "bialet"
    if local.exists() and local.stat().st_mode & 0o111:
        return str(local)
    found = shutil.which("bialet")
    if not found:
        sys.exit("bialet binary not found: run `make` or install it")
    return found


def token_class(ttype) -> str:
    cls = STANDARD_TYPES.get(ttype)
    if cls:
        return cls
    for parent, name in STANDARD_TYPES.items():
        if ttype in parent:
            return name
    return ""


def highlight_lines(path: Path, lexer=None) -> list[str]:
    source = path.read_text(encoding="utf-8").rstrip("\n")
    buckets: list[list[tuple[str, str]]] = [[]]

    for ttype, value in lex(source, lexer or WrenLexer()):
        cls = "" if ttype in Token.Text else token_class(ttype)
        for index, part in enumerate(value.split("\n")):
            if index:
                buckets.append([])
            if part:
                buckets[-1].append((cls, part))

    lines = []
    for segments in buckets:
        out = "".join(
            f'<span class="{cls}">{html.escape(text)}</span>' if cls else html.escape(text)
            for cls, text in segments
        )
        lines.append(out or "&nbsp;")
    return lines


# --------------------------------------------------------------- capture


class QuietHandler(SimpleHTTPRequestHandler):
    def log_message(self, *args):  # noqa: D401 - silence http.server
        return


def serve(directory: Path) -> ThreadingHTTPServer:
    handler = partial(QuietHandler, directory=str(directory))
    server = ThreadingHTTPServer(("127.0.0.1", 0), handler)
    threading.Thread(target=server.serve_forever, daemon=True).start()
    return server


def post(page, base: str, message: str) -> None:
    page.goto(base + "/")
    page.fill('input[name="msg"]', message)
    with page.expect_navigation():
        page.click("form button")
    page.wait_for_load_state("networkidle")


def rect(page, selector: str) -> dict:
    box = page.locator(selector).first.bounding_box()
    if not box:
        raise RuntimeError(f"no box for {selector}")
    return {
        "x": box["x"] * DEVICE_SCALE,
        "y": box["y"] * DEVICE_SCALE,
        "w": box["width"] * DEVICE_SCALE,
        "h": box["height"] * DEVICE_SCALE,
    }


def capture_app(browser, app_dir, work_dir, assets, tag, seed, typed, clip_height=None):
    port = free_port()
    base = f"http://127.0.0.1:{port}"

    work_dir.mkdir(parents=True, exist_ok=True)
    shutil.copytree(app_dir, work_dir, dirs_exist_ok=True)
    for stale in work_dir.glob("_db.sqlite3*"):
        stale.unlink()

    clip = None
    if clip_height:
        clip = {"x": 0, "y": 0, "width": VIEWPORT["width"], "height": clip_height}

    log_path = work_dir.parent / f"{tag}.log"
    with open(log_path, "w", encoding="utf-8") as log_file:
        process = subprocess.Popen(
            [bialet_binary(), "-p", str(port), str(work_dir)],
            cwd=work_dir,
            stdout=log_file,
            stderr=subprocess.STDOUT,
        )
        try:
            wait_for_port(port)
            page = browser.new_page(viewport=VIEWPORT, device_scale_factor=DEVICE_SCALE)

            for message in seed:
                post(page, base, message)
                time.sleep(1.15)  # distinct created_at seconds

            page.goto(base + "/")
            page.wait_for_load_state("networkidle")
            boxes = {"input": rect(page, 'input[name="msg"]'), "button": rect(page, "form button")}
            page.screenshot(path=str(assets / f"{tag}-before.png"), clip=clip)

            page.fill('input[name="msg"]', typed)
            with page.expect_navigation():
                page.click("form button")
            page.wait_for_load_state("networkidle")
            page.screenshot(path=str(assets / f"{tag}-saved.png"), clip=clip)

            page.goto(base + "/?new=1")
            page.wait_for_load_state("networkidle")
            page.screenshot(path=str(assets / f"{tag}-after.png"), clip=clip)

            page.close()
        finally:
            process.terminate()
            process.wait(timeout=10)

    dims = {
        "w": VIEWPORT["width"] * DEVICE_SCALE,
        "h": (clip_height or VIEWPORT["height"]) * DEVICE_SCALE,
    }
    return boxes, log_path.read_text(encoding="utf-8", errors="replace"), dims, port


def terminal_lines(raw_log: str, port: int | None = None) -> list[dict]:
    picked: list[str] = []
    for line in raw_log.splitlines():
        text = line.split("Log ", 1)[-1] if " Log " in line else line
        text = text.split(" ", 2)[-1] if text[:4].isdigit() else text
        text = text.strip()
        if not text or text.startswith("Path too long"):
            continue
        if "riding" in text or "Migration applied" in text or text.startswith("Request "):
            if port:
                text = text.replace(f":{port}", ":7001")
            if text not in picked:
                picked.append(text)
        if len(picked) == len(TERMINAL_AT):
            break

    fallback = [
        "🚲 bialet is riding on http://127.0.0.1:7001",
        "Migration applied - init",
        "Request GET /",
        "Request POST /message",
    ]
    while len(picked) < len(TERMINAL_AT):
        picked.append(fallback[len(picked)])

    return [
        {"at": at, "html": html.escape(text)}
        for at, text in zip(TERMINAL_AT, picked)
    ]


# ------------------------------------------------------------ rendering


def render_frames(stage_dir: Path, frames_dir: Path, fps: int, stills: list[int] | None = None) -> None:
    server = serve(stage_dir)
    port = server.server_address[1]
    times = stills if stills is not None else [
        round(index * 1000 / fps) for index in range(round(DURATION / 1000 * fps))
    ]

    with sync_playwright() as playwright:
        browser = playwright.chromium.launch()
        page = browser.new_page(viewport={"width": 1080, "height": 1920}, device_scale_factor=1)
        page.goto(f"http://127.0.0.1:{port}/stage.html")
        page.wait_for_function(
            "document.images.length > 0 && "
            "Array.from(document.images).every(i => i.complete && i.naturalWidth > 0)"
        )
        page.wait_for_timeout(150)

        for index, t in enumerate(times):
            page.evaluate(f"window.seek({t})")
            name = f"still-{t:05d}.png" if stills is not None else f"f{index:05d}.png"
            page.screenshot(path=str(frames_dir / name))
            if index % fps == 0:
                log(f"frame {index}/{len(times)}")

        browser.close()

    server.shutdown()


def encode(frames_dir: Path, fps: int, fast: bool) -> None:
    OUT.mkdir(parents=True, exist_ok=True)
    preset = "veryfast" if fast else "slow"
    crf = "22" if fast else "20"
    common = ["ffmpeg", "-y", "-loglevel", "error", "-framerate", str(fps), "-i", str(frames_dir / "f%05d.png")]

    subprocess.run(
        common
        + ["-vf", "format=yuv420p", "-c:v", "libx264", "-preset", preset, "-crf", crf,
           "-movflags", "+faststart", "-r", str(fps), str(OUT / "guestbook-demo.mp4")],
        check=True,
    )
    subprocess.run(
        common
        + ["-vf", "scale=720:1280:flags=lanczos,format=yuv420p", "-c:v", "libx264",
           "-preset", preset, "-crf", crf, "-movflags", "+faststart", "-r", str(fps),
           str(OUT / "guestbook-demo-720.mp4")],
        check=True,
    )

    poster_frame = frames_dir / "f00036.png"  # ~1.2s: styled app, before typing
    subprocess.run(
        ["ffmpeg", "-y", "-loglevel", "error", "-i", str(poster_frame),
         "-q:v", "3", str(OUT / "guestbook-demo-poster.jpg")],
        check=True,
    )


# ----------------------------------------------------------------- main


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--fast", action="store_true", help="lower fps and faster x264 preset")
    parser.add_argument("--stills", help="comma-separated ms values: render only these frames")
    args = parser.parse_args()

    stills = [int(v) for v in args.stills.split(",")] if args.stills else None

    fps = FPS_FAST if args.fast else FPS
    build = Path(tempfile.mkdtemp(prefix="bialet-video-"))
    stage_dir = build / "stage"
    assets = stage_dir / "assets"
    frames_dir = build / "frames"
    assets.mkdir(parents=True)
    frames_dir.mkdir()

    log(f"work dir {build}")

    with sync_playwright() as playwright:
        browser = playwright.chromium.launch()

        log("capturing the plain guestbook example")
        plain_boxes, plain_log, plain_dims, plain_port = capture_app(
            browser,
            EXAMPLES / "guestbook",
            build / "run-plain",
            assets,
            "plain",
            [MSG_1],
            MSG_2,
            clip_height=460,
        )

        log("capturing the styled guestbook example")
        styled_boxes, _, styled_dims, _ = capture_app(
            browser,
            EXAMPLES / "guestbook-styled",
            build / "run-styled",
            assets,
            "styled",
            [MSG_1, MSG_2],
            MSG_3,
        )

        browser.close()

    log("highlighting sources")
    data = {
        "duration": DURATION,
        "image": {"w": VIEWPORT["width"] * DEVICE_SCALE, "h": VIEWPORT["height"] * DEVICE_SCALE},
        "placeholder": "Write a message...",
        "shots": {
            "plainBefore": {"src": "assets/plain-before.png", **plain_dims},
            "plainSaved": {"src": "assets/plain-saved.png", **plain_dims},
            "plainAfter": {"src": "assets/plain-after.png", **plain_dims},
            "styledBefore": {"src": "assets/styled-before.png", **styled_dims},
            "styledSaved": {"src": "assets/styled-saved.png", **styled_dims},
            "styledAfter": {"src": "assets/styled-after.png", **styled_dims},
        },
        "rects": {
            "plain": {**plain_boxes, **plain_dims},
            "styled": {**styled_boxes, **styled_dims},
        },
        "terminal": terminal_lines(plain_log, plain_port),
        "files": [
            {"id": "mig1", "label": "_migration.wren",
             "lines": highlight_lines(EXAMPLES / "guestbook" / "_migration.wren")},
            {"id": "idx1", "label": "index.wren",
             "lines": highlight_lines(EXAMPLES / "guestbook" / "index.wren")},
            {"id": "msg1", "label": "message.wren",
             "lines": highlight_lines(EXAMPLES / "guestbook" / "message.wren")},
            {"id": "css", "label": "style.css",
             "lines": highlight_lines(
                 EXAMPLES / "guestbook-styled" / "css" / "style.css", CssLexer()
             )[:CSS_EXCERPT]
             + ['<span class="c1">/* &hellip; */</span>']},

            {"id": "app", "label": "_app.wren",
             "lines": highlight_lines(EXAMPLES / "guestbook-styled" / "_app.wren")},
            {"id": "idx2", "label": "index.wren",
             "lines": highlight_lines(EXAMPLES / "guestbook-styled" / "index.wren")},
            {"id": "msg2", "label": "message.wren",
             "lines": highlight_lines(EXAMPLES / "guestbook-styled" / "message.wren")},
        ],
    }

    for name in ("stage.html", "stage.css", "stage.js"):
        shutil.copy(ROOT / "stage" / name, stage_dir / name)
    (stage_dir / "data.js").write_text(
        "window.BIALET_DATA = " + json.dumps(data, ensure_ascii=False) + ";\n", encoding="utf-8"
    )

    log(f"rendering {round(DURATION / 1000 * fps)} frames at {fps} fps")
    render_frames(stage_dir, frames_dir, fps, stills)

    if stills:
        STILLS.mkdir(parents=True, exist_ok=True)
        for still in sorted(frames_dir.iterdir()):
            shutil.copy(still, STILLS / still.name)
        log("stills written to video/out")
        return

    log("encoding")
    encode(frames_dir, fps, args.fast)

    log("done")
    for item in (OUT / "guestbook-demo.mp4", OUT / "guestbook-demo-720.mp4", OUT / "guestbook-demo-poster.jpg"):
        log(f"{item.name}  {item.stat().st_size / 1024:.0f} KB")


if __name__ == "__main__":
    main()
