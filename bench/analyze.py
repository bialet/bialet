#!/usr/bin/env python3
"""Turn a Bialet benchmark results directory into report.md and pareto.md.

Reads what bench/run.sh, profile.sh and mem.sh wrote:

  load/<workload>/rN.json|txt   oha JSON, wrk text, or the python fallback
  perf/stat-<workload>.txt      perf stat counters
  profile/<label>.flat.txt      perf report self time
  profile/<label>.cumulative.txt perf report inclusive time
  profile/<label>.srcline.txt   perf report with source file:line
  alloc/heaptrack.txt           heaptrack_print output
  syscalls/strace-c.txt         strace -c summary
  mem/rss.csv                   mem.sh sampler output

Missing pieces are reported as unavailable; they never abort the report.
Standard library only. Run directly or through bench/report.sh.
"""

import argparse
import csv
import glob
import json
import math
import os
import re
import sys
import time

# ---------------------------------------------------------------------------
# Small helpers
# ---------------------------------------------------------------------------

PCT_RE = re.compile(r"^\s*([\d.]+)%")
PATH_RE = re.compile(r"((?:/|\.{0,2}/)?[\w./+-]+\.(?:c|h|cc|cpp|cxx|wren|inc)):(\d+)")
SIZE_RE = re.compile(r"^\s*([\d.]+)\s*([KMGTP]?i?B)\b\s*\(?([\d.]+)?%?\)?\s*(.*)$")
WRK_LAT_RE = re.compile(r"^\s*Latency\s+([\d.]+)(us|ms|s)\b")
WRK_PCT_RE = re.compile(r"^\s*(\d+)%\s+([\d.]+)(us|ms|s)\b")
WRK_RPS_RE = re.compile(r"Requests/sec:\s*([\d.]+)")
WRK_RUN_RE = re.compile(r"Running\s+(\d+)s test")
PERF_RE = re.compile(
    r"^\s*([\d,]+)\s+(cycles|instructions|branches|branch-misses|"
    r"cache-references|cache-misses|context-switches|cpu-migrations|"
    r"page-faults)\b")


def read_text(path):
    try:
        with open(path, "r", encoding="utf-8", errors="replace") as handle:
            return handle.read()
    except OSError:
        return ""


def safe_float(value, default=0.0):
    try:
        return float(value)
    except (TypeError, ValueError):
        return default


def median(values):
    values = [v for v in values if v is not None]
    if not values:
        return None
    ordered = sorted(values)
    n = len(ordered)
    if n % 2:
        return ordered[n // 2]
    return (ordered[n // 2 - 1] + ordered[n // 2]) / 2.0


def percentile(values, pct):
    values = [v for v in values if v is not None]
    if not values:
        return None
    ordered = sorted(values)
    idx = int(round((pct / 100.0) * (len(ordered) - 1)))
    return ordered[max(0, min(idx, len(ordered) - 1))]


def fmt_num(value, digits=1):
    if value is None:
        return "n/a"
    if abs(value) >= 1000:
        return "{:,.0f}".format(value)
    return "{:.{d}f}".format(value, d=digits)


def fmt_int(value):
    if value is None:
        return "n/a"
    try:
        return "{:,}".format(int(value))
    except (TypeError, ValueError):
        return "n/a"


def fmt_ms(value):
    if value is None:
        return "n/a"
    if value < 1:
        return "{:.3f}".format(value)
    return "{:.2f}".format(value)


def fmt_pct(value):
    if value is None:
        return "n/a"
    return "{:.1f}%".format(value)


def fmt_bytes(value):
    if value is None:
        return "n/a"
    units = ["B", "KiB", "MiB", "GiB", "TiB"]
    size = float(value)
    for unit in units:
        if size < 1024 or unit == units[-1]:
            return "{:.1f} {}".format(size, unit)
        size /= 1024.0


def size_to_bytes(number, unit):
    mult = {"B": 1, "KB": 1000, "MB": 1000 ** 2, "GB": 1000 ** 3,
            "TB": 1000 ** 4, "PB": 1000 ** 5,
            "KiB": 1024, "MiB": 1024 ** 2, "GiB": 1024 ** 3,
            "TiB": 1024 ** 4, "PiB": 1024 ** 5}
    return safe_float(number) * mult.get(unit, 1)


def short_path(path, keep=3):
    if not path or path == "(unknown)":
        return path or "(unknown)"
    parts = path.split("/")
    return "/".join(parts[-keep:])


def parse_percent_tokens(line):
    """Return (children_pct, self_pct) as found in a perf report line."""
    tokens = line.split()
    found = []
    for index, token in enumerate(tokens):
        if re.fullmatch(r"[\d.]+%", token):
            found.append((index, safe_float(token.rstrip("%"))))
    if not found:
        return None, None, tokens
    if len(found) >= 2:
        return found[0][1], found[1][1], tokens
    return None, found[0][1], tokens


def perf_symbol(tokens, last_pct_index):
    rest = tokens[last_pct_index + 1:]
    if not rest:
        return "", ""
    path_match = PATH_RE.search(rest[-1])
    if path_match:
        source = path_match.group(1)
        symbol = rest[-2] if len(rest) >= 2 else ""
        return symbol, source
    symbol = rest[-1]
    # perf decorates unresolved symbols as [.] func+0x12 or [k] func.
    symbol = symbol.split("+0x")[0]
    return symbol, ""


# ---------------------------------------------------------------------------
# Load results
# ---------------------------------------------------------------------------

def parse_oha(path):
    try:
        with open(path, "r", encoding="utf-8", errors="replace") as handle:
            data = json.load(handle)
    except (OSError, ValueError):
        return None
    if not isinstance(data, dict):
        return None
    summary = data.get("summary")
    if not isinstance(summary, dict):
        return None
    # oha has shipped both camelCase (current) and snake_case (older) JSON.
    # Percentiles moved from summary.response_time_percentiles to top-level
    # latencyPercentiles; status codes moved to top-level statusCodeDistribution.
    pcts = (data.get("latencyPercentiles")
            or data.get("responseTimePercentiles")
            or summary.get("response_time_percentiles")
            or summary.get("responseTimePercentiles")
            or {})

    def pick_pct(name):
        for key in (name, name.lstrip("p")):
            if key in pcts:
                return pcts[key]
        return None

    rps = summary.get("requestsPerSec")
    if rps is None:
        rps = summary.get("requests_per_sec")
    rps = safe_float(rps, None)
    p50 = pick_pct("p50")
    p90 = pick_pct("p90")
    p99 = pick_pct("p99")

    def seconds_to_ms(value):
        return safe_float(value) * 1000.0 if value is not None else None

    distribution = (data.get("statusCodeDistribution")
                    or data.get("status_code_distribution")
                    or summary.get("status_code_distribution")
                    or {})
    status_codes = {}
    for code, count in distribution.items():
        try:
            status_codes[str(code)] = int(count)
        except (TypeError, ValueError):
            pass

    total = sum(status_codes.values()) if status_codes else 0
    error_distribution = (data.get("errorDistribution")
                          or summary.get("error_distribution")
                          or {})
    error_count = 0
    for count in error_distribution.values():
        try:
            error_count += int(count)
        except (TypeError, ValueError):
            pass
    total += error_count
    if total == 0:
        histogram = (data.get("responseTimeHistogram")
                     or summary.get("response_time_histogram")
                     or {})
        try:
            total = sum(int(count) for count in histogram.values())
        except (TypeError, ValueError):
            total = 0
    if total == 0:
        # Older oha used an integer `total` as the request count; newer oha
        # uses it as the elapsed time in seconds. Only trust an integer.
        old_total = summary.get("total")
        if isinstance(old_total, int) and not isinstance(old_total, bool):
            total = old_total

    success = summary.get("successRate")
    if success is None:
        success = summary.get("success_rate")
    success = safe_float(success, None)
    if success is not None and success <= 1.0:
        success *= 100.0
    ok = int(round(total * success / 100.0)) if success is not None else total
    return {
        "impl": "oha",
        "rps": rps,
        "p50_ms": seconds_to_ms(p50),
        "p90_ms": seconds_to_ms(p90),
        "p99_ms": seconds_to_ms(p99),
        "total": total,
        "ok": ok,
        "errors": max(0, total - ok),
        "success_pct": success,
        "status_codes": status_codes,
        "urls": data.get("urls") or {},
    }


def parse_fallback(path):
    try:
        with open(path, "r", encoding="utf-8", errors="replace") as handle:
            data = json.load(handle)
    except (OSError, ValueError):
        return None
    if not isinstance(data, dict) or data.get("impl") is None:
        return None
    total = int(safe_float(data.get("total"), 0))
    ok = int(safe_float(data.get("ok"), 0))
    return {
        "impl": str(data.get("impl")),
        "rps": safe_float(data.get("rps"), None),
        "p50_ms": safe_float(data.get("p50_ms"), None),
        "p90_ms": safe_float(data.get("p90_ms"), None),
        "p99_ms": safe_float(data.get("p99_ms"), None),
        "total": total,
        "ok": ok,
        "errors": int(safe_float(data.get("errors"), max(0, total - ok))),
        "success_pct": (100.0 * ok / total) if total else None,
        "status_codes": {str(k): int(v) for k, v in (data.get("status_codes") or {}).items()},
        "urls": data.get("urls") or [],
    }


def wrk_duration_to_ms(value, unit):
    value = safe_float(value)
    if unit == "us":
        return value / 1000.0
    if unit == "s":
        return value * 1000.0
    return value


def parse_wrk(path):
    text = read_text(path)
    if "Requests/sec" not in text:
        return None
    rps = None
    p50 = p90 = p99 = None
    avg = None
    duration = None
    for line in text.splitlines():
        match = WRK_RPS_RE.search(line)
        if match:
            rps = safe_float(match.group(1))
            continue
        match = WRK_RUN_RE.search(line)
        if match:
            duration = safe_float(match.group(1))
            continue
        match = WRK_LAT_RE.search(line)
        if match:
            avg = wrk_duration_to_ms(match.group(1), match.group(2))
            continue
        match = WRK_PCT_RE.search(line)
        if match:
            pct = int(match.group(1))
            value = wrk_duration_to_ms(match.group(2), match.group(3))
            if pct == 50:
                p50 = value
            elif pct == 90:
                p90 = value
            elif pct == 99:
                p99 = value
    total = int(round(rps * duration)) if rps is not None and duration else 0
    return {
        "impl": "wrk",
        "rps": rps,
        "p50_ms": p50,
        "p90_ms": p90,
        "p99_ms": p99,
        "p50_fallback_ms": avg,
        "total": total,
        "ok": total,
        "errors": 0,
        "success_pct": None,
        "status_codes": {},
        "urls": [],
    }


def collect_load(results):
    """Return {workload: [run, ...]} sorted by repeat number."""
    workloads = {}
    load_dir = os.path.join(results, "load")
    if not os.path.isdir(load_dir):
        return workloads
    for wl_dir in sorted(os.listdir(load_dir)):
        wl_path = os.path.join(load_dir, wl_dir)
        if not os.path.isdir(wl_path):
            continue
        runs = []
        for path in glob.glob(os.path.join(wl_path, "r*")):
            base = os.path.basename(path)
            if not re.match(r"r\d+\.(json|txt)$", base):
                continue
            parsed = None
            if base.endswith(".json"):
                parsed = parse_oha(path) or parse_fallback(path)
            else:
                parsed = parse_wrk(path)
            if parsed:
                parsed["repeat"] = int(re.match(r"r(\d+)", base).group(1))
                parsed["file"] = os.path.basename(path)
                runs.append(parsed)
        if runs:
            runs.sort(key=lambda run: run["repeat"])
            workloads[wl_dir] = runs
    return workloads


def summarize_load(workloads):
    summary = {}
    for wl, runs in workloads.items():
        rps = [run["rps"] for run in runs if run.get("rps") is not None]
        p50 = [run["p50_ms"] if run.get("p50_ms") is not None
               else run.get("p50_fallback_ms") for run in runs]
        p90 = [run["p90_ms"] for run in runs if run.get("p90_ms") is not None]
        p99 = [run["p99_ms"] for run in runs if run.get("p99_ms") is not None]
        totals = [run["total"] for run in runs]
        statuses = {}
        for run in runs:
            for code, count in (run.get("status_codes") or {}).items():
                statuses[code] = statuses.get(code, 0) + count
        success = [run["success_pct"] for run in runs
                   if run.get("success_pct") is not None]
        summary[wl] = {
            "repeats": len(runs),
            "impl": runs[0].get("impl"),
            "rps_median": median(rps),
            "rps_p90": percentile(rps, 90),
            "rps_min": min(rps) if rps else None,
            "rps_max": max(rps) if rps else None,
            "p50_median_ms": median(p50),
            "p50_p90_ms": percentile(p50, 90),
            "p90_median_ms": median(p90),
            "p90_p90_ms": percentile(p90, 90),
            "p99_median_ms": median(p99),
            "requests_median": median(totals),
            "success_median_pct": median(success),
            "status_codes": statuses,
        }
    return summary


# ---------------------------------------------------------------------------
# perf stat
# ---------------------------------------------------------------------------

def parse_perf_stat(path):
    text = read_text(path)
    counters = {}
    elapsed = None
    for line in text.splitlines():
        match = PERF_RE.match(line)
        if match:
            counters[match.group(2)] = int(match.group(1).replace(",", ""))
            continue
        match = re.search(r"([\d.]+)\s+seconds time elapsed", line)
        if match:
            elapsed = safe_float(match.group(1))
    return {"counters": counters, "elapsed_s": elapsed, "text": text}


def collect_perf_stats(results):
    stats = {}
    for path in sorted(glob.glob(os.path.join(results, "perf", "stat-*.txt"))):
        name = os.path.basename(path)[len("stat-"):-len(".txt")]
        parsed = parse_perf_stat(path)
        if parsed["counters"] or parsed["text"].strip():
            stats[name] = parsed
    return stats


# ---------------------------------------------------------------------------
# CPU Pareto
# ---------------------------------------------------------------------------

def collect_cpu_pareto(results):
    """Return {label: {"self": [...], "cumulative": [...], "files": {...}}}."""
    srcline_map = {}
    for path in sorted(glob.glob(os.path.join(results, "profile", "*.srcline.txt"))):
        label = os.path.basename(path)[:-len(".srcline.txt")]
        srcline_map[label] = parse_srcline(path)

    profiles = {}
    for path in sorted(glob.glob(os.path.join(results, "profile", "*.flat.txt"))):
        label = os.path.basename(path)[:-len(".flat.txt")]
        self_entries = parse_perf_report(path, srcline_map.get(label, {}),
                                         cumulative=False)
        cum_path = os.path.join(results, "profile", label + ".cumulative.txt")
        cum_entries = parse_perf_report(cum_path, srcline_map.get(label, {}),
                                        cumulative=True)
        if not self_entries and not cum_entries:
            continue
        profiles[label] = {
            "self": self_entries,
            "cumulative": cum_entries,
            "files": group_by_file(self_entries),
            "pareto": smallest_80(self_entries),
            "file_pareto": smallest_80(self_entries, key="file"),
        }
    return profiles


def parse_srcline(path):
    mapping = {}
    last_symbol = ""
    for line in read_text(path).splitlines():
        if not line.strip() or line.lstrip().startswith("#"):
            continue
        children, self_pct, tokens = parse_percent_tokens(line)
        if self_pct is not None:
            idx = next((i for i, token in enumerate(tokens)
                        if re.fullmatch(r"[\d.]+%", token)), 0)
            # With two percentage columns the symbol follows the second.
            pct_indices = [i for i, token in enumerate(tokens)
                           if re.fullmatch(r"[\d.]+%", token)]
            symbol, source = perf_symbol(tokens, pct_indices[-1])
            last_symbol = symbol
            match = PATH_RE.search(line)
            if match:
                mapping[symbol or last_symbol] = match.group(1)
        else:
            match = PATH_RE.search(line)
            if match and last_symbol:
                mapping[last_symbol] = match.group(1)
    return mapping


def parse_perf_report(path, srcline_map, cumulative=False):
    entries = []
    for line in read_text(path).splitlines():
        stripped = line.strip()
        if not stripped or stripped.startswith("#"):
            continue
        children, self_pct, tokens = parse_percent_tokens(line)
        if self_pct is None:
            continue
        pct_indices = [i for i, token in enumerate(tokens)
                       if re.fullmatch(r"[\d.]+%", token)]
        symbol, source = perf_symbol(tokens, pct_indices[-1])
        if not symbol:
            continue
        source = source or srcline_map.get(symbol, "")
        entries.append({
            "symbol": symbol,
            "file": source or "(no debug info)",
            "self_pct": self_pct,
            "children_pct": children,
        })
    if cumulative:
        # Cumulative report may print both Children and Self. Merge into the
        # self list by symbol so the report has one row per function.
        merged = {}
        for entry in entries:
            key = entry["symbol"]
            if key not in merged:
                merged[key] = entry
            else:
                merged[key]["children_pct"] = max(
                    merged[key].get("children_pct") or 0,
                    entry.get("children_pct") or 0)
        return list(merged.values())
    return entries


def group_by_file(entries):
    groups = {}
    for entry in entries:
        key = entry.get("file") or "(no debug info)"
        groups[key] = groups.get(key, 0.0) + entry.get("self_pct", 0.0)
    result = [{"file": key, "self_pct": value}
              for key, value in groups.items()]
    result.sort(key=lambda item: item["self_pct"], reverse=True)
    return result


def smallest_80(entries, key="symbol"):
    if key == "file":
        ordered = [{"label": item["file"], "self_pct": item["self_pct"]}
                   for item in entries]
    else:
        ordered = [{"label": item["symbol"], "self_pct": item["self_pct"]}
                   for item in entries]
    ordered.sort(key=lambda item: item["self_pct"], reverse=True)
    total = sum(item["self_pct"] for item in ordered)
    running = 0.0
    selected = []
    for item in ordered:
        selected.append(item)
        running += item["self_pct"]
        if total > 0 and running / total >= 0.80:
            break
    return {
        "items": selected,
        "count": len(selected),
        "total": len(ordered),
        "percent": (100.0 * running / total) if total else 0.0,
    }


# ---------------------------------------------------------------------------
# Allocation Pareto
# ---------------------------------------------------------------------------

def clean_location(text):
    text = text.strip()
    text = re.sub(r"^[.\[\]kgu]+\s+", "", text)
    text = re.sub(r"\s+", " ", text)
    return text[:200]


def parse_heaptrack(path):
    text = read_text(path)
    if not text:
        return None
    peak = None
    match = re.search(r"peak memory consumption:\s*([\d.]+)\s*([KMGTP]?i?B)",
                      text, re.IGNORECASE)
    if match:
        peak = size_to_bytes(match.group(1), match.group(2))
    entries = []
    section = ""
    pending = None
    for line in text.splitlines():
        stripped = line.strip()
        if not stripped:
            continue
        if re.fullmatch(r"[A-Z][A-Z0-9 /_-]{4,}", stripped):
            section = stripped
            continue
        size_match = SIZE_RE.match(line)
        pct_match = re.match(
            r"^\s*([\d.]+)%\s+([\d.]+)\s*([KMGTP]?i?B)\s*(.*)$", line)
        if size_match:
            number, unit, pct, rest = size_match.groups()
            size = size_to_bytes(number, unit)
            percent = safe_float(pct) if pct is not None else None
            location = clean_location(rest)
            if not location:
                pending = {"size": size, "percent": percent, "section": section}
                continue
            entries.append({"size": size, "percent": percent,
                            "location": location, "section": section})
            pending = None
        elif pct_match:
            percent, number, unit, rest = pct_match.groups()
            entries.append({"size": size_to_bytes(number, unit),
                            "percent": safe_float(percent),
                            "location": clean_location(rest),
                            "section": section})
            pending = None
        elif pending is not None and (" at " in line or " in " in line
                                      or PATH_RE.search(line)):
            pending["location"] = clean_location(stripped)
            entries.append(pending)
            pending = None
        elif entries and (" at " in line or " in " in line) and not entries[-1]["location"]:
            entries[-1]["location"] = clean_location(stripped)

    # Keep allocation-ish sections if heaptrack labelled them; otherwise keep
    # everything that looks like a site.
    interesting = [entry for entry in entries
                   if not entry["section"]
                   or re.search(r"ALLOC|PEAK|LEAK", entry["section"])]
    if interesting:
        entries = interesting
    if entries:
        known = [entry["size"] for entry in entries if entry["size"] > 0]
        total = sum(known)
        for entry in entries:
            if entry["percent"] is None and total > 0:
                entry["percent"] = 100.0 * entry["size"] / total
            entry["file"] = location_file(entry["location"])
        # Deduplicate identical locations, keeping the largest.
        dedup = {}
        for entry in entries:
            key = entry["location"] or entry["file"]
            if key not in dedup or entry["size"] > dedup[key]["size"]:
                dedup[key] = entry
        entries = sorted(dedup.values(),
                         key=lambda item: item.get("percent") or 0,
                         reverse=True)
    return {"entries": entries, "peak_bytes": peak, "text": text}


def location_file(location):
    match = PATH_RE.search(location or "")
    if match:
        return match.group(1)
    return "(unknown)"


# ---------------------------------------------------------------------------
# Syscall Pareto
# ---------------------------------------------------------------------------

def parse_strace(path):
    text = read_text(path)
    entries = []
    for line in text.splitlines():
        stripped = line.strip()
        if not stripped or stripped.startswith("---"):
            continue
        if re.match(r"^%\s*time|^-+$|^total\b", stripped):
            continue
        tokens = stripped.split()
        if len(tokens) < 5:
            continue
        try:
            percent = float(tokens[0])
            seconds = float(tokens[1])
        except ValueError:
            continue
        name = tokens[-1]
        if name == "total":
            continue
        calls = None
        errors = None
        # Find the last numeric token before the syscall name; the field before
        # it is calls when the errors column is present.
        numbers = []
        for token in tokens[2:-1]:
            try:
                numbers.append(int(token))
            except ValueError:
                pass
        if len(numbers) >= 3:
            calls = numbers[-2]
            errors = numbers[-1]
        elif len(numbers) == 2:
            calls = numbers[-1]
            errors = None
        elif len(numbers) == 1:
            calls = numbers[0]
            errors = None
        else:
            calls = None
            errors = None
        entries.append({"name": name, "percent": percent,
                        "seconds": seconds, "calls": calls,
                        "errors": errors})
    entries.sort(key=lambda item: item["percent"], reverse=True)
    total = sum(item["percent"] for item in entries)
    running = 0.0
    selected = []
    for entry in entries:
        selected.append(entry)
        running += entry["percent"]
        if total > 0 and running / total >= 0.80:
            break
    return {
        "entries": entries,
        "count": len(entries),
        "pareto": {"items": selected, "count": len(selected),
                   "total": len(entries),
                   "percent": (100.0 * running / total) if total else 0.0},
    }


# ---------------------------------------------------------------------------
# Memory CSV
# ---------------------------------------------------------------------------

def parse_rss(results):
    path = os.path.join(results, "mem", "rss.csv")
    if not os.path.isfile(path):
        return None
    rows = []
    try:
        with open(path, "r", encoding="utf-8", errors="replace") as handle:
            reader = csv.DictReader(handle)
            for row in reader:
                rows.append(row)
    except OSError:
        return None
    if not rows:
        return None
    # mem.sh writes one row per process per iteration, all sharing elapsed_s.
    phases = {}
    for row in rows:
        phase = row.get("phase") or "sample"
        try:
            elapsed = row.get("elapsed_s") or "0"
            rss = int(float(row.get("vmrss_kb") or 0))
            hwm = int(float(row.get("vmhwm_kb") or 0))
        except ValueError:
            continue
        key = (phase, elapsed)
        entry = phases.setdefault(key, {"rss": 0, "hwm": 0})
        entry["rss"] += rss
        entry["hwm"] += hwm
    by_phase = {}
    for (phase, _elapsed), values in phases.items():
        bucket = by_phase.setdefault(phase, {"rss": [], "hwm": []})
        bucket["rss"].append(values["rss"])
        bucket["hwm"].append(values["hwm"])
    summary = {}
    for phase, bucket in by_phase.items():
        summary[phase] = {
            "samples": len(bucket["rss"]),
            "rss_median_kb": median(bucket["rss"]),
            "rss_max_kb": max(bucket["rss"]) if bucket["rss"] else None,
            "hwm_max_kb": max(bucket["hwm"]) if bucket["hwm"] else None,
        }
    return {"path": path, "phases": summary, "rows": len(rows)}


# ---------------------------------------------------------------------------
# Markdown
# ---------------------------------------------------------------------------

def workload_meta(results):
    """Read workload files from the repo, keyed by name, for path/method."""
    bench_dir = os.path.dirname(os.path.abspath(__file__))
    meta = {}
    for path in glob.glob(os.path.join(bench_dir, "workloads", "*.txt")):
        name = os.path.basename(path)[:-4]
        data = {}
        for line in read_text(path).splitlines():
            if "=" in line and not line.lstrip().startswith("#"):
                key, value = line.split("=", 1)
                data[key.strip()] = value.strip()
        meta[name] = data
    return meta


def render_report(results, baseline, loads, load_summary, perf_stats,
                  cpu_profiles, alloc, syscalls, rss, top_n):
    lines = []
    lines.append("# Bialet benchmark report\n")
    lines.append("Results: `{}`  ".format(results))
    lines.append("Generated: {} (UTC)\n".format(
        time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime())))

    run_env = {}
    for line in read_text(os.path.join(results, "run.env")).splitlines():
        if "=" in line:
            key, value = line.split("=", 1)
            run_env[key.strip()] = value.strip()
    if run_env:
        lines.append("## Configuration\n")
        lines.append("| key | value |")
        lines.append("| --- | --- |")
        for key in sorted(run_env):
            lines.append("| {} | {} |".format(key, run_env[key].replace("|", "\\|")))
        lines.append("")

    env_text = read_text(os.path.join(results, "meta", "environment.txt")).strip()
    if env_text:
        lines.append("## Environment\n")
        lines.append("```")
        lines.append(env_text)
        lines.append("```\n")

    meta = workload_meta(results)
    lines.append("## Workload table\n")
    lines.append("Median and p90 are across repeats. `worst p90` is the p90 of "
                 "the repeat-level p90 values; it is the number to watch for "
                 "regressions.\n")
    lines.append("| workload | request | repeats | rps median | rps p90 | "
                 "p50 ms | p90 ms | worst p90 ms | success | status codes |")
    lines.append("| --- | --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | --- |")
    order = list(meta.keys())
    for wl in sorted(load_summary, key=lambda name: (
            order.index(name) if name in order else 999, name)):
        data = load_summary[wl]
        info = meta.get(wl, {})
        request = info.get("path") or info.get("paths") or "?"
        method = info.get("method", "GET")
        codes = ", ".join("{}:{}".format(k, v)
                          for k, v in sorted(data["status_codes"].items()))
        success = data["success_median_pct"]
        lines.append("| {} | {} {} | {} | {} | {} | {} | {} | {} | {} | {} |".format(
            wl, method, request.replace("|", "\\|"), data["repeats"],
            fmt_num(data["rps_median"]), fmt_num(data["rps_p90"]),
            fmt_ms(data["p50_median_ms"]), fmt_ms(data["p90_median_ms"]),
            fmt_ms(data["p90_p90_ms"]),
            fmt_pct(success) if success is not None else "n/a",
            codes or "n/a"))
    lines.append("")

    if baseline:
        baseline_loads = collect_load(baseline)
        baseline_summary = summarize_load(baseline_loads)
        lines.append("### Delta vs baseline `{}`\n".format(baseline))
        lines.append("| workload | rps | Δ rps | p50 ms | Δ p50 | p90 ms | Δ p90 |")
        lines.append("| --- | ---: | ---: | ---: | ---: | ---: | ---: |")
        for wl in sorted(load_summary):
            if wl not in baseline_summary:
                continue
            base = baseline_summary[wl]
            cur = load_summary[wl]

            def delta(new, old):
                if new is None or old in (None, 0):
                    return "n/a"
                return "{:+.1f}%".format(100.0 * (new - old) / old)

            lines.append("| {} | {} | {} | {} | {} | {} | {} |".format(
                wl, fmt_num(cur["rps_median"]),
                delta(cur["rps_median"], base["rps_median"]),
                fmt_ms(cur["p50_median_ms"]),
                delta(cur["p50_median_ms"], base["p50_median_ms"]),
                fmt_ms(cur["p90_median_ms"]),
                delta(cur["p90_median_ms"], base["p90_median_ms"])))
        lines.append("")

    lines.append("### Per-repeat numbers\n")
    for wl in sorted(loads):
        lines.append("| {} repeat | rps | p50 ms | p90 ms | p99 ms | requests | impl |".format(wl))
        lines.append("| ---: | ---: | ---: | ---: | ---: | ---: | --- |")
        for run in loads[wl]:
            lines.append("| {} | {} | {} | {} | {} | {} | {} |".format(
                run.get("repeat", "?"), fmt_num(run.get("rps")),
                fmt_ms(run.get("p50_ms") if run.get("p50_ms") is not None
                       else run.get("p50_fallback_ms")),
                fmt_ms(run.get("p90_ms")), fmt_ms(run.get("p99_ms")),
                run.get("total", 0), run.get("impl", "?")))
        lines.append("")

    if perf_stats:
        lines.append("## perf stat\n")
        lines.append("Counters are for the whole load window on the HTTP child. "
                     "`per request` divides by the median request count of the "
                     "matching load runs.\n")
        base_stats = collect_perf_stats(baseline) if baseline else {}
        metrics = ["cycles", "instructions", "branches", "branch-misses",
                   "cache-references", "cache-misses"]
        lines.append("| workload | metric | value | per request | Δ vs baseline |")
        lines.append("| --- | --- | ---: | ---: | ---: |")
        for wl in sorted(perf_stats):
            counters = perf_stats[wl]["counters"]
            requests = (load_summary.get(wl, {}) or {}).get("requests_median")
            for metric in metrics:
                if metric not in counters:
                    continue
                value = counters[metric]
                per_req = (value / requests) if requests else None
                delta = "n/a"
                if wl in base_stats and metric in base_stats[wl]["counters"]:
                    old = base_stats[wl]["counters"][metric]
                    if old:
                        delta = "{:+.1f}%".format(100.0 * (value - old) / old)
                lines.append("| {} | {} | {} | {} | {} |".format(
                    wl, metric, fmt_num(value), fmt_num(per_req), delta))
        lines.append("")

    if rss:
        lines.append("## Memory\n")
        lines.append("From `{}`; totals sum every sampled process. `peak HWM` is "
                     "the sum of per-process high-water marks.\n".format(
                         os.path.relpath(rss["path"], results)))
        lines.append("| phase | samples | median RSS | max RSS | peak HWM |")
        lines.append("| --- | ---: | ---: | ---: | ---: |")
        for phase in sorted(rss["phases"]):
            data = rss["phases"][phase]
            lines.append("| {} | {} | {} | {} | {} |".format(
                phase, data["samples"], fmt_bytes(data["rss_median_kb"] * 1024),
                fmt_bytes(data["rss_max_kb"] * 1024),
                fmt_bytes(data["hwm_max_kb"] * 1024)))
        lines.append("")

    if cpu_profiles:
        lines.append("## CPU profile\n")
        lines.append("Self time is the flat `perf report --no-children` view; "
                     "see `pareto.md` for the 80/20 set.\n")
        for label in sorted(cpu_profiles):
            entries = sorted(cpu_profiles[label]["self"],
                             key=lambda item: item["self_pct"], reverse=True)
            lines.append("### `{}`\n".format(label))
            lines.append("| self % | function | source file |")
            lines.append("| ---: | --- | --- |")
            for entry in entries[:top_n]:
                lines.append("| {} | {} | {} |".format(
                    fmt_pct(entry["self_pct"]), entry["symbol"],
                    short_path(entry["file"])))
            lines.append("")

    if alloc and alloc["entries"]:
        lines.append("## Allocation sites\n")
        lines.append("Peak consumption: {}. Top sites from `heaptrack_print`.\n".format(
            fmt_bytes(alloc.get("peak_bytes"))))
        lines.append("| % | size | site |")
        lines.append("| ---: | ---: | --- |")
        for entry in alloc["entries"][:top_n]:
            lines.append("| {} | {} | {} |".format(
                fmt_pct(entry.get("percent")), fmt_bytes(entry.get("size")),
                entry.get("location", "")[:140]))
        lines.append("")

    if syscalls and syscalls["entries"]:
        lines.append("## Syscall profile\n")
        lines.append("`strace -c`; the tracer adds overhead, so compare shapes "
                     "and ratios, not absolute latency.\n")
        lines.append("| % time | syscall | calls | errors |")
        lines.append("| ---: | --- | ---: | ---: |")
        for entry in syscalls["entries"][:top_n]:
            lines.append("| {} | {} | {} | {} |".format(
                fmt_pct(entry["percent"]), entry["name"],
                fmt_int(entry.get("calls")), fmt_int(entry.get("errors"))))
        lines.append("")

    lines.append("## Raw artifacts\n")
    artifacts = []
    for sub in ("run.env", "servers.tsv", "meta", "load", "perf", "profile",
                "alloc", "syscalls", "mem", "logs"):
        path = os.path.join(results, sub)
        if os.path.isdir(path):
            artifacts.append("`{}/`".format(sub))
        elif os.path.isfile(path):
            artifacts.append("`{}`".format(sub))
    lines.append(", ".join(artifacts))
    lines.append("")
    return "\n".join(lines)


def render_pareto(cpu_profiles, alloc, syscalls, top_n):
    lines = []
    lines.append("# Pareto: the small set of hot spots\n")
    lines.append("80/20 here is taken literally: sort by self cost, accumulate, "
                 "and show the smallest prefix that reaches 80% of the total. "
                 "CPU numbers are sample-based, so a function at 0.1% is below "
                 "the noise floor on a short run.\n")

    if not cpu_profiles:
        lines.append("## CPU self time\n\nNo `perf report` output was found. "
                     "Run `bench/run.sh profile` first.\n")
    for label in sorted(cpu_profiles):
        profile = cpu_profiles[label]
        entries = sorted(profile["self"],
                         key=lambda item: item["self_pct"], reverse=True)
        pareto = profile["pareto"]
        lines.append("## CPU self time: `{}`\n".format(label))
        lines.append("Top {} symbols out of {} ({}% of samples shown):\n".format(
            min(top_n, len(entries)), len(entries),
            fmt_num(sum(item["self_pct"] for item in entries[:top_n]))))
        lines.append("| # | function | source file | self % | cumulative % |")
        lines.append("| ---: | --- | --- | ---: | ---: |")
        running = 0.0
        for index, entry in enumerate(entries[:top_n], 1):
            running += entry["self_pct"]
            lines.append("| {} | {} | {} | {} | {} |".format(
                index, entry["symbol"], short_path(entry["file"]),
                fmt_pct(entry["self_pct"]), fmt_pct(running)))
        lines.append("")
        lines.append("**Smallest set reaching 80% of self time:** {} functions "
                     "out of {} ({} of symbols, {} of samples).\n".format(
                         pareto["count"], pareto["total"],
                         fmt_pct(100.0 * pareto["count"] / pareto["total"]
                                 if pareto["total"] else 0),
                         fmt_pct(pareto["percent"])))
        lines.append("Those functions are the Pareto candidates for the next "
                     "round. They are listed here in descending order:\n")
        for index, entry in enumerate(pareto["items"], 1):
            lines.append("{}. `{}` - {}".format(index, entry["label"],
                                                fmt_pct(entry["self_pct"])))
        lines.append("")

        lines.append("### Grouped by source file\n")
        lines.append("| # | source file | self % | cumulative % |")
        lines.append("| ---: | --- | ---: | ---: |")
        running = 0.0
        for index, entry in enumerate(profile["files"][:top_n], 1):
            running += entry["self_pct"]
            lines.append("| {} | {} | {} | {} |".format(
                index, short_path(entry["file"]), fmt_pct(entry["self_pct"]),
                fmt_pct(running)))
        lines.append("")
        file_pareto = profile["file_pareto"]
        lines.append("**Files reaching 80%:** {} of {} files.\n".format(
            file_pareto["count"], file_pareto["total"]))
        lines.append("")

        cumulative = sorted(
            [entry for entry in profile.get("cumulative", [])
             if entry.get("children_pct") is not None],
            key=lambda item: item["children_pct"], reverse=True)
        if cumulative:
            lines.append("### Inclusive time (children)\n")
            lines.append("A high children percentage means the function calls "
                         "the hot code rather than being hot itself.\n")
            lines.append("| # | function | children % | self % |")
            lines.append("| ---: | --- | ---: | ---: |")
            for index, entry in enumerate(cumulative[:top_n], 1):
                lines.append("| {} | {} | {} | {} |".format(
                    index, entry["symbol"], fmt_pct(entry["children_pct"]),
                    fmt_pct(entry.get("self_pct"))))
            lines.append("")

    lines.append("## Allocation sites (`heaptrack_print`)\n")
    if not alloc or not alloc["entries"]:
        lines.append("No heaptrack output found. Run `bench/run.sh alloc`.\n")
    else:
        entries = alloc["entries"]
        running = 0.0
        lines.append("Peak memory: {}. The site column is heaptrack's "
                     "allocation location, usually a stack leaf.\n".format(
                         fmt_bytes(alloc.get("peak_bytes"))))
        lines.append("| # | site | file | % of allocations | cumulative % |")
        lines.append("| ---: | --- | --- | ---: | ---: |")
        selected = []
        for index, entry in enumerate(entries[:max(top_n, 1)], 1):
            percent = entry.get("percent") or 0.0
            running += percent
            selected.append(entry)
            lines.append("| {} | {} | {} | {} | {} |".format(
                index, entry.get("location", "")[:110],
                short_path(entry.get("file", "(unknown)")),
                fmt_pct(percent), fmt_pct(running)))
        lines.append("")
        total_pct = sum(item.get("percent") or 0 for item in entries)
        running = 0.0
        count = 0
        for entry in entries:
            running += entry.get("percent") or 0
            count += 1
            if total_pct and running / total_pct >= 0.80:
                break
        lines.append("**Smallest set reaching 80% of the listed allocations:** "
                     "{} of {} sites.\n".format(count, len(entries)))

    lines.append("## Syscalls (`strace -c`)\n")
    if not syscalls or not syscalls["entries"]:
        lines.append("No strace summary found. Run `bench/run.sh syscalls`.\n")
    else:
        lines.append("| # | syscall | % time | calls | errors | cumulative % |")
        lines.append("| ---: | --- | ---: | ---: | ---: | ---: |")
        running = 0.0
        for index, entry in enumerate(syscalls["entries"][:top_n], 1):
            running += entry["percent"]
            lines.append("| {} | {} | {} | {} | {} | {} |".format(
                index, entry["name"], fmt_pct(entry["percent"]),
                fmt_int(entry.get("calls")), fmt_int(entry.get("errors")),
                fmt_pct(running)))
        lines.append("")
        pareto = syscalls["pareto"]
        lines.append("**Smallest set reaching 80% of syscall time:** {} of {} "
                     "syscalls.\n".format(pareto["count"], pareto["total"]))
        lines.append("`strace` interrupts and resumes the target for every "
                     "syscall; use this table to find the shape of the syscall "
                     "mix, not to price a request.\n")
    return "\n".join(lines)


# ---------------------------------------------------------------------------
# Entry point
# ---------------------------------------------------------------------------

def latest_results(bench_dir):
    results_root = os.path.join(bench_dir, "results")
    candidates = []
    if os.path.isdir(results_root):
        for name in os.listdir(results_root):
            path = os.path.join(results_root, name)
            if os.path.isfile(os.path.join(path, "run.env")):
                candidates.append((os.path.getmtime(path), path))
    return max(candidates)[1] if candidates else None


def main():
    parser = argparse.ArgumentParser(
        description=__doc__,
        formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("results", nargs="?", help="results directory")
    parser.add_argument("--baseline", default=None,
                        help="earlier results directory for deltas")
    parser.add_argument("--top", type=int, default=15,
                        help="rows per table (default 15)")
    args = parser.parse_args()

    bench_dir = os.path.dirname(os.path.abspath(__file__))
    results = args.results or latest_results(bench_dir)
    if not results:
        sys.exit("no results directory given and none found under bench/results/")
    results = os.path.abspath(results)
    if not os.path.isdir(results):
        sys.exit("not a directory: %s" % results)
    baseline = os.path.abspath(args.baseline) if args.baseline else None

    loads = collect_load(results)
    load_summary = summarize_load(loads)
    perf_stats = collect_perf_stats(results)
    cpu_profiles = collect_cpu_pareto(results)
    alloc = parse_heaptrack(os.path.join(results, "alloc", "heaptrack.txt"))
    syscalls = parse_strace(os.path.join(results, "syscalls", "strace-c.txt"))
    rss = parse_rss(results)

    summary = {
        "results": results,
        "baseline": baseline,
        "generated_utc": time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime()),
        "load": load_summary,
        "perf_stat": {wl: data["counters"] for wl, data in perf_stats.items()},
        "cpu_pareto": {
            label: {
                "self": profile["self"],
                "files": profile["files"],
                "pareto": profile["pareto"],
                "file_pareto": profile["file_pareto"],
            }
            for label, profile in cpu_profiles.items()
        },
        "allocation_top": (alloc or {}).get("entries", [])[:50],
        "syscalls": (syscalls or {}).get("entries", []),
        "memory": (rss or {}).get("phases", {}),
    }
    with open(os.path.join(results, "load-summary.json"), "w",
              encoding="utf-8") as handle:
        json.dump(summary, handle, indent=2)

    report = render_report(results, baseline, loads, load_summary, perf_stats,
                           cpu_profiles, alloc, syscalls, rss, args.top)
    pareto = render_pareto(cpu_profiles, alloc, syscalls, args.top)
    with open(os.path.join(results, "report.md"), "w", encoding="utf-8") as handle:
        handle.write(report + "\n")
    with open(os.path.join(results, "pareto.md"), "w", encoding="utf-8") as handle:
        handle.write(pareto + "\n")

    print("report: {}/report.md".format(results))
    print("pareto: {}/pareto.md".format(results))
    print("json:   {}/load-summary.json".format(results))


if __name__ == "__main__":
    try:
        main()
    except KeyboardInterrupt:
        sys.exit(130)
