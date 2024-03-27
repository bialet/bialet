# Installation

This guide provides instructions on how to set up the Bialet project using Docker Compose, as well as details on building the project from source and using the Bialet CLI.

## Quick Start with Docker Compose

The quickest way to get Bialet up and running is by using Docker Compose.

1. First, clone the Bialet repository:

    ```bash
    git clone https://github.com/bialet/bialet.git
    ```

2. Navigate to the cloned directory and start the application:

    ```bash
    cd bialet
    docker compose up
    ```

## Configuring the Docker Compose Setup

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

## Building from Source

To build Bialet from source, you'll need to install certain dependencies and run the build process.

### Dependencies

- **SQLite3**
- **OpenSSL 3**
- **curl**

#### Debian/Ubuntu

```bash
sudo apt install -y libsqlite3-dev libssl-dev libcurl4-openssl-dev
```

#### MacOS

```bash
brew install sqlite3 openssl curl
```

### Building the Project

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
