# Five-round improvement plan

A template, not a promise. Fill it in from `bench/results/<first run>/`. The
order of the rounds is set by the first Pareto table, not by this file. The
candidate list at the end is a set of hypotheses to confirm with data.

## Rules for every round

1. One Pareto item per round. Do not bundle allocator, build flags and code
   changes.
2. Start from the current `dev` plus the previous accepted rounds. Rebuild with
   `--clean-build` when flags change.
3. Measure with the same harness, same box, same mode, same server flags and
   the same database seed as the baseline:
   `bench/run.sh baseline --results bench/results/round-N --flags "<same>"`.
4. Five repeats. Compare median; check the p90 across repeats; check
   instructions/request from `measure` when the change is in the CPU path.
5. Accept only if the primary workload's median improves and the worst p90 does
   not regress beyond the baseline spread. Otherwise revert.
6. Commit the harness result directory is gitignored; commit the code change
   with the measured numbers in the commit message body.
7. Stop when a round is below 2% on the primary workload, or the remaining top
   items are architectural (a different process model, a different storage
   engine) rather than small safe changes.

## Round 0 - baseline

- Commit: `<git rev-parse HEAD>`
- Result dir: `bench/results/<UTC>/`
- Server flags: `-x -w` (record the exact line from `run.env`)
- Load client: local / `BENCH_HOST=<host>` (circle one)
- Repeats: 5 x 10 s, connections 4
- Primary workload: `mixed` (confirm it is representative of the target app)
- Artifacts: `report.md`, `pareto.md`, `load-summary.json`,
  `meta/environment.txt`

Baseline numbers:

| workload | rps median | p50 ms | p90 ms | worst p90 ms | notes |
| --- | ---: | ---: | ---: | ---: | --- |
| static-small | | | | | |
| static-large | | | | | |
| html-cpu | | | | | |
| db-read | | | | | |
| db-write | | | | | |
| json | | | | | |
| session | | | | | |
| mixed | | | | | |

Top self-time functions:

| # | function | file | self % | in 80% set? |
| ---: | --- | --- | ---: | --- |
| 1 | | | | |
| 2 | | | | |
| 3 | | | | |
| 4 | | | | |
| 5 | | | | |

Top allocation sites and syscalls: copy from `pareto.md`.

## Round 1

- Pareto item: `<function / file / syscall / allocation site>`
- Hypothesis: `<why this item costs what it costs>`
- Proposal: `<smallest safe change>`
- Expected effect: `<x% on workload Y; instructions/request down>`
- Risk: `<correctness, cache invalidation, portability, Windows>`

Measure:

| metric | baseline | round 1 | delta |
| --- | ---: | ---: | ---: |
| primary rps median | | | |
| primary p50 median | | | |
| primary p90 median | | | |
| worst p90 | | | |
| instructions / request | | | |
| cycles / request | | | |
| peak RSS | | | |

Decision: accept / revert. Commit: `<sha>`. Notes:

## Round 2

- Pareto item:
- Hypothesis:
- Proposal:
- Expected effect:
- Risk:

Measure:

| metric | baseline | round 2 | delta |
| --- | ---: | ---: | ---: |
| primary rps median | | | |
| primary p50 median | | | |
| primary p90 median | | | |
| worst p90 | | | |
| instructions / request | | | |
| cycles / request | | | |
| peak RSS | | | |

Decision: accept / revert. Commit: `<sha>`. Notes:

## Round 3

- Pareto item:
- Hypothesis:
- Proposal:
- Expected effect:
- Risk:

Measure:

| metric | baseline | round 3 | delta |
| --- | ---: | ---: | ---: |
| primary rps median | | | |
| primary p50 median | | | |
| primary p90 median | | | |
| worst p90 | | | |
| instructions / request | | | |
| cycles / request | | | |
| peak RSS | | | |

Decision: accept / revert. Commit: `<sha>`. Notes:

## Round 4

- Pareto item:
- Hypothesis:
- Proposal:
- Expected effect:
- Risk:

Measure:

| metric | baseline | round 4 | delta |
| --- | ---: | ---: | ---: |
| primary rps median | | | |
| primary p50 median | | | |
| primary p90 median | | | |
| worst p90 | | | |
| instructions / request | | | |
| cycles / request | | | |
| peak RSS | | | |

Decision: accept / revert. Commit: `<sha>`. Notes:

## Round 5

- Pareto item:
- Hypothesis:
- Proposal:
- Expected effect:
- Risk:

Measure:

| metric | baseline | round 5 | delta |
| --- | ---: | ---: | ---: |
| primary rps median | | | |
| primary p50 median | | | |
| primary p90 median | | | |
| worst p90 | | | |
| instructions / request | | | |
| cycles / request | | | |
| peak RSS | | | |

Decision: accept / revert. Commit: `<sha>`. Notes:

## Exit criteria

Stop the loop when any of these is true:

- the next candidate is below 2% on the primary workload (inside the noise);
- the remaining 80% set is dominated by architectural items: process model,
  HTTP connection reuse, SQLite itself, or the Wren VM lifecycle;
- three consecutive rounds are reverted;
- the change would add a dependency or platform-specific code that the project
  does not already carry.

Write down why you stopped. "The remaining 20% of functions are all inside
SQLite" is a valid result.

## Candidate rounds (hypotheses, to be confirmed by the first run)

Do these in the order the Pareto table justifies, not in this order.

1. **Build flags: LTO.** `EXTRA_CFLAGS=-flto`, `EXTRA_LDFLAGS=-flto`, new
   `--clean-build` result dir. Look for instruction/cycle changes on
   `html-cpu` and `json`, not only wall-clock.
2. **Build flags: PGO.** Train on `mixed` with the generated app, then rebuild
   with `-fprofile-use`. Check that the profile does not overfit `mixed` at the
   expense of `static-large`.
3. **Allocator: mimalloc.** Link mimalloc and compare allocation sites and
   peak RSS. This is only worth it if `heaptrack` shows malloc/free or arena
   growth inside the top allocation sites.
4. **SQLite prepared-statement cache.** If `db-read`, `db-write` and `json`
   spend time in `sqlite3_prepare*`, cache prepared statements inside the
   request or across the worker's life. Measure `db-write` separately: a write
   cache has different semantics from a read cache.
5. **Blob / VM setup allocations.** If `heaptrack` points at VM creation,
   module compilation or response buffers, look for one large allocation per
   request that can be reused or sized from the request. Do not grow a
   process-wide cache without an invalidation story.
6. **Network path: keep-alive / sendfile.** `static-large` and `static-small`
   expose the per-request connection cost. A keep-alive or `sendfile` change is
   architectural; only attempt it if the static workloads are a meaningful
   share of the target traffic and the Pareto says the syscall/read/write path
   dominates.
7. **`-x` / `-w` interaction.** The baseline runs with both on because that is
   the documented production shape. A round that only changes the default flag
   is not a code improvement; measure it to understand the cache's value, then
   leave the flag alone.
