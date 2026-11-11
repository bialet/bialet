# Performance

Bialet is a single worker serving one request per connection. That design makes
performance easy to reason about and easy to measure: there is no thread pool,
no connection reuse, and no cache between requests. This page publishes what
the benchmark harness measures, on named hardware, so you can decide whether
one worker is enough for your app.

These are capacity-planning numbers, not a leaderboard. Bialet is not trying to
beat Go or nginx on throughput; it is trying to be fast enough for an internal
tool on a small server, and to say so with measurements instead of adjectives.

## What was measured

[`scripts/perf_bench.py`](https://github.com/bialet/bialet/blob/main/scripts/perf_bench.py)
boots a throwaway app and measures both latency (one request at a time) and
throughput (four clients in parallel, which is more than one worker can serve —
the extra connections queue in the kernel).

| Endpoint  | What the request does                                             |
| --------- | ----------------------------------------------------------------- |
| `static`  | A 3 KB static HTML file, no interpreter                           |
| `hello`   | A `.wren` handler that returns `"ok"` (VM creation and compile)   |
| `markdown`| Render a 60-section Markdown document                              |
| `json`    | Parse and stringify a ~20 KB JSON document                         |
| `rows`    | `SELECT *` over 2,000 rows                                         |
| `render`  | An HTML template interpolating a 500-element list                  |
| `escape`  | HTML-escape a 200-line document                                    |
| `post`    | A form POST with an 8 KB body                                      |
| `upload`  | A 64 KB multipart file upload                                      |

Each request opens a new TCP connection and the server closes it after the
response, exactly as a client sees it in production.

## Test setup

|                  | macOS run                          | Linux run                          |
| ---------------- | ---------------------------------- | ---------------------------------- |
| Machine          | MacBook Air, Apple M3, 16 GB       | Same machine, Ubuntu 24.04 (arm64) |
| OS               | macOS 15.8                         | Docker Desktop 29.9.2 VM           |
| Build            | `make` (`-O2`, clang), arm64       | `make` (`-O2`, gcc), arm64         |
| Requests         | 100 per endpoint, 4 concurrent      | 100 per endpoint, 4 concurrent     |
| Rounds           | 5, best median                     | 3, best median                     |

Both runs used the same source tree. The Linux run is a container on the same
laptop, not a VPS: expect a small machine to be slower and a modern VPS to be
in the same range.

## Results: macOS arm64

| Endpoint   | Median  | p95     | Requests/s (4 clients) |
| ---------- | ------- | ------- | ---------------------- |
| `static`   | 0.27 ms | 0.36 ms | 7,109                  |
| `hello`    | 3.99 ms | 4.58 ms | 266                    |
| `markdown` | 4.15 ms | 4.62 ms | 254                    |
| `json`     | 5.19 ms | 5.97 ms | 199                    |
| `rows`     | 7.56 ms | 8.88 ms | 137                    |
| `render`   | 6.83 ms | 8.39 ms | 150                    |
| `escape`   | 6.22 ms | 7.17 ms | 164                    |
| `post`     | 4.33 ms | 5.03 ms | 237                    |
| `upload`   | 4.06 ms | 4.64 ms | 246                    |

## Results: Linux arm64 (Ubuntu 24.04 container)

| Endpoint   | Median  | p95     | Requests/s (4 clients) |
| ---------- | ------- | ------- | ---------------------- |
| `static`   | 0.14 ms | 0.16 ms | 4,588                  |
| `hello`    | 2.83 ms | 3.13 ms | 350                    |
| `markdown` | 2.91 ms | 3.24 ms | 340                    |
| `json`     | 3.84 ms | 5.44 ms | 286                    |
| `rows`     | 5.40 ms | 6.76 ms | 187                    |
| `render`   | 4.85 ms | 5.00 ms | 209                    |
| `escape`   | 4.71 ms | 5.08 ms | 205                    |
| `post`     | 3.37 ms | 8.30 ms | 294                    |
| `upload`   | 3.03 ms | 3.47 ms | 346                    |

## Startup and memory

Measured on the same builds, five runs each, with an empty app that returns
`"ok"`:

| Metric                        | macOS      | Linux      |
| ----------------------------- | ---------- | ---------- |
| Start to first response       | ~27 ms     | ~26 ms     |
| Peak RSS after first request  | ~6.4 MB    | ~13 MB     |

The Linux build links OpenSSL and libcurl dynamically and its C library keeps
more per-process state, which is most of the difference. Both are small enough
that memory is not the constraint; the single worker is.

## How to read these numbers

- **A dynamic page costs 3–7 ms.** Most of that is not your SQL: every request
  creates a fresh Wren VM and compiles the handler script. That is the price of
  request isolation, and it is why an opcode cache is on the roadmap.
- **Throughput is roughly `1000 / median`** once the queue is saturated, and
  it scales with workers. Four workers on a four-core box serve about four
  times the requests per second, sharing one SQLite file.
- **Latency is queueing.** At 4 clients, the fourth request waits for the
  three in front of it. That is why p95 grows when clients are added: the
  server is honest about being sequential.
- **Static files are the fast path.** If a page can be prerendered, let the
  reverse proxy serve or cache it.

Two limits are documented in [Deployment](deployment.md) and show up in
practice: the listen backlog is 10, so a burst larger than that can be refused
before the worker accepts it, and there is no keep-alive, so every request pays
a TCP handshake. A reverse proxy absorbs both.

## Reproduce it

```bash
make
python3 scripts/perf_bench.py --rounds 5 --requests 100 --concurrency 4
```

Add `--json out.json` to keep the raw timings, and `--baseline out.json` to
compare two builds (for example, before and after a change to the request
path). The machine will not match this one; the ratios will.
