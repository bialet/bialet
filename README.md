# 🚲 bialet

Build lightweight dynamic web apps effortlessly with [Wren](https://wren.io/) and integrated database.

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

Install SQLite3 dev dependencies and run the build.

```bash
apt install libsqlite3-dev
make clean
make
```
