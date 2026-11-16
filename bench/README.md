# Bialet performance harness

A reproducible harness for one question: on a small Linux VPS, where do the
CPU cycles, allocations and syscalls actually go, and which small code changes
move them?

It generates a synthetic Bialet app, runs it under a real HTTP load, records
latency/throughput and RSS, then profiles the server with `perf`, `heaptrack`
and `strace`. `report.sh` turns the raw files into `report.md` and a Pareto
table in `pareto.md`.

This is not a leaderboard. It is a measurement loop for A/B changes on the same
box, with the same flags, in the same session.

## macOS cannot run this stack

`perf`, `heaptrack`, `strace` and `/proc/<pid>/status` are Linux. The generator
scripts are portable enough to syntax-check an app on macOS, but `run.sh`
refuses to start outside Linux. Use a VPS, a Linux VM or WSL2.

## Reference machines

| Machine | Shape | Role |
| --- | --- | --- |
| IONOS VPS S+ | 1 vCPU, 2 GB RAM, NVMe | the smallest target; client must be separate |
| Hetzner CX23 | 2 vCPU, 4 GB RAM, NVMe | server on one core, client on the other |

Both are shared-vCPU VMs. Absolute numbers move between runs and between
hosts. Ratios between two builds on the same VM are what this harness is for.

## Install

```bash
sudo bench/install-tools.sh
```

It is idempotent and installs build-essential, SQLite/curl/OpenSSL headers,
`perf` (trying `linux-tools-$(uname -r)`, then `linux-tools-generic`, then
`linux-perf`), heaptrack, valgrind, strace, ltrace, hyperfine, gdb, sysstat,
`oha` (or `wrk`), cpupower and the FlameGraph scripts. It never changes kernel
settings. It prints the commands for `perf_event_paranoid` and the CPU
governor instead.

Required for the load itself: oha, wrk or python3 (in that preference order).
`install-tools.sh` installs oha from the GitHub release when the distro has no
`wrk`.

## First baseline run

Run everything from the repository root. `run.sh` needs an explicit mode; it
will not start a load run by accident.

```bash
# 1. Install the toolchain, then build (run.sh also builds; this catches
#    missing headers before the first load run).
sudo bench/install-tools.sh
make

# 2. Make perf usable by your user, and pin the governor if the VM exposes it.
sudo sysctl -w kernel.perf_event_paranoid=1
sudo cpupower frequency-set -g performance      # skip if cpufreq is not exposed

# 3. Clean numbers: 5 repeats, 10 s each, 4 connections, one site.
#    This builds, generates bench/work/apps/site-1, starts it with -x -w,
#    warms up, measures every workload and samples RSS.
bench/run.sh baseline --sites 1 --repeats 5 --duration 10 --connections 4

# 4. Profile the mixed workload, then allocation and syscall shapes.
bench/run.sh profile  --workload mixed
bench/run.sh alloc
bench/run.sh syscalls

# 5. Report. The newest directory under bench/results/ is used by default.
bench/report.sh
```

`run.sh` writes `bench/results/<UTC timestamp>/` and prints the path. Each
directory is self-contained: `run.env`, `meta/`, `load/`, `perf/`, `profile/`,
`alloc/`, `syscalls/`, `mem/`, `logs/` and the two reports.

To see the plan without building, starting servers or sending a request:

```bash
DRY_RUN=1 bench/run.sh all --sites 2
```

## Why the client should be on another machine

Bialet is single-worker: the supervisor forks one HTTP child that accepts one
request at a time, and there is no keep-alive. The load client has to open a
TCP connection, write a request and read a response for every iteration. On a
1 vCPU box the client and server compete for the same core, so wall-clock
latency and requests/s measure the contention, not the server.

The rule:

| Measurement | Same box as server | Separate client |
| --- | --- | --- |
| Latency, requests/s, p90 | indicative only | valid |
| A/B ratio between two builds | valid, if the harness is unchanged | valid |
| `perf stat` instruction/cycle counts | valid | valid |
| Pareto shape (self time, syscalls) | valid | valid |

Run the client elsewhere when you care about absolute numbers:

```bash
# On the server. The harness starts the servers and profiles locally.
BENCH_HOST=bench-client \
BENCH_TARGET_HOST=192.0.2.10 \
bench/run.sh baseline --sites 1

# bench-client needs oha (preferred) or wrk for the load, curl for the
# one-request preflight, and passwordless ssh access.
# BENCH_TARGET_HOST is the server address as seen from that machine. If it is
# omitted, run.sh tries `hostname -I` and warns.
```

When `BENCH_HOST` is set the server binds `0.0.0.0` and the client is reached
over SSH. `perf`, `heaptrack`, `strace` and the RSS sampler still run on the
server.

## Modes

| Mode | What it does | Use it for |
| --- | --- | --- |
| `baseline` | build, generate, start, warm up, N repeats of every workload | latency/throughput table, RSS |
| `measure` | baseline + one `perf stat` window per workload | cycles, instructions, branch and cache counters |
| `profile` | `perf record -g --call-graph dwarf` around the selected workload | CPU Pareto, flamegraph |
| `alloc` | `heaptrack` attach around the selected workload | allocation-site Pareto |
| `syscalls` | `strace -c -f` attach around the selected workload | syscall Pareto |
| `all` | baseline + measure + profile + alloc + syscalls | the first deep run |

Defaults: `--sites 1`, `--duration 10`, `--connections 4`, `--repeats 5`,
`--warmup 2`, `--cooldown 2`, server flags `-x -w`.

### Server flags

`--cache on|off` controls `-x` (cache compiled user `.wren` files in memory;
the framework module is always cached). `--wal on|off` controls `-w` (SQLite
WAL). `--flags "-x -w"` overrides both and passes the string verbatim.

The harness records the exact flags in `run.env`, so two results directories
are comparable only when that line matches.

### Workloads

Each file under `bench/workloads/` is a small key/value definition with a path,
method and expected status. `preflight` sends one real request per URL before
any load run and aborts if the status differs.

| Workload | Request | What it isolates |
| --- | --- | --- |
| `static-small` | `GET /assets/site.css` | static fast path |
| `static-large` | `GET /assets/blob.bin` (1 MiB) | read/write path for a large body |
| `html-cpu` | `GET /render` | 600 interpolated/escaped rows, no DB |
| `db-read` | `GET /item?id=5000` | prepared SELECT + one-row render |
| `db-write` | `POST /write` | SQLite write (fixed 1000 slots, no unbounded growth) |
| `json` | `GET /api/items` | SELECT 20 rows + JSON serialisation |
| `session` | `GET /session` | session read/write + `Set-Cookie` |
| `mixed` | static + dynamic + JSON + session + generated routes | profiling, because one run covers the request mix |

`--workload NAME` (repeatable) restricts the measurement. `--profile-workload
NAME` chooses what `profile`, `alloc` and `syscalls` attach to; it defaults to
`mixed`.

## Multi-site

The harness can run N apps, one server process each, on consecutive ports.
`run.sh --sites N` generates N apps and drives every workload against all of
them in one load run (oha cycles URLs; wrk and the Python fallback round-robin
too). The RSS sampler sums all HTTP children.

```bash
bench/run.sh baseline --sites 8 --routes 120 --duration 10 --repeats 5
```

The standalone pair is useful when you want to poke at servers by hand:

```bash
bench/gen-sites.sh --count 8
bench/start-sites.sh --count 8 --flags "-x -w"
# manifest: bench/work/run/sites.tsv (supervisor and HTTP child pids)
# logs:     bench/work/logs/site-N.log
# RSS CSV:  bench/results/<UTC>/mem/sites-rss.csv
bench/stop-sites.sh
```

`start-sites.sh` samples all HTTP children at 5 Hz by default and writes a CSV
under `bench/results/`. `stop-sites.sh` stops the sampler first, then each
supervisor (which forwards SIGTERM to its child), then anything left.

Multi-site load needs oha (it accepts several URLs) or the Python fallback.
wrk can only target one origin, so the harness refuses a multi-origin wrk run
rather than silently measuring site-1 only.

With `--sites N` in `run.sh`, `perf stat` covers every HTTP child (comma-
separated `-p`), but `profile`, `alloc` and `syscalls` attach to the first site
only. Profile one process at a time; summing DWARF samples across eight
servers is not what the Pareto table means.

## Reading the Pareto tables

`profile.sh` runs `perf report` twice:

- **flat / self**: `--no-children`. The percentage is time spent in that
  function itself, not in its callees. This is the table to use for the 80/20
  set.
- **cumulative / children**: `--children`. The percentage includes everything
  called below it. A 90% `main` means nothing; a 40% `wrenNewString` means
  string allocation is the story.

`pareto.md` sorts self time, accumulates it and lists the smallest prefix that
reaches 80% of the samples. It also groups the same numbers by source file when
`perf report --srcline` produced paths. The build uses `-g`, so DWARF is there;
if your distro's perf lacks `--srcline`, the file column says
`(no debug info)` and the function names still work.

Sampling has a noise floor. On a 10 s run at 999 Hz, a function at 0.2% has
about 20 samples. Treat the top of the table and the 80% set as signal; do not
chase 0.1% entries.

`heaptrack_print` sites are allocation stack leaves. Peak memory is reported
separately. `strace -c` time percentages include the tracer's overhead, so use
the syscall mix and the calls/request ratio, not absolute latency.

## Noise rules

1. **5 repeats, median and p90.** The report gives the median and the p90
   across repeats. A change is interesting when the median improves and the
   worst p90 does not regress beyond the run-to-run spread.
2. **Warm up.** The first requests compile and cache route modules and open the
   database. `run.sh` warms each workload for `--warmup` seconds (default 2)
   and does not count it.
3. **Pin CPUs on a 2 vCPU box.** Run the server on one core and the client on
   the other (or use `BENCH_HOST`). `taskset` is in util-linux. If you run the
   client locally, pinning both to separate cores is the next best thing.
4. **Governor.** `performance` when the VM exposes cpufreq. If it does not,
   record that in the report by leaving `meta/environment.txt` intact.
5. **`perf_event_paranoid`.** `1` is enough for user-space `perf stat/record`.
   If callchains are empty, try `0`. On a shared box, prefer running the
   profiling modes with the permissions you already have rather than lowering
   it globally.
6. **Concurrency.** The listen backlog is 10 and Bialet has no keep-alive.
   More than ~8 connections mostly measures the kernel queue. `--connections 4`
   is the realistic default for these VPS sizes.
7. **One change at a time.** Rebuild flags, allocator, SQLite settings and code
   changes are separate rounds. Never compare a baseline measured with `-x`
   against a candidate measured without it.

`run.sh --clean-build` exists because the Makefile does not track flag changes.
A plain `make` after changing `EXTRA_CFLAGS` keeps the old objects. It runs
`make clean`, which deletes every `_db.sqlite3` under the tree, including the
generated apps; they reseed on the next start.

## A/B a change

Use the same results root, the same mode, the same flags and the same box:

```bash
bench/run.sh baseline --results bench/results/base --flags "-x -w"

# edit src/, then:
bench/run.sh baseline --results bench/results/candidate --flags "-x -w" --clean-build

bench/report.sh --results bench/results/candidate --baseline bench/results/base
```

Accept a change only when:

- the median moves in the right direction on the primary workload;
- the p90 across repeats does not regress beyond the baseline spread;
- `perf stat` instructions/request does not go up, or the rise is explained;
- no workload outside the target collapses unexpectedly.

For build-flag rounds (LTO, PGO), the same sequence works:

```bash
EXTRA_CFLAGS="-flto -fprofile-use=..." \
EXTRA_LDFLAGS="-flto" \
bench/run.sh baseline --results bench/results/lto --clean-build
```

PGO training profiles are not generated by this harness; build them with the
compiler's documented workflow and pass the flags through. Record them in
`run.env` via `EXTRA_CFLAGS`.

## The generated app

`gen-app.sh` writes a real Bialet app, not a mock:

- `_migration.wren` creates `items` and `writes` and seeds 10,000 rows once
  (guarded by `COUNT(*)`), plus a per-site label in `BIALET_CONFIG`;
- `index.wren` does a SELECT and an HTML loop over 50 rows;
- `item.wren` reads by id with a `?` parameter and routes a miss to `404.wren`;
- `write.wren` POSTs an upsert into 1,000 fixed slots and returns 201;
- `api/items.wren` returns `Response.json`;
- `session.wren` writes session rows and a cookie;
- `module.wren` imports `_app/shared.wren`;
- `render.wren` builds 600 rows with escaping and no DB;
- `page-000.wren` ... `page-119.wren` are generated routes; every 17th has a
  ~32 KiB template, so the bytecode cache has a realistic file-size mix;
- `assets/` has a small CSS, a ~50 KiB JS and a 1 MiB binary;
- `404.wren` is the custom error page.

Generated apps live under `bench/work/apps/` and are gitignored. Re-running
`gen-app.sh` without `--force` preserves `_db.sqlite3`, so the seed is paid
once. `--reset-db` on `run.sh` passes `--force` and gives every A/B arm the same
starting database.

Validate the generated Wren without starting a server:

```bash
bench/gen-app.sh --out bench/work/apps/site-1 --validate
```

It runs `build/bialet -t` on every generated `.wren` from a scratch directory.
If the app did not already have a `_db.sqlite3`, the validator's throwaway
database is removed afterwards.

## Results layout

```text
bench/results/<UTC>/
├── run.env                 exact flags, commit, binary hash, config
├── servers.tsv             site, port, app, supervisor pid, HTTP child pid, log
├── meta/environment.txt    uname, lscpu, free, compiler, governor, paranoid
├── meta/build.log          make output
├── load/<workload>/rN.*    one JSON (oha/fallback) or text (wrk) per repeat
├── perf/stat-<workload>.txt
├── profile/<workload>.*    .data, .flat.txt, .cumulative.txt, .srcline.txt, .svg
├── alloc/heaptrack.txt     heaptrack_print
├── syscalls/strace-c.txt
├── mem/rss.csv             phase-labelled samples, all HTTP children
├── logs/site-N.log
├── report.md
├── pareto.md
└── load-summary.json
```

`bench/work/` and `bench/results/` are gitignored. Nothing in `bench/` writes
outside those directories except the optional `make` build and `make clean`.

## Script reference

| File | Purpose |
| --- | --- |
| `install-tools.sh` | idempotent Debian/Ubuntu toolchain installer |
| `gen-app.sh` | one synthetic app (`--routes`, `--seed`, `--force`, `--validate`) |
| `gen-sites.sh` | N apps under `bench/work/apps/` |
| `start-sites.sh` | one server per app, consecutive ports, logs + RSS sampler |
| `stop-sites.sh` | stop sampler and servers from the manifest |
| `run.sh` | orchestrator: build, generate, start, warm up, measure/profile |
| `profile.sh` | perf record + flat/cumulative/srcline reports + flamegraph |
| `mem.sh` | RSS sampler; standalone mode reports startup/idle/load |
| `report.sh` | analyze a results directory |
| `analyze.py` | parser and Markdown/Pareto generator (stdlib only) |
| `lib/load.sh` | oha/wrk/python client selection, URL building, preflight |
| `lib/load.py` | dependency-free fallback load generator |
| `lib/wrk-request.lua` | wrk request script for GET/POST/mixed |
| `lib/common.sh` | paths, port checks, pid helpers, cleanup |
| `workloads/*.txt` | path/method/status definitions |
| `plan/ROUNDS.md` | five-round improvement plan template |
| `plan/FINDINGS.md` | first-run findings template |

## Troubleshooting

- **`perf stat` says "Permission denied"** - lower `kernel.perf_event_paranoid`
  to 1 or run with the permissions the box gives you. Counts are not optional;
  without them, use the latency table only.
- **`perf record` produces an empty file** - the same permission issue, or
  missing debug info. The build already passes `-g`; check `file build/bialet`.
- **heaptrack attach exits immediately** - the attach path needs ptrace and
  gdb (the installer includes gdb; the collector injects through gdb). Some
  container and hardened-VM configurations disallow it. The error is kept in
  `alloc/attach.log`; the rest of the run is unaffected.
- **No flamegraph** - install the FlameGraph scripts (`install-tools.sh`) or
  point `FLAMEGRAPH_DIR` at a checkout. The profile text and Pareto tables are
  produced without it.
- **`wrk` reports connection refused at high concurrency** - the listen backlog
  is 10. Lower `--connections`; this is documented server behavior, not a
  harness bug.
- **Every request is slow on the first run** - you are paying the 10k-row seed
  and first-request compiles. Warm-up should absorb it; check that the seed
  count in `_db.sqlite3` is already 10,000 before measuring.
- **`make clean` deleted my data** - it deletes every `_db.sqlite3*` under the
  repository. That is intentional (see `AGENTS.md`). Keep app data elsewhere.
