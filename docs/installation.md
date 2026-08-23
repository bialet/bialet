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

## With Docker

Run the published [`bialet/bialet`](https://hub.docker.com/r/bialet/bialet) image
directly with [Docker](https://docs.docker.com/):

```bash
docker run -t -p 7001:7001 -v "$PWD":/var/www bialet/bialet
```

The `-t` flag allocates a pseudo-TTY so the server's logs are colored. Drop it
if you want plain, uncolored output.

The image serves the current directory, which is mounted at `/var/www`.

### Customizing the Application Directory

To serve a different directory, mount it at `/var/www`:

```bash
docker run -t -p 7001:7001 -v /path/to/app:/var/www bialet/bialet
```

### Changing the Default Port

The application listens on port `7001` inside the container. To expose a
different host port, use `-p`:

```bash
docker run -t -p 8080:7001 -v "$PWD":/var/www bialet/bialet
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
