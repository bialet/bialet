# Findings from the first run

Fill this from `bench/results/<UTC>/report.md` and `pareto.md`. Keep the raw
tables in the result directory; this file is the summary that survives it.

## Run

- Date (UTC):
- Commit / `git describe`:
- Results directory:
- Machine: IONOS VPS S+ (1 vCPU / 2 GB / NVMe) or Hetzner CX23 (2 vCPU / 4 GB / NVMe)
- Kernel / distro:
- Compiler:
- `perf_event_paranoid`:
- CPU governor:
- Server flags: `-x` on/off, `-w` on/off
- Load client: local / separate host
- Repeats x duration x connections:
- `EXTRA_CFLAGS` / `EXTRA_LDFLAGS`:

## Workload table

Copy the median/p90 table from `report.md`.

| workload | rps median | rps p90 | p50 ms | p90 ms | worst p90 ms | success | notes |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | --- |
| static-small | | | | | | | |
| static-large | | | | | | | |
| html-cpu | | | | | | | |
| db-read | | | | | | | |
| db-write | | | | | | | |
| json | | | | | | | |
| session | | | | | | | |
| mixed | | | | | | | |

## Latency per endpoint

What the absolute cost buys on this box. One short sentence per workload:

- `static-small`:
- `static-large`:
- `html-cpu`:
- `db-read`:
- `db-write`:
- `json`:
- `session`:
- `mixed`:

## Top self-time functions (`perf report --no-children`)

| # | function | source file | self % | cumulative % | workload |
| ---: | --- | --- | ---: | ---: | --- |
| 1 | | | | | |
| 2 | | | | | |
| 3 | | | | | |
| 4 | | | | | |
| 5 | | | | | |
| 6 | | | | | |
| 7 | | | | | |
| 8 | | | | | |
| 9 | | | | | |
| 10 | | | | | |

## Top allocation sites (`heaptrack_print`)

Peak memory:

| # | site | source file | % of allocations |
| ---: | --- | --- | ---: |
| 1 | | | |
| 2 | | | |
| 3 | | | |
| 4 | | | |
| 5 | | | |

## Syscall profile (`strace -c`)

| # | syscall | % time | calls | errors | calls / request |
| ---: | --- | ---: | ---: | ---: | ---: |
| 1 | | | | | |
| 2 | | | | | |
| 3 | | | | | |
| 4 | | | | | |
| 5 | | | | | |

## Memory

- Startup RSS (standalone `mem.sh`):
- Idle RSS under the harness:
- Peak RSS under load:
- Largest phase delta:
- Does memory grow across repeats?:

## What is 80% of the time?

One paragraph. Name the functions and the file(s) they live in. Separate the
CPU Pareto from the allocation and syscall Paretos; they answer different
questions.

## Candidate changes

Ordered by the Pareto table, not by intuition. For each: the item, the change,
the expected impact, the risk.

1. **Item:** `<function/file/allocation/syscall>`
   - Change:
   - Expected impact:
   - Risk:
2. **Item:**
   - Change:
   - Expected impact:
   - Risk:
3. **Item:**
   - Change:
   - Expected impact:
   - Risk:

## Verification plan

- Primary workload and why:
- Secondary workloads that must not regress:
- Repeats and acceptance threshold:
- How the baseline is kept comparable (same flags, same DB seed, same box):
- What would make this change a revert:

## Open questions

- Is the local client contaminating absolute numbers? (If yes, repeat the
  baseline with `BENCH_HOST` before accepting any round.)
- Is the generated app representative of the target traffic mix? Which
  workload should be added or weighted differently?
- Is the Pareto stable across two baseline runs on this box? Record the
  run-to-run spread; a 1% round is not measurable without it.
