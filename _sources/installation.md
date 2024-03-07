# Installation

The easiest way to use it is with [Docker Compose](https://docs.docker.com/compose/).
Clone the repository and run:

```bash
BIALET_DIR=/path/to/app docker compose up
```

The default port is 7000. Use the environment variable **BIALET_PORT** to change the port value.

```bash
BIALET_PORT=7001 docker compose up
```

## Building

Install SQLite3, OpenSSL 3 and curl for dev dependencies and run the build.

In Debian based

```bash
sudo apt install -y libsqlite3-dev libssl-dev libcurl4-openssl-dev
```

In MacOS

```bash
brew install sqlite3 openssl curl
```

Then run the build

```bash
make clean && make
```
