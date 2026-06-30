# Getting Started

In this tutorial we'll build a working poll application with Bialet. You'll
learn how to write Wren files, query a SQLite database, handle form
submissions, and render dynamic HTML — all with zero configuration.

## What we'll build

A simple poll that lets users vote "Yes" or "No" and see live results with
percentage bars.

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

Already familiar with Bialet? Jump to [Installation](../installation.md) or the
[API Reference](../reference.md).

## Source files

The complete source for each step is available:

- [Starting HTML](1-html/vote.html) and [results](1-html/results.html)
- [Templates step](2-no-logic/_app.wren) (Wren with static layout only)
- [Final app files](3-final/_app.wren) (complete working poll)
