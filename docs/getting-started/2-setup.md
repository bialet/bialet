# 2. Setup

## Install Bialet

The quickest way is the install script:

```bash
curl -fsSL https://get.bialet.dev | sh
```

For other options (Homebrew, Docker, building from source), see
[Installation](../installation.md).

## Create a project

Make a new directory for your poll app:

```bash
mkdir poll && cd poll
```

Copy the `vote.html` and `results.html` files you downloaded in the
[introduction](1-introduction) into this directory.

## Run the server

```bash
bialet
```

Bialet starts on port 7001. Open [127.0.0.1:7001](http://127.0.0.1:7001) in
your browser. For now you'll see a directory listing — we haven't created
any `.wren` files yet.

## Using Docker

If you prefer containers, clone the
[Bialet Skeleton](https://github.com/bialet/skeleton) and use Docker Compose:

```bash
git clone --depth 1 https://github.com/bialet/skeleton.git poll
cd poll
BIALET_DIR=/path/to/your/files docker compose up
```

## Restarting

On Linux, Bialet watches `.wren` files and reloads automatically. On macOS,
restart the server manually after changes (`Ctrl+C` then `bialet` again).

---
**Previous:** [1. Introduction](1-introduction) &nbsp; | &nbsp; **Next:** [3. Your First Page](3-first-page)
