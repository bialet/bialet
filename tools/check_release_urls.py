#!/usr/bin/env python3
"""Check that every release download link in the built documentation resolves.

The site embeds links of the form

    https://github.com/bialet/bialet/releases/download/vX.Y.Z/<asset>

built by `docs/conf.py` from `BIALET_VERSION`. Those URLs only work once the
release asset exists, so publishing the site before the tag's build finishes
ships dead download links.

The links are read out of the rendered HTML rather than rebuilt here, so a
mismatch between the version in `src/bialet.h` and the asset names produced by
`.github/workflows/release.yml` is caught too.

A manual run of the Documentation workflow passes `--warn-only`: right after a
version bump the tag may not exist yet, and that must not block the deploy. The
release workflow passes no flag, so a missing asset fails the job.
"""

import argparse
import os
import re
import socket
import sys
import time
import urllib.error
import urllib.request


RELEASE_LINK = re.compile(
    r'https://github\.com/bialet/bialet/releases/download/[^\s"\'<>)\\]+'
)
# `releases/latest` needs no check: it resolves to whatever is published.
ATTEMPTS = 3
TIMEOUT = 30


def links_in(path):
    found = set()
    for root, _dirs, files in os.walk(path):
        for name in files:
            if not name.endswith('.html'):
                continue
            with open(os.path.join(root, name), encoding='utf-8') as f:
                found.update(RELEASE_LINK.findall(f.read()))
    return sorted(found)


def resolves(url):
    """True when the asset answers, following GitHub's redirect to its CDN."""
    request = urllib.request.Request(url, method='HEAD')
    for attempt in range(1, ATTEMPTS + 1):
        try:
            with urllib.request.urlopen(request, timeout=TIMEOUT) as response:
                return 200 <= response.status < 300
        except urllib.error.HTTPError as error:
            # Some CDN edges reject HEAD on a signed URL but serve GET.
            if error.code in (403, 405, 501):
                get = urllib.request.Request(url, headers={'Range': 'bytes=0-0'})
                try:
                    with urllib.request.urlopen(get, timeout=TIMEOUT) as response:
                        return 200 <= response.status < 300
                except urllib.error.HTTPError:
                    pass
            if error.code != 404 or attempt == ATTEMPTS:
                return False
        except (urllib.error.URLError, socket.timeout):
            if attempt == ATTEMPTS:
                return False
        time.sleep(2 * attempt)
    return False


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('site', help='built HTML directory, e.g. build/html')
    parser.add_argument(
        '--warn-only',
        action='store_true',
        help='report missing assets as warnings instead of failing',
    )
    args = parser.parse_args()

    if not os.path.isdir(args.site):
        print(f'error: {args.site} is not a directory', file=sys.stderr)
        return 2

    urls = links_in(args.site)
    if not urls:
        print(f'warning: no release download links found in {args.site}')
        return 0

    missing = []
    for url in urls:
        if resolves(url):
            print(f'ok    {url}')
        else:
            missing.append(url)
            print(f'MISSING {url}')

    print(f'{len(urls) - len(missing)} of {len(urls)} release links resolve')
    if missing:
        # Annotations, so the failure points at the run's summary page.
        for url in missing:
            marker = 'warning' if args.warn_only else 'error'
            print(f'::{marker}::release asset not found: {url}')
        if args.warn_only:
            print('not failing the deploy (--warn-only)')
            return 0
        return 1
    return 0


if __name__ == '__main__':
    sys.exit(main())
