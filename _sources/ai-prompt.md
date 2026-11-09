# AI Coding Prompt

Building a Bialet project with Claude, ChatGPT, Cursor, or any other AI
coding tool? Copy the prompt below and paste it in before you describe what
you want to build. It teaches the assistant Bialet's conventions — routing,
templates, the database API, and project structure — so it writes idiomatic
`.wren` code from the first response instead of guessing.

```{admonition} How to use it
:class: tip

1. Copy the whole block below.
2. Paste it as the first message in a new chat (or as a system/project prompt).
3. Follow it with what you want to build, e.g. "Build a guestbook app with a
   name and message field."
```

````{admonition} Using a coding agent?
:class: tip

If your agent loads skills (Claude Code, Codex, Cursor, OpenCode), install the
companion skill instead of pasting the prompt:

```bash
npx skills add bialet/skills --skill bialet-app-builder
```

It teaches the same conventions in four gated phases — static design first,
your confirmation, then the database and the logic. Use it *or* the prompt
below, not both.
````

---

```{literalinclude} prompt-guide.md
:language: markdown
```
