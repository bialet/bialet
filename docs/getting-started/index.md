# Getting Started

In this tutorial we'll build a working poll application with Bialet. You'll
learn how to write [Wren](../wren.md) files, query a SQLite database, handle
form submissions, and render dynamic HTML — all with zero configuration.

> ℹ️ **Bialet has no router to configure.** Every `.wren` file is a URL at the
> same path, just like a static HTML file: `about.wren` → `/about`.
>
> Dynamic data lives in the query string or the path — both are equal:
> `article.wren` → `/article?id=123` with `var id = Request.get("id")`, or
> `/article/my-post` with `var slug = Request.route(0)`. Keep routes
> shallow; Bialet discourages complex routing. See
> [Advanced Routing](../advanced-routing.md) for the full routing model.

Already familiar with Bialet? Jump to [Installation](../installation.md) or the
[API Reference](../reference.md).

Building with an AI assistant instead? Paste the
[AI Coding Prompt](../ai-prompt.md) into Claude, ChatGPT, or Cursor before you
start — it teaches the assistant Bialet's conventions in one message.

## What we'll build

A simple poll that lets users vote "Yes" or "No" and see live results with
percentage bars. [Check it live](https://poll.bialet.dev/).

- **Vote page** — Form with radio buttons, styled with Tailwind
- **Results page** — Progress bars showing vote distribution
- **Database** — SQLite table for poll options and vote counts

![Poll Vote](../_static/poll-vote.png)
![Poll Results](../_static/poll-results.png)

## Chapters

```{toctree}
:maxdepth: 1

1-introduction
2-setup
3-first-page
4-templates
5-logic-database
6-migrations
7-forms-voting
8-results
```

## Source files

The complete source for each step is available:

- [Starting HTML](1-html/vote.html) and [results](1-html/results.html)
- [Templates step](2-no-logic/_app.wren) (Wren with static layout only)
- [Final app files](3-final/_app.wren) (complete working poll)
