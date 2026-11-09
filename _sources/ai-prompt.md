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

<div class="ai-prompt-actions">
  <button type="button" class="sd-sphinx-override sd-btn sd-text-wrap sd-btn-outline-secondary" data-copy-prompt>
    <span data-copy-label>Copy Prompt</span>
  </button>
</div>

<div class="copy-status" role="status" aria-live="polite"></div>

<p>Or feed the whole site to your tool:
<a href="llms.txt">llms.txt</a> (index) and
<a href="llms-full.txt">llms-full.txt</a> (every page, plain text).</p>

---

```{literalinclude} prompt-guide.md
:language: markdown
```
