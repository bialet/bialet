# Installation

For how to run and configure the `bialet` binary, see
[Command Line Usage](usage.md).

## With the Install Script

The easiest way to install Bialet on supported platforms (macOS ARM, Ubuntu
x86_64, Ubuntu ARM) is with the install script:

```bash
curl -fsSL https://get.bialet.dev | sh
```

## For Windows

Download the latest `bialet.exe` (inside the zip) directly:

- **[bialet-{{release_tag}}-windows-x86_64.zip]({{release_windows_url}})**

Extract and run — no installation required:

```bash
bialet.exe
```

Double-clicking `bialet.exe` in File Explorer starts it in dev mode (live
reload, in-browser error display, and the browser opens automatically). Run it
from a terminal to start without dev mode.

## Releases

Pre-built binaries for all supported platforms are available on the [releases
page](https://github.com/bialet/bialet/releases):

| Platform        | Download                                                                       |
| --------------- | ------------------------------------------------------------------------------ |
| macOS ARM64     | [bialet-{{release_tag}}-macos-arm64.tar.gz]({{release_macos_arm64_url}})       |
| Linux x86\_64   | [bialet-{{release_tag}}-linux-x86_64.tar.gz]({{release_linux_x64_url}})        |
| Linux ARM64     | [bialet-{{release_tag}}-linux-arm64.tar.gz]({{release_linux_arm64_url}})       |
| Windows x86\_64 | [bialet-{{release_tag}}-windows-x86_64.zip]({{release_windows_url}})           |

Download the archive for your platform, extract it, and place the binary
(`bialet` on macOS/Linux, `bialet.exe` on Windows) in your `PATH`:

```bash
# macOS / Linux
tar xzf bialet-{{release_tag}}-linux-x86_64.tar.gz
sudo mv bialet /usr/local/bin/
```

## With Brew

Use [Homebrew](https://brew.sh/) to install Bialet on macOS or Linux:

```bash
brew install bialet/bialet/bialet
```

## With Docker Compose

Use the [Bialet Skeleton](https://github.com/bialet/skeleton) repository or the
[framework repository](https://github.com/bialet/bialet) to start
[Docker Compose](https://docs.docker.com/compose/) the application.

```bash
git clone --depth 1 https://github.com/bialet/skeleton.git mywebapp
cd mywebapp
docker compose up
```

### Customizing the Application Directory

To specify a custom directory for the Bialet project, set the `BIALET_DIR`
environment variable:

```bash
BIALET_DIR=/path/to/app docker compose up
```

### Changing the Default Port

The default application port is `7001`. To use a different port, set the
`BIALET_PORT` environment variable:

```bash
BIALET_PORT=8080 docker compose up
```

## Building from Source

```bash
git clone https://github.com/bialet/bialet.git
cd bialet
make
make install
```

For per-OS dependencies, cross-compiling (including Windows), the full list
of `make` targets, and the development workflow, see
[CONTRIBUTING.md](https://github.com/bialet/bialet/blob/main/CONTRIBUTING.md)
on GitHub.

## Editor Support

For syntax highlighting in VS Code, Vim, and Neovim, see
[Editor Support](editor-support.md).

For the full command-line reference — starting the server, `bialet dev`, every
flag, `-r`, `-t`, and `-T` — see [Command Line Usage](usage.md).
