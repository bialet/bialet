# Guestbook demo video

The 52-second vertical clip used on the landing page, plus the 1080×1920
master for social.

**Outputs** (written to `docs/_static/`):

| File | Use |
|---|---|
| `guestbook-demo.mp4` | 1080×1920 master, for TikTok / Reels / Shorts |
| `guestbook-demo-720.mp4` | 720×1280, what the landing page loads |
| `guestbook-demo-poster.jpg` | poster frame, 1080×1920 |

**Sources**

- `../docs/examples/guestbook/` — the minimal app: `_migration.wren`,
  `index.wren`, `message.wren`.
- `../docs/examples/guestbook-styled/` — the same app plus `_app.wren` (the
  `Template` layout class) and `css/style.css`.
- `stage/` — an HTML page at 1080×1920 with a deterministic timeline
  (`window.seek(ms)`), screenshotted frame by frame.
- `build.py` — runs both apps with `bialet dev`, drives a real browser to post
  messages, screenshots before/after, highlights the sources with Pygments,
  steps the stage, and encodes with ffmpeg.

The clip has its own palette — charcoal, amber, coral, green — rather than the
site's navy and cyan, so it reads as an editor screen instead of competing with
the page around it. `stage/stage.css` holds it; the demo app's stylesheet uses
the same values.

Nothing in the video is a mockup: the pages, the responses, and the terminal
output come from running `bialet`. The typing is composited over real
screenshots so the frames are reproducible.

## Rebuilding

```bash
.venv/bin/python video/build.py            # 30 fps, full quality
.venv/bin/python video/build.py --fast     # 24 fps, quicker encode
.venv/bin/python video/build.py --stills 1000,19000,28000   # stills only
```

Stills land in `video/out/` for review; the video and poster overwrite
`docs/_static/`.

Requires the `bialet` binary (`build/bialet` or on `PATH`), ffmpeg, Playwright
with Chromium, and Pygments — all present in the repo's `.venv`.
