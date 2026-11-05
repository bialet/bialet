#!/usr/bin/env python3
"""Validate the Wren examples on the documentation landing page with `bialet -t`.

Two kinds of example are checked:

* code blocks written out in `docs/index.rst` (``.. code-block:: wren``)
* files pulled in with `literalinclude`, which are validated in a copy of
  their own directory so imports resolve and no database lands in the docs

Examples that were copied into the page by hand drift from the real app the
moment the app changes; this is the gate that catches it.
"""

import argparse
import os
import re
import shutil
import subprocess
import sys
import tempfile


RST_BLOCK = re.compile(r"^(?P<indent>[ \t]*)\.\. code-block:: wren[ \t]*$")
RST_INCLUDE = re.compile(r"^(?P<indent>[ \t]*)\.\. literalinclude::[ \t]*(?P<path>\S+)[ \t]*$")
RST_LINES = re.compile(r"^[ \t]*:lines:[ \t]*(?P<spec>\S.*)$")
RST_OPTION = re.compile(r"^[ \t]*:[a-z-]+:")


def indented_block(lines, start, indent):
    """Return the lines of a directive body, without the directive indentation."""
    body = []
    for line in lines[start:]:
        if not line.strip():
            body.append("")
            continue
        if len(line) - len(line.lstrip()) <= len(indent):
            break
        body.append(line[len(indent):] if line.startswith(indent) else line.lstrip())
    while body and not body[-1].strip():
        body.pop()
    return body


def find_examples(path):
    """Yield (kind, lineno, payload) for each example in one source file."""
    with open(path, encoding="utf-8") as handle:
        lines = handle.read().split("\n")

    for index, line in enumerate(lines):
        block = RST_BLOCK.match(line)
        if block:
            yield "block", index + 1, indented_block(lines, index + 1, block.group("indent"))
            continue
        include = RST_INCLUDE.match(line)
        if include:
            target = os.path.normpath(
                os.path.join(os.path.dirname(path), include.group("path"))
            )
            # Options follow the directive, indented under it, until a blank
            # line or the next directive. `:lines:` decides what readers see.
            spec = None
            for option in lines[index + 1:]:
                if not option.strip():
                    break
                if not RST_OPTION.match(option):
                    break
                line_range = RST_LINES.match(option)
                if line_range:
                    spec = line_range.group("spec").strip()
            yield "include", index + 1, (target, spec)


def select_lines(text, spec):
    """Apply Sphinx's `:lines:` syntax (``1-12``, ``3-``, ``-9``, comma lists)."""
    if not spec:
        return text
    lines = text.split("\n")
    selected = []
    for part in spec.split(","):
        part = part.strip()
        if not part:
            continue
        if "-" in part:
            start, _, end = part.partition("-")
            first = int(start) if start.strip() else 1
            last = int(end) if end.strip() else len(lines)
        else:
            first = last = int(part)
        selected.extend(lines[first - 1:last])
    return "\n".join(selected)


def syntax_ok(output):
    return "Syntax OK" in output


def run(binary, app_dir, source_file, label):
    """Run `bialet -t` inside `app_dir` and return (ok, detail)."""
    result = subprocess.run(
        [binary, "-t", source_file],
        cwd=app_dir,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        text=True,
    )
    output = result.stdout
    ok = result.returncode == 0 and syntax_ok(output)
    detail = "" if ok else output.strip().splitlines()[-1] if output.strip() else "no output"
    print(f"  {'ok  ' if ok else 'FAIL'}  {label}")
    return ok, detail


def check_block(binary, docs_root, path, lineno, body):
    if not body:
        print(f"  FAIL  {path}:{lineno} empty block")
        return False, "empty block"
    with tempfile.TemporaryDirectory() as app_dir:
        with open(os.path.join(app_dir, "example.wren"), "w", encoding="utf-8") as handle:
            handle.write("\n".join(body) + "\n")
        return run(
            binary,
            app_dir,
            "example.wren",
            f"{os.path.relpath(path, docs_root)}:{lineno} (inline block)",
        )


def check_include(binary, docs_root, path, lineno, payload):
    target, spec = payload
    if not os.path.isfile(target):
        print(f"  FAIL  {path}:{lineno} missing include {target}")
        return False, f"missing include {target}"
    with tempfile.TemporaryDirectory() as app_dir:
        source_dir = os.path.dirname(target)
        app_root = os.path.join(app_dir, os.path.basename(source_dir))
        shutil.copytree(source_dir, app_root)
        with open(target, encoding="utf-8") as handle:
            selected = select_lines(handle.read(), spec)
        # Validate the slice the page actually shows, next to the sibling files
        # it imports, so the include cannot quietly select half a statement.
        with open(os.path.join(app_root, "example.wren"), "w", encoding="utf-8") as handle:
            handle.write(selected if selected.endswith("\n") else selected + "\n")
        label = os.path.relpath(target, docs_root)
        if spec:
            label += f" lines {spec}"
        return run(binary, app_root, "example.wren", f"{os.path.relpath(path, docs_root)}:{lineno} ({label})")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--binary", default="build/bialet", help="path to the bialet binary")
    parser.add_argument("--docs", default="docs", help="docs source directory")
    parser.add_argument(
        "--page",
        default="index.rst",
        help="page to check, relative to --docs (default: the landing page)",
    )
    args = parser.parse_args()

    binary = os.path.abspath(args.binary)
    if not os.path.isfile(binary):
        print(f"error: no binary at {args.binary} (run make first)", file=sys.stderr)
        return 2

    page = os.path.join(args.docs, args.page)
    if not os.path.isfile(page):
        print(f"error: no page at {page}", file=sys.stderr)
        return 2

    print(f"Checking Wren examples in {page}")
    failures = []
    checked = 0
    for kind, lineno, payload in find_examples(page):
        checked += 1
        if kind == "block":
            ok, detail = check_block(binary, args.docs, page, lineno, payload)
        else:
            ok, detail = check_include(binary, args.docs, page, lineno, payload)
        if not ok:
            failures.append((page, lineno, detail))

    if failures:
        print(f"\n{len(failures)} of {checked} example(s) failed:", file=sys.stderr)
        for path, lineno, detail in failures:
            print(f"  {path}:{lineno}: {detail}", file=sys.stderr)
        return 1

    print(f"{checked} example(s) OK")
    return 0


if __name__ == "__main__":
    sys.exit(main())
