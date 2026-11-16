#!/usr/bin/env python3
"""Minimal fallback load generator for the Bialet benchmark harness.

Only used when neither oha nor wrk is installed. It opens one TCP connection
per request (Bialet has no keep-alive), so its absolute throughput is lower
than oha's, but it is enough to keep the server busy and to compare two builds
with the same harness.

Output is JSON on stdout or in --out, with the same fields analyze.py reads
from oha. Standard library only.
"""

import argparse
import http.client
import json
import socket
import sys
import threading
import time
from urllib.parse import urlsplit


def percentile(samples, pct):
    if not samples:
        return 0.0
    ordered = sorted(samples)
    idx = int(round((pct / 100.0) * (len(ordered) - 1)))
    return ordered[max(0, min(idx, len(ordered) - 1))]


def one_request(host, port, method, path, body, ctype, timeout):
    started = time.perf_counter()
    conn = http.client.HTTPConnection(host, port, timeout=timeout)
    try:
        headers = {"Connection": "close"}
        if ctype and body is not None:
            headers["Content-Type"] = ctype
        conn.request(method, path, body=body, headers=headers)
        resp = conn.getresponse()
        resp.read()
        status = resp.status
    finally:
        conn.close()
    return status, time.perf_counter() - started


def worker(urls, method, body, ctype, deadline, timeout, lock, samples, statuses,
           errors):
    local_samples = []
    local_statuses = {}
    local_errors = 0
    while time.perf_counter() < deadline:
        target = urls[len(local_samples) % len(urls)]
        parts = urlsplit(target)
        path = parts.path or "/"
        if parts.query:
            path += "?" + parts.query
        try:
            status, elapsed = one_request(parts.hostname, parts.port or 80,
                                          method, path, body, ctype, timeout)
            local_samples.append(elapsed)
            local_statuses[status] = local_statuses.get(status, 0) + 1
        except (OSError, http.client.HTTPException):
            local_errors += 1
    with lock:
        samples.extend(local_samples)
        for status, count in local_statuses.items():
            statuses[status] = statuses.get(status, 0) + count
        errors[0] += local_errors


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("urls", nargs="+", help="absolute http:// URLs to cycle")
    ap.add_argument("--duration", type=float, default=10.0)
    ap.add_argument("--connections", type=int, default=4)
    ap.add_argument("--method", default="GET")
    ap.add_argument("--body", default=None)
    ap.add_argument("--content-type", default=None)
    ap.add_argument("--timeout", type=float, default=10.0)
    ap.add_argument("--warmup", type=float, default=0.0)
    ap.add_argument("--out", default=None)
    args = ap.parse_args()

    if args.connections < 1:
        sys.exit("--connections must be >= 1")

    for url in args.urls:
        parts = urlsplit(url)
        if parts.scheme != "http" or not parts.hostname:
            sys.exit("only absolute http:// URLs are supported: %s" % url)

    if args.warmup > 0:
        end = time.perf_counter() + args.warmup
        while time.perf_counter() < end:
            for url in args.urls:
                parts = urlsplit(url)
                path = (parts.path or "/") + (("?" + parts.query)
                                              if parts.query else "")
                try:
                    one_request(parts.hostname, parts.port or 80, args.method,
                                path, args.body, args.content_type, args.timeout)
                except (OSError, http.client.HTTPException):
                    pass

    samples = []
    statuses = {}
    errors = [0]
    lock = threading.Lock()
    started = time.perf_counter()
    deadline = started + args.duration
    threads = [
        threading.Thread(target=worker,
                         args=(args.urls, args.method, args.body,
                               args.content_type, deadline, args.timeout,
                               lock, samples, statuses, errors))
        for _ in range(args.connections)
    ]
    for thread in threads:
        thread.start()
    for thread in threads:
        thread.join()
    elapsed = max(time.perf_counter() - started, 1e-9)

    total = len(samples) + errors[0]
    result = {
        "impl": "python-fallback",
        "total": total,
        "ok": len(samples),
        "errors": errors[0],
        "rps": len(samples) / elapsed,
        "p50_ms": percentile(samples, 50) * 1000.0,
        "p90_ms": percentile(samples, 90) * 1000.0,
        "p99_ms": percentile(samples, 99) * 1000.0,
        "mean_ms": ((sum(samples) / len(samples)) * 1000.0) if samples else 0.0,
        "status_codes": {str(k): v for k, v in sorted(statuses.items())},
        "urls": args.urls,
        "duration_s": elapsed,
    }
    text = json.dumps(result, indent=2)
    if args.out:
        with open(args.out, "w", encoding="utf-8") as handle:
            handle.write(text + "\n")
    else:
        print(text)


if __name__ == "__main__":
    try:
        main()
    except KeyboardInterrupt:
        sys.exit(130)
