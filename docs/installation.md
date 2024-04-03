# Installation

## With Docker Compose

Use the [Bialet Skeleton](https://github.com/bialet/skeleton) repository or the [framework repository](https://github.com/bialet/bialet)
to start [Docker Compose](https://docs.docker.com/compose/) the application.

### Customizing the Application Directory

To specify a custom directory for the Bialet project, set the `BIALET_DIR` environment variable:

```bash
BIALET_DIR=/path/to/app docker compose up
```

### Changing the Default Port

The default application port is `7000`. To use a different port, set the `BIALET_PORT` environment variable:

```bash
BIALET_PORT=7001 docker compose up
```

## With the Desktop Application

Currently, the Bialet Desktop is only available for Ubuntu/Debian.

Download the [Bialet Desktop](https://github.com/bialet/bialet/releases/download/v0.4/bialet-desktop_0.1.0_amd64.deb) application.

![Bialet Desktop in Ubuntu](_static/bialet-desktop.png)

## Building from Source

To build Bialet from source, you'll need to install certain dependencies and run the build process.

### Dependencies

#### Debian/Ubuntu

```bash
sudo apt install -y libsqlite3-dev libssl-dev libcurl4-openssl-dev
```

#### MacOS

```bash
brew install sqlite3 openssl curl
```
### Windows

* libcrypto-3-x64.dll
* libsqlite3-0.dll
* libssl-3-x64.dll

## Building the Project

After installing the dependencies, compile the project:

```bash
make clean && make
```

To install the built application, run:

```bash
make install
```

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
bialet -p 7000 /path/to/app
```

### CLI Parameters

The table below summarizes the available command-line parameters for the Bialet CLI:

| Parameter | Description | Default Value |
| --- | --- | --- |
| `-p` | Port number | `7000` |
| `-h` | Host name | `localhost` |
| `-r` | Run the code passed as argument | None |
| `-l` | Log file location | `stdout` |
| `-d` | SQLite database file location | `_db.sqlite` |
| `-i` | Ignored files, comma separated list of glob expressions | README*,LICENSE*,package.* |
| `-m` | Memory limit (MB) | `50` |
| `-M` | Hard memory limit (MB) | `100` |
| `-c` | CPU limit (%) | `15` |
| `-C` | Hard CPU limit (%) | `30` |
