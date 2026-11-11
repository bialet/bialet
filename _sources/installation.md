# Installation

For how to run and configure the `bialet` binary, see
[Command Line Usage](usage.md).

## Install Script

The easiest way to install Bialet on supported platforms (macOS ARM, Ubuntu
x86_64, Ubuntu ARM) is with the install script:

```bash
curl -fsSL https://get.bialet.dev | sh
```

## Docker

Run the published [`bialet/bialet`](https://hub.docker.com/r/bialet/bialet)
image directly with [Docker](https://docs.docker.com/):

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

## Debian and Ubuntu

Bialet is available as a Debian package from the
[`ppa:bialet/bialet`](https://launchpad.net/~bialet/+archive/ubuntu/bialet)
Personal Package Archive. It is built for every Ubuntu release still in standard
support, on `amd64` and `arm64`:

```bash
sudo add-apt-repository ppa:bialet/bialet
sudo apt update
sudo apt install bialet
```

Upgrades arrive with the rest of your system packages:

```bash
sudo apt update && sudo apt upgrade
```

The package installs the binary as `/usr/bin/bialet` and links against the
distribution's SQLite, libcurl, and OpenSSL. It is not installed as a service:
run `bialet` from your app directory, or point a reverse proxy at it as
described in [Deployment](deployment.md).

> If you previously installed Bialet with the install script, remove
> `~/.local/bin/bialet` first. That path usually comes before `/usr/bin` in
> `PATH`, so the old binary would keep running after the package is installed.

## Brew

Use [Homebrew](https://brew.sh/) to install Bialet on macOS or Linux:

```bash
brew install bialet/bialet/bialet
```

## Windows

Download the latest `bialet.exe` (inside the zip) directly:

- **[bialet-{{release_tag}}-windows-x86_64.zip]({{release_windows_url}})**

Extract it, open PowerShell or Windows Terminal in that folder, and run:

```powershell
.\bialet.exe
```

Double-clicking `bialet.exe` in File Explorer also works: it starts in dev mode
(live reload, in-browser error display, and the browser opens automatically).
The terminal is the better default — it shows the logs and leaves dev mode off.

## Releases

Pre-built binaries for all supported platforms are available on the
[releases page](https://github.com/bialet/bialet/releases):

| Platform       | Download                                                                 |
| -------------- | ------------------------------------------------------------------------ |
| macOS ARM64    | [bialet-{{release_tag}}-macos-arm64.tar.gz]({{release_macos_arm64_url}}) |
| Linux x86_64   | [bialet-{{release_tag}}-linux-x86_64.tar.gz]({{release_linux_x64_url}})  |
| Linux ARM64    | [bialet-{{release_tag}}-linux-arm64.tar.gz]({{release_linux_arm64_url}}) |
| Windows x86_64 | [bialet-{{release_tag}}-windows-x86_64.zip]({{release_windows_url}})     |

Download the archive for your platform, extract it, and place the binary
(`bialet` on macOS/Linux, `bialet.exe` on Windows) in your `PATH`:

```bash
# macOS / Linux
tar xzf bialet-{{release_tag}}-linux-x86_64.tar.gz
sudo mv bialet /usr/local/bin/
```

Each release publishes a `SHA256SUMS` file next to the archives. To check a
download by hand:

```bash
curl -fsSLO https://github.com/bialet/bialet/releases/download/{{release_tag}}/SHA256SUMS
sha256sum --check --ignore-missing SHA256SUMS   # shasum -a 256 -c on macOS
```

The install script does this automatically: it refuses an archive whose checksum
does not match, and warns when a release predates `SHA256SUMS`.

## Building from Source

```bash
git clone https://github.com/bialet/bialet.git
cd bialet
make
make install
```

For per-OS dependencies, cross-compiling (including Windows), the full list of
`make` targets, and the development workflow, see
[CONTRIBUTING.md](https://github.com/bialet/bialet/blob/main/CONTRIBUTING.md) on
GitHub.
