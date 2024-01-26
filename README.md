# 🚲 bialet

Build lightweight dynamic web apps effortlessly with a custom flavor of [Wren](https://wren.io/)
and an integrated SQLite database.

See [examples](examples/run.md) for usage.

## Running

```bash
bialet -p 7000 /path/to/root
```

Available Configuration

| Description | Argument | Default value |
| --- | --- | --- |
| Port | -p | 7000 |
| Host | -h | localhost |
| Log file | -l | stdout |
| Memory limit | -m | 50 (in Megabytes) |
| Hard memory limit | -M | 100 (in Megabytes) |
| CPU limit | -c | 15 |
| Hard CPU limit | -C | 30 |

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
