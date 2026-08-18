# 2. Setup

## Install Bialet

The quickest way is the install script:

```bash
curl -fsSL https://get.bialet.dev | sh
```

For other options (Homebrew, Docker, building from source), see
[Installation](../installation.md).

On Windows, download the latest `bialet.exe` directly:
[bialet-{{release_tag}}-windows-x86_64.zip]({{release_windows_url}}).

## Create a project

Make a new directory for your poll app:

```bash
mkdir poll && cd poll
```

Copy the `vote.html` and `results.html` files you downloaded in the
[introduction](1-introduction) into this directory.

## Run the server

```bash
bialet dev
```

`bialet dev` starts the server from the current directory, enables live reload
and the in-browser error display, and opens
[127.0.0.1:7001](http://127.0.0.1:7001) in your browser. It only needs to be
run once — the flags are stored in the database and persist across restarts.

If you don't need those development conveniences, plain `bialet` starts the
server the same way without touching the flags or opening the browser.

On Windows, double-clicking `bialet.exe` in the project folder starts it in dev
mode, just like `bialet dev`.

For now you'll see a directory listing — we haven't created any `.wren` files
yet.

## Restarting

On Linux, Bialet watches `.wren` files and reloads automatically. On macOS,
restart the server manually after changes (`Ctrl+C` then `bialet` again).

---
**Previous:** [1. Introduction](1-introduction) &nbsp; | &nbsp; **Next:** [3. Your First Page](3-first-page)
