#!/usr/bin/env python3
"""HTTP benchmark harness for Bialet.

Boots a throwaway app directory with the given bialet binary and measures
latency/throughput for the request paths that dominate server cost:

  static      a plain static file (no interpreter)
  hello       a minimal .wren handler (VM creation + tiny script)
  markdown    Markdown.html on a medium document
  json        Json.parse + Json.stringify on a ~20 KB document
  rows        SELECT over 2000 rows (query result collection)
  render      an HTML template interpolating a 500 element list

Usage:
  python3 scripts/perf_bench.py                       # default build/bialet
  python3 scripts/perf_bench.py --binary ./build/bialet
  python3 scripts/perf_bench.py --rounds 5 --requests 200
  python3 scripts/perf_bench.py --json out.json       # save raw timings
  python3 scripts/perf_bench.py --baseline out.json   # compare to a saved run

Only the standard library is used, so it runs anywhere Python 3.8+ does.
"""

import argparse
import json
import os
import shutil
import socket
import statistics
import subprocess
import sys
import tempfile
import threading
import time
import urllib.request

BENCH_APP = {
    "index.wren": 'return "ok"\n',
    "hello.wren": 'return "ok"\n',
    "static.html": "<!doctype html>\n<html><body><h1>static</h1>"
    + ("<p>filler filler filler filler filler filler</p>" * 40)
    + "</body></html>\n",
    "markdown.wren": """
var md = ""
for (i in 0...60) {
  md = md + "## Section " + i.toString + "\\n\\n"
  md = md + "Some **bold** text with a [link](https://example.com/x) and `code`.\\n\\n"
  md = md + "- item one\\n- item two\\n- item three\\n\\n"
}
return Markdown.html(md)
""",
    "json.wren": """
var parts = []
for (i in 0...400) {
  parts.add("{\\"id\\":" + i.toString + ",\\"name\\":\\"item " + i.toString +
            "\\",\\"tags\\":[\\"a\\",\\"b\\",\\"c\\"],\\"ok\\":true}")
}
var doc = "[" + parts.join(",") + "]"
var parsed = Json.parse(doc)
return "len:" + parsed.count.toString
""",
    "rows.wren": """
var rows = `SELECT * FROM bench_rows`.fetch
return "rows:" + rows.count.toString + ":" + rows[0]["name"]
""",
    "render.wren": """
var items = []
for (i in 0...500) {
  items.add({"name": "item" + i.toString, "value": i})
}
return <ul>{{ items.map { |it| <li>{{ it["name"] }}={{ it["value"] }}</li> } }}</ul>
""",
    "escape.wren": """
var doc = ""
for (i in 0...200) {
  doc = doc + "line " + i.toString + " with <tags> & 'apostrophes' and more text\n"
}
return <pre>{{ doc }}</pre>
""",
    "upload.wren": """
var file = Request.file("file")
if (!file) return "no-file"
return "saved:" + file.name
""",
    "post.wren": """
if (Request.isPost) {
  return "a:" + (Request.post("a") ? "1" : "0") + ",b:" + (Request.post("b") ? "1" : "0")
}
return "get"
""",
    "_migration.wren": """
`CREATE TABLE IF NOT EXISTS bench_rows (id INTEGER PRIMARY KEY, name TEXT, value TEXT)`.query
var n = `SELECT COUNT(*) FROM bench_rows`.toNum
if (n == 0) {
  for (i in 0...2000) {
    `INSERT INTO bench_rows (name, value) VALUES (?, ?)`.query("row" + i.toString, "value" + i.toString)
  }
}
""",
}

# Binary payload for the upload endpoint: NUL bytes, CRLFs and a byte range that
# no memchr-based scanner can shortcut.
UPLOAD_BOUNDARY = "----bialetperfboundary"
UPLOAD_PAYLOAD = bytes(range(256)) * 256  # 64 KiB

# (name, path, body) -- body is None for GET.
ENDPOINTS = [
    ("static", "/static.html", None),
    ("hello", "/hello", None),
    ("markdown", "/markdown", None),
    ("json", "/json", None),
    ("rows", "/rows", None),
    ("render", "/render", None),
    ("escape", "/escape", None),
    ("post", "/post", b"a=1&b=2&" + b"k" * 8192),
    (
        "upload",
        "/upload",
        b"--" + UPLOAD_BOUNDARY.encode() + b'\r\n'
        b'Content-Disposition: form-data; name="file"; filename="blob.bin"\r\n'
        b"Content-Type: application/octet-stream\r\n\r\n"
        + UPLOAD_PAYLOAD
        + b"\r\n--" + UPLOAD_BOUNDARY.encode() + b"--\r\n",
    ),
]


def free_port():
    s = socket.socket()
    try:
        s.bind(("127.0.0.1", 0))
        return s.getsockname()[1]
    finally:
        s.close()


def write_app(root):
    for name, body in BENCH_APP.items():
        path = os.path.join(root, name)
        with open(path, "w", encoding="utf-8") as fh:
            fh.write(body)


def wait_for_port(port, proc, timeout=30.0):
    deadline = time.time() + timeout
    while time.time() < deadline:
        if proc.poll() is not None:
            return False
        try:
            with socket.create_connection(("127.0.0.1", port), timeout=0.25):
                return True
        except OSError:
            time.sleep(0.05)
    return False


def fetch_once(port, path, body=None):
    url = "http://127.0.0.1:%d%s" % (port, path)
    if body is not None:
        content_type = ("multipart/form-data; boundary=" + UPLOAD_BOUNDARY
                        if path == "/upload" else
                        "application/x-www-form-urlencoded")
        req = urllib.request.Request(url, data=body, method="POST",
                                     headers={"Content-Type": content_type})
    else:
        req = url
    start = time.perf_counter()
    with urllib.request.urlopen(req, timeout=30) as resp:
        resp.read()
    return time.perf_counter() - start


def measure_sequential(port, path, requests, body=None):
    for _ in range(5):  # warmup
        fetch_once(port, path, body)
    samples = []
    for _ in range(requests):
        samples.append(fetch_once(port, path, body))
    samples.sort()
    return {
        "median": statistics.median(samples),
        "p95": samples[min(len(samples) - 1, int(len(samples) * 0.95))],
        "min": samples[0],
        "mean": statistics.fmean(samples),
        "n": len(samples),
    }


def measure_concurrent(port, path, requests, concurrency, body=None):
    errors = []

    def worker(count):
        for _ in range(count):
            try:
                fetch_once(port, path, body)
            except Exception as exc:  # noqa: BLE001 - report, do not abort
                errors.append(exc)

    per_thread = max(1, requests // concurrency)
    threads = [
        threading.Thread(target=worker, args=(per_thread,))
        for _ in range(concurrency)
    ]
    start = time.perf_counter()
    for t in threads:
        t.start()
    for t in threads:
        t.join()
    elapsed = time.perf_counter() - start
    total = per_thread * concurrency
    if errors:
        raise RuntimeError("concurrent requests failed: %r" % errors[0])
    return {"rps": total / elapsed, "n": total, "seconds": elapsed}


def run(args):
    binary = os.path.abspath(args.binary)
    if not os.path.exists(binary):
        sys.exit("binary not found: %s (run make first)" % binary)

    root = tempfile.mkdtemp(prefix="bialet-perf-")
    proc = None
    try:
        write_app(root)
        port = args.port or free_port()
        cmd = [binary, "-p", str(port), "-q", root]
        proc = subprocess.Popen(
            cmd,
            stdout=subprocess.DEVNULL,
            stderr=subprocess.DEVNULL,
            cwd=root,
        )
        if not wait_for_port(port, proc):
            sys.exit("server did not start on port %d" % port)

        results = {}
        for name, path, body in ENDPOINTS:
            seq = measure_sequential(port, path, args.requests, body)
            conc = measure_concurrent(port, path, args.requests,
                                      args.concurrency, body)
            results[name] = {"sequential": seq, "concurrent": conc}
            print(
                "%-9s median %7.2fms  p95 %7.2fms  %8.1f req/s (%d threads)"
                % (
                    name,
                    seq["median"] * 1000,
                    seq["p95"] * 1000,
                    conc["rps"],
                    args.concurrency,
                )
            )
        return results
    finally:
        if proc is not None and proc.poll() is None:
            proc.terminate()
            try:
                proc.wait(timeout=10)
            except subprocess.TimeoutExpired:
                proc.kill()
        shutil.rmtree(root, ignore_errors=True)


def compare(baseline, current):
    print("\n%-9s %12s %12s %9s %10s" % ("endpoint", "base median", "new median",
                                         "delta", "base rps"))
    print("-" * 58)
    for name, _path, _body in ENDPOINTS:
        if name not in baseline or name not in current:
            continue
        b = baseline[name]["sequential"]["median"]
        c = current[name]["sequential"]["median"]
        br = baseline[name]["concurrent"]["rps"]
        cr = current[name]["concurrent"]["rps"]
        delta = (c - b) / b * 100.0 if b else 0.0
        print("%-9s %10.2fms %10.2fms %8.1f%% %8.1f→%.1f" %
              (name, b * 1000, c * 1000, delta, br, cr))


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--binary", default="./build/bialet")
    ap.add_argument("--requests", type=int, default=150,
                    help="sequential requests per endpoint (default 150)")
    ap.add_argument("--concurrency", type=int, default=8)
    ap.add_argument("--rounds", type=int, default=1,
                    help="repeat the whole measurement N times, keep the best median")
    ap.add_argument("--port", type=int, default=0)
    ap.add_argument("--json", dest="json_out", default=None,
                    help="write raw results to this path")
    ap.add_argument("--baseline", default=None,
                    help="compare against a previous --json run")
    args = ap.parse_args()

    best = None
    for r in range(args.rounds):
        if args.rounds > 1:
            print("round %d/%d" % (r + 1, args.rounds))
        result = run(args)
        if best is None:
            best = result
        else:
            for name, data in result.items():
                if (data["sequential"]["median"] <
                        best[name]["sequential"]["median"]):
                    best[name] = data

    if args.json_out:
        with open(args.json_out, "w", encoding="utf-8") as fh:
            json.dump(best, fh, indent=2)

    if args.baseline:
        with open(args.baseline, encoding="utf-8") as fh:
            compare(json.load(fh), best)


if __name__ == "__main__":
    main()
