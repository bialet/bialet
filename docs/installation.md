# Installation and Usage

## With Brew

The easiest way to install Bialet is to use [Homebrew](https://brew.sh/):

```bash
brew install bialet/bialet/bialet
```

## With Docker Compose

Use the [Bialet Skeleton](https://github.com/bialet/skeleton) repository or the [framework repository](https://github.com/bialet/bialet)
to start [Docker Compose](https://docs.docker.com/compose/) the application.

```bash
git clone --depth 1 https://github.com/bialet/skeleton.git mywebapp
cd mywebapp
docker compose up
```

### Customizing the Application Directory

To specify a custom directory for the Bialet project, set the `BIALET_DIR` environment variable:

```bash
BIALET_DIR=/path/to/app docker compose up
```

### Changing the Default Port

The default application port is `7001`. To use a different port, set the `BIALET_PORT` environment variable:

```bash
BIALET_PORT=7001 docker compose up
```

## Building from Source

To build Bialet from source, you'll need to install certain dependencies and run the build process.

### Dependencies

#### Debian/Ubuntu

```bash
sudo apt install -y libsqlite3-dev libssl-dev 
# Optional, but recommended for production
sudo apt install -y libcurl4-openssl-dev
```

#### MacOS

```bash
brew install sqlite3 curl
# Optional, but recommended for production
brew install openssl
```

### Windows

* libcrypto-3-x64.dll
* libsqlite3-0.dll
* libssl-3-x64.dll (Optional, but recommended for production)

### Building the Project

After installing the dependencies, compile the project:

```bash
make clean && make
```

To install the built application, run:

```bash
make install
```

## Syntax Highlighting

You have available the [plugin for Vim/Neovim](https://github.com/bialet/bialet.vim).

```bash
Plug 'bialet/bialet.vim'
```

We are still working on supporting VSCode and other IDEs.

## Using the Bialet CLI

The Bialet CLI allows you to interact with the application directly from the command line.

### Basic Usage

To start the application, simply run:

```bash
bialet
```

By default, the application runs in the current directory.

### Customizing Startup Options

To change the directory where the application runs or adjust other settings, you can use various command-line arguments:

```bash
bialet -p 7001 /path/to/app
```

### CLI Parameters

The table below summarizes the available command-line parameters for the Bialet CLI:

| Parameter | Description | Default Value |
| --- | --- | --- |
| `-p` | Port number | `7001` |
| `-h` | Host name | `127.0.0.1` |
| `-r` | Run the code passed as argument | None |
| `-l` | Log file location | `stdout` |
| `-d` | SQLite database file location | `_db.sqlite` |
| `-w` | Enable SQLite [Write-Ahead logging mode](https://www.sqlite.org/wal.html) | Disabled |
| `-i` | Ignored files, comma separated list of glob expressions | README\*, LICENSE\* , \*.json, \*.yml, \*.yaml |
| `-m` | Memory limit (MB) | `50` |
| `-M` | Hard memory limit (MB) | `100` |
| `-c` | CPU limit (%) | `15` |
| `-C` | Hard CPU limit (%) | `30` |

## Run code from the Command Line

To run code from the command line, use the `-r` command:

```bash
bialet -r 'System.print("Hello, World!")'
```

The response will be printed directly.

```bash
bialet -r 'import "bialet" for Response
Response.out("No log, plain response")'
```

You have to respect new lines in the code.
