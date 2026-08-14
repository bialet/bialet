# Bialet Brand Guide

This document defines how Bialet looks, sounds, and is written about. It
applies to the website, README, social previews, talks, and any third-party
material that references the project. When in doubt, favor the plainer,
smaller, less decorated option — that's the whole point of the project.

## Philosophy

Bialet's identity follows the same rule as its code: **do less, on purpose.**

- **Clarity over decoration.** Every color, icon, and word has a job.
- **One symbol, not a mascot.** The bicycle is the whole idea — a small,
  human-scale tool next to an industry that keeps building spaceships.
- **Plain over slick.** No gradients, mascots, or marketing gloss that a
  one-file, zero-dependency tool wouldn't actually need.
- **Honest, not hyped.** Say what Bialet does and who it's for — including
  who it's *not* for — instead of overselling it.

Reference tagline (use verbatim, don't paraphrase):

> Web development became a spaceship. Bialet is a bicycle.

---

## Naming

| Term | Use for | Notes |
|---|---|---|
| `Bialet` | The project, the framework, the language runtime | Capital B, always. Never an acronym. |
| `bialet` | The CLI binary / command (`bialet dev`, `bialet run`) | Lowercase, matches the actual executable name. |
| `Wren` | The scripting language | Capital W. Bialet *embeds* Wren; Credit wren.io when relevant. |
| `.wren` | Source file extension | Always lowercase, always with the dot when written inline. |
| Ride Light | The project's guiding manifesto | Capitalize both words; treat as a proper name, not a slogan to remix. |

**Correct:**
- "Bialet is a single-binary web framework."
- "Run `bialet dev` to start the server."
- "Bialet embeds the Wren language."

**Incorrect:**
- "BiaLet", "Bialet.js", "the Bialet Framework™" — no camel case, no fake
  package suffix, no trademark symbol.
- "Bialet, the NPM alternative" — Bialet isn't defined by what it replaces.
- Writing "wren" (lowercase) for the language name in prose.

---

## Logo & Variants

The mark is a simple line-art **bicycle**, dark navy/slate on a light or
transparent circle — the same shape used at every size, never redrawn.

| Asset | File | Use |
|---|---|---|
| Primary logo | `docs/_static/logo.png` (512×512) | Docs header, README, app icon |
| Favicon | `src/favicon.ico` (16/32 px) | Browser tab, bookmarks |
| Social preview | `docs/_static/og-image.png` | Link unfurls (Slack, X, Discord) |
| Text-safe stand-in | 🚲 emoji | README/heading prefix where an image can't render (plain-text titles, chat) |

**Do:**
- Keep the bicycle on a plain white/transparent background.
- Keep proportions locked — scale the whole mark, never one axis.
- Leave clear space around it at least equal to the width of one wheel.
- Use the 🚲 emoji sparingly, as a one-time marker (a title, a section
  heading) — not as a bullet or decoration repeated through a page.

**Don't:**
- Recolor the bicycle into `--link` blue, gradients, or any color outside
  navy/slate-on-light.
- Add drop shadows, 3D bevels, motion blur, or a "speed lines" treatment —
  it's a bicycle, not a spaceship.
- Pair the logo with a different icon or mascot to represent Bialet.
- Stretch or crop the circle backdrop.

---

## Color Palette

Bialet uses a **minimal, high-contrast, dual-mode palette** built entirely
from 3-character hex values. The constraint is deliberate: fewer colors,
each with one job, expressed in the shortest valid CSS — the same
philosophy as the framework itself.

| Token | Light | Dark | Use |
|---|---|---|---|
| `--bg` | `#FFF` | `#024` | Page background |
| `--text` | `#024` | `#FFF` | Body text |
| `--link` | `#06F` | `#0BF` | Hyperlinks, primary CTAs |
| `--link-hover` | `#04F` | `#0FF` | Link/CTA hover, focus |
| `--code` | `#B00` | `#FA0` | Inline code, short snippets |

```css
:root {
  --bg: #FFF;
  --text: #024;
  --link: #06F;
  --link-hover: #04F;
  --code: #B00;
}

@media (prefers-color-scheme: dark) {
  :root {
    --bg: #024;
    --text: #FFF;
    --link: #0BF;
    --link-hover: #0FF;
    --code: #FA0;
  }
}
```

**Don't:**
- Add a third accent color "for variety." The palette is short on purpose.
- Use `--code` styling for anything that isn't literal code or a filename.
- Ship a color that fails WCAG AA (4.5:1 text, 3:1 large text/UI) in either
  mode — check both before shipping a new combination.
- Reach for a 6-digit hex when a 3-digit one gives the same color — the
  palette exists to be typed in full every time, not abbreviated as an
  afterthought.

---

## Typography

| Property | Value | Use |
|---|---|---|
| Font family | `system-ui` stack | Body text, UI — no web fonts, no build step, matches "zero dependencies" |
| Code font | Monospace stack (`ui-monospace, SFMono-Regular, Menlo, monospace`) | Code blocks, inline code, file paths |
| Body size | Browser default (~1rem), left-aligned | Docs prose, long-form content |
| Hero/landing heading | `clamp(1.75rem, 5vw, 2.5rem)`, center-aligned | Landing page H1 only |

**Pitfall:** the large, centered, generously-spaced heading style (big
size, `2em` line height, centered) belongs to hero sections and landing
pages — one headline, a short subhead, a CTA. Never apply it to body
copy or documentation prose; docs are left-aligned, normal size, and
read top-to-bottom like a manual, not a poster.

---

## Iconography

Bialet uses plain outline icons ([Octicons](https://primer.style/foundations/icons)) for
feature and audience cards — one icon per concept, no icon soup.

| Convention | Example |
|---|---|
| Feature cards ("what it does") | `package`, `database`, `zap`, `code` |
| Audience cards ("who it's for") | `beaker`, `mortar-board`, `heart` |
| Brand symbol | Bicycle only — never combine with the feature icon set |

Icons render in `--text` (neutral), never a dedicated icon color. The
palette has one job per token already — feature and audience cards are
told apart by their icon and copy, not by a second color layered on top.

**Do:** pick the one icon that most literally names the concept (a
database icon for "SQLite is built-in," not an abstract shape).

**Don't:**
- Invent new icon families, mix filled and outline styles on the same
  page, or use an icon as decoration where words alone are clear.
- Tint icons with a color outside the core palette (no "info blue,"
  "warning amber," or any other utility-class color) — that's a
  fourth and fifth accent color smuggled in through icons.
- Use a rocket, spaceship, or any launch/speed icon for a Bialet feature —
  that's the thing Bialet is defined *against*. If a feature needs an
  icon for "fast" or "zero build step," use `zap`, not `rocket`.

---

## Tone of Communication

Bialet's writing is **plain, technical, and no-nonsense** — short
declarative sentences, minimal hedging, second-person and code-first. It
is honest about limitations instead of marketing around them.

- **Show, don't sell.** Lead with a real, runnable code snippet, not a
  paragraph of adjectives.
- **Second person, instructional.** "Run `bialet dev`," not "Users can
  run bialet in development mode."
- **Confident, occasionally dry.** The spaceship/bicycle line is the
  ceiling for how playful Bialet gets — a single sharp contrast, not a
  running joke.
- **Say what it's not for.** Docs include "when not to use Bialet"
  sections; the brand voice does the same instead of implying it fits
  every use case.
- **Pitfall callouts, not fear-mongering.** Anticipate the mistake
  ("Request.post returns null when the key is missing") instead of vague
  warnings ("be careful with forms").

**Correct:**
> Bialet is a small, self-contained executable. Copy it to your server,
> and your app is deployed.

**Incorrect:**
> Unleash the power of next-generation, blazing-fast web development with
> our revolutionary, seamless full-stack solution! 🚀✨

The second example is wrong for three reasons: hype adjectives with no
content ("revolutionary," "unleash the power"), a rocket emoji competing
with the bicycle symbol, and no concrete claim a reader could verify.

---

## Correct / Incorrect Usage — Quick Reference

| | Correct | Incorrect |
|---|---|---|
| Name | "Bialet embeds Wren and SQLite." | "BiaLet is powered by wren.js." |
| Command | "Start the server with `bialet dev`." | "Run the Bialet server tool." |
| Tagline | Quoted verbatim, unaltered. | "Bialet: the rocket-powered bicycle of web dev!" |
| Logo | Bicycle, navy/slate, plain background. | Bicycle recolored blue with a drop shadow. |
| Accent color | `--link` blue for hyperlinks and CTAs. | A random new accent added "for variety." |
| Tone | "SQLite is built-in — no connection strings." | "Say goodbye to database headaches forever!" |
| Emoji | One 🚲 as a title marker. | 🚲🚀✨ stacked as decoration. |

---

## Examples

**Hero copy pattern** (headline + one-sentence subhead + code):

```
Web development became a spaceship. Bialet is a bicycle.

Build data-driven web apps from a single file. No NPM, no YAML,
no separate database servers.

`CREATE TABLE IF NOT EXISTS messages (text TEXT)`.query
```

**Feature card pattern** (icon + bold three-word title + one plain sentence):

```
[database icon]
SQLite is Built-in
Forget about provisioning databases or writing connection strings.
```

**Manifesto pattern** (numbered, one bold claim per line, no elaboration
beyond a single supporting clause):

```
1. Simplicity is a superpower. Every line of tooling you don't write
   is a line of your app that ships faster.
```
