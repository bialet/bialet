---
name: release
description: >-
  Cuts a Bialet release end to end: preflight, semver decision, CHANGELOG and
  BIALET_VERSION bump, the local test gate on macOS (make check, check-mingw),
  a scoped security audit of the diff since the last tag, merge to main,
  annotated vX.Y.Z tag, verification of the GitHub release workflow (assets,
  SHA256SUMS, Homebrew, Docker, PPA, docs), local `make install`, and syncing
  the bialet-app-builder skill in the sibling ../skills repo. Use only in the
  bialet/bialet repository, when the user asks to release, cut or publish a
  version, bump BIALET_VERSION, or tag a new release. Do not use for CHANGELOG
  edits alone, for feature work, or in any other repository.
metadata:
  repo: bialet/bialet
  requires: git, gh, make, a C toolchain
  optional: docker (crossplatform mirror), mingw-w64 (Windows syntax gate)
---

# Release Bialet

Execute the phases in order. Each phase has a pass condition; do not enter the
next one until it holds.

Commands assume the repository root and a POSIX shell. Keep `VERSION` in the
shape `1.2.30` (no `v`) and `PREV` at the last released tag.

## Hard rules

1. Never bump, commit, tag, or push before the user explicitly confirms the
   target version. Propose the version, show the semver reasoning, wait.
2. `vX.Y.Z` must equal `BIALET_VERSION`, and the tagged commit must already be
   an ancestor of `origin/main`. The `Guard` job in
   `.github/workflows/release.yml` enforces both and fails the run otherwise.
3. Never force-push. Never move, delete, or re-create a published tag or
   release without explicit approval. A wrong tag means a new patch version.
4. Stop on red tests. Stop on a confirmed high or critical security finding.
   The only way past a red job is a rerun that proves the runner caused it.
5. Never release from a dirty worktree. A stale `_db.sqlite3` is gitignored but
   breaks tests; `make clean` and `find . -name "_db.sqlite3*"` are your
   friends when one fails for no obvious reason.
6. Any subagent used along the way never commits, tags, or pushes. You do.

## Phase 0 — Preflight

```bash
git fetch origin --tags --prune
git status --short
git branch --show-current
git log --oneline -1
git rev-parse --short main origin/main
git describe --tags --abbrev=0
git tag --sort=-v:refname | head -5
gh auth status
```

Pass condition:

- `git status --short` prints nothing.
- You are on `dev` (feature work happens there) or `main`, and you know how far
  the branch is ahead of `origin/main` (`git log --oneline origin/main..HEAD`).
- `PREV=$(git describe --tags --abbrev=0)` is the last release.
- No tag for the target version exists yet (`git tag -l "v$VERSION"` is empty);
  an existing tag is a stop.
- `gh auth status` reports a working `github.com` login with `repo` scope.

The release commit is authored on `dev` but must land on `main`. Record the
merge path you will use (fast-forward preferred) and confirm it with the user
before pushing anything.

## Phase 1 — Version decision

```bash
grep -n 'BIALET_VERSION' src/bialet.h
PREV=$(git describe --tags --abbrev=0)
git log --oneline "$PREV"..HEAD
git diff --stat "$PREV"..HEAD
```

Pick the bump from what the diff actually does:

| Bump  | Trigger                                                             |
| ----- | ------------------------------------------------------------------- |
| major | Breaking change to Wren API, CLI flags, HTTP/DB behaviour, or the file layout. Requires explicit approval. |
| minor | New user-facing feature, new Wren API surface, new CLI flag.        |
| patch | Fix, docs, performance, CI, dependency bump, internal refactor.     |

Show the user the version, the bump class, and the two or three commits that
justify it, then wait for confirmation.

Require full CHANGELOG coverage before bumping: every user-visible change since
`PREV` needs a bullet. Walk `git log "$PREV"..HEAD` commit by commit and match
each one to a line. Internal refactors that change nothing observable stay out.

Apply the bump:

1. `src/bialet.h`: change the `#define BIALET_VERSION "X.Y.Z"` string only.
2. `CHANGELOG.md`: insert a new section directly under the intro paragraph,
   collecting the entries:
   `## X.Y.Z — YYYY-MM-DD` (em dash U+2014, ISO date, today).
   Subsections in this order, only the ones with content: `### Added`,
   `### Changed`, `### Fixed`, `### Removed`.
3. `docs/conf.py` needs no edit: it reads the version from `src/bialet.h` and
   the date from the newest `##` heading in `CHANGELOG.md`.
4. Sweep `docs/` for stale version strings and decide each one:

   ```bash
   grep -rn 'v[0-9]\+\.[0-9]\+\.[0-9]\+' docs/ | grep -v conf.py
   ```

   `docs/` derives the version at build time, so leave illustrative paths
   alone: the `v1.2.x` release directories in `docs/hosted-deployment.md` are an
   example layout, not a claim about the current release. Update only a string
   that would be wrong for the reader, such as prose that names the version
   being installed today.

```bash
git add src/bialet.h CHANGELOG.md docs
git commit -m "chore(release): bump to X.Y.Z"
git show --stat HEAD
```

Pass condition: one commit, `chore(release): bump to X.Y.Z`, touching only the
version and the changelog (plus any stale doc string). Do not tag yet.

## Phase 2 — Tests on every platform

Run locally on macOS:

```bash
make clean && make
make check
make check-mingw
```

`make check` runs `check-wren-inc` (embedded Wren `.inc` files match a fresh
regeneration), `tests/run.sh`, `bialet -T tests/`, and `check-examples`.
`make check-mingw` compiles every source with `-fsyntax-only` for `_WIN32` and
skips cleanly when MinGW or the SQLite/curl/OpenSSL target headers are absent.

Optional Linux x86_64 mirror, only if Docker is running:

```bash
docker info > /dev/null
tools/crosscompile.sh linux
```

That produces `build/bialet` for Linux in an `ubuntu:22.04` container. Run the
suite there only if the image has the test dependencies; a build-only pass is
not a substitute for the CI suite.

Honest matrix:

| Platform              | Where it really runs                                  |
| --------------------- | ----------------------------------------------------- |
| macOS arm64 (host)    | local `make check` above, plus CI                     |
| Linux x86_64          | CI `build-linux` + `smoke-test-linux` on Ubuntu 24.04 |
| Linux arm64           | CI build only, no suite                               |
| macOS arm64 (runner)  | CI suite                                              |
| Windows x86_64 (Wine) | CI cross-compile + suite under Wine                   |

The authoritative runs are the workflow jobs. The `release` job `needs` every
platform job, so a red suite blocks publication before any asset is published.
The tag, however, is already pushed by then, which is why the local gate comes
first: a broken build caught here costs a reset, not a patch release.

Known-environmental, not waivers: `tests/run.sh` skips two tests on a `.exe`
target (nested relative import, `folder.wren` symlink) because Wine does not
reproduce them, and the macOS runner sets `BIALET_SKIP_CRON_RELOAD_TEST=1`
because FSEvents delivery is flaky there. Both are printed as skipped. Any
other skip or failure is a real problem.

## Phase 3 — Security audit of the diff

Load the `security-audit` skill (`skill` tool, id `security-audit`) in
guidance/scoped mode: review, not a full codebase audit. Run it against the
release commit, so the scope is exactly:

```bash
git diff "$PREV"..HEAD
```

Rules for this pass:

- Guidance and review of this diff only. Not a full audit, no report artifacts,
  no `AUDIT.md`, unless the user asks for a full audit.
- Every candidate finding needs a boundary (where untrusted input enters), the
  affected principal and resource, and a concrete result (what an attacker
  gains). A checklist item with no boundary is not a finding.
- Report confirmed findings with severity; medium and low get a recommendation,
  not a blocked release.

High-risk areas in this codebase, check each one the diff touches:

- Path resolution and the static-file handler in `src/server.c` (traversal,
  `..`, encoded separators, symlinks).
- `realpath()` containment, and the denial of `_`/`.` prefixed files and
  directories (must be 403/404, never a served file).
- Routing, including `.wren` handlers resolved from a folder name and the
  deepest-match walk.
- SQL construction from Wren (backtick-delimited statements with `?`
  placeholders are prepared; concatenated strings are not) and anything
  interpolated into a statement.
- Session and cookie handling (flags, signing, rotation, fixation).
- Outbound `Http` calls (URL and header injection, redirect and TLS handling).
- Uploads and multipart parsing (size limits, filename handling, temp paths).
- Response headers (injection through reflected values, missing `nosniff`).
- `--secret-key-file` handling and what may end up in logs or dumps.
- TLS and proxy assumptions documented in `docs/deployment.md`.

Pass condition: no confirmed high or critical finding. Everything else is
written into the final report with severity and a recommendation.

## Phase 4 — Land on `main` and tag

```bash
git fetch origin --tags --prune
git switch dev && git status --short
git switch main
git pull --ff-only origin main
git merge --ff-only dev
```

If `--ff-only` refuses, run a real merge, inspect `git log --oneline -5` and
`git diff --stat origin/main`, then continue. Then, after explicit
confirmation:

```bash
git push origin main
VERSION=X.Y.Z
git tag -a "v$VERSION" -m "bialet $VERSION"
git push origin "v$VERSION"
git cat-file -t "v$VERSION"     # tag
git tag -n9 "v$VERSION"         # bialet X.Y.Z
```

Pass condition: `origin/main` contains the bump commit, the tag is annotated
(`cat-file -t` prints `tag`), and its message is `bialet X.Y.Z`.

## Phase 5 — Verify the GitHub release

```bash
gh run list --workflow=release.yml --limit 5
RUN=$(gh run list --workflow=release.yml --limit 1 --json databaseId --jq '.[0].databaseId')
gh run watch "$RUN" --exit-status --compact
gh run view "$RUN" --json jobs --jq '.jobs[] | "\(.name): \(.conclusion)"'
```

Every job must be `success`, and these must all appear:

- `Lockfile / Verify actions.lock`
- `Guard`
- `Linux x86_64`
- `Smoke test Linux (24.04)`
- `Linux arm64`
- `macOS arm64`
- `Windows x86_64`
- `Docker image`
- `Create GitHub Release`
- `PPA / upload (<distro>)`, one per supported Ubuntu release
- `Documentation / build` and `Documentation / deploy`

`PPA / lockfile` reports `skipped` on a tag push by design: it only runs on
`workflow_dispatch`. For a prerelease tag the PPA upload jobs are skipped too.

Assets:

```bash
gh release view "v$VERSION" --json tagName,isDraft,isPrerelease,assets \
  --jq '{tag: .tagName, draft: .isDraft, prerelease: .isPrerelease, assets: [.assets[].name]}'
```

Expect exactly five entries, no draft:

- `bialet-vX.Y.Z-linux-x86_64.tar.gz`
- `bialet-vX.Y.Z-linux-arm64.tar.gz`
- `bialet-vX.Y.Z-macos-arm64.tar.gz`
- `bialet-vX.Y.Z-windows-x86_64.zip`
- `SHA256SUMS`

Checksums, verified locally:

```bash
D=$(mktemp -d)
gh release download "v$VERSION" --dir "$D"
(cd "$D" && shasum -a 256 -c SHA256SUMS)
rm -rf "$D"
```

Homebrew. The formula bump is `continue-on-error`, so it can be silently
missing; check it explicitly. The formula lives at the root of
`bialet/homebrew-bialet`.

```bash
gh api repos/bialet/homebrew-bialet/contents/bialet.rb \
  -H "Accept: application/vnd.github.raw" | grep -E 'url "|sha256 "'
```

Both the `url` (which must end in `refs/tags/v$VERSION.tar.gz`) and the
`sha256` must be the new release.

Docker:

```bash
docker buildx imagetools inspect "bialet/bialet:$VERSION" | head -3
docker buildx imagetools inspect bialet/bialet:latest | head -3
```

The second only for a stable release; a prerelease must not move `latest`.

The `Documentation` jobs link the release assets with `verify_links`, so a
missing archive fails them instead of publishing a 404. Docs are not deployed
by a push to `main`: after later `docs/` edits, run the Documentation workflow
by hand.

On failure:

```bash
gh run view "$RUN" --log-failed
```

- External cause (flaky runner, expired secret, network): fix the cause, then
  `gh run rerun "$RUN" --failed`.
- Code defect: the tag is already public, so the fix ships as a new patch
  version through the same phases. Do not reuse the version.
- Re-creating a published tag or release needs explicit approval.

## Phase 6 — Local install and local skills

```bash
make install          # atomic swap into ~/.local/bin
command -v bialet     # must resolve to ~/.local/bin/bialet, not Homebrew's
bialet -v             # must print: bialet X.Y.Z
make installcheck     # optional: installs, then runs the suite against it
```

Then sync the companion skills repo, which is a sibling checkout at `../skills`
(remote `git@github.com:bialet/skills.git`):

```bash
cd ../skills
git fetch origin --tags
git pull --ff-only
git log --oneline -5
```

Read the new `CHANGELOG.md` entries for this release and update
`skills/bialet-app-builder/SKILL.md` with anything user-facing: new Wren API,
changed behaviour, new pitfalls, removed features. Keep that file's voice and
structure.

```bash
git add skills/bialet-app-builder/SKILL.md
git commit -m "skill: sync bialet-app-builder with Bialet X.Y.Z"
git push origin main
```

Commit and push that repository separately; nothing from it goes into this one.

Refresh the globally installed copy and prove it matches:

```bash
npx skills update bialet-app-builder -g -y
diff -r skills/bialet-app-builder ~/.agents/skills/bialet-app-builder
```

`diff` prints nothing on a match. If `update` cannot find the skill, reinstall:

```bash
npx skills add bialet/skills --skill bialet-app-builder -g -y
```

The global copy is symlinked into the agent directories (Claude Code, Copilot,
OpenCode, Zed), so no extra step is needed there.

## Checklist

| Phase | Pass condition                                                     |
| ----- | ------------------------------------------------------------------ |
| 0     | Clean tree, on `dev`, no tag for the target version, `gh` logged in |
| 1     | `chore(release): bump to X.Y.Z` commit; version, changelog, docs agree |
| 2     | `make check` and `make check-mingw` green on macOS                  |
| 3     | No confirmed high or critical finding in `git diff PREV..HEAD`      |
| 4     | Bump commit on `origin/main`; annotated `vX.Y.Z` pushed             |
| 5     | All workflow jobs `success`; 4 archives + `SHA256SUMS` verified     |
| 6     | `bialet -v` prints X.Y.Z; `../skills` pushed; installed copy matches |

## Do not

- Bump, commit, tag, or push without a confirmed version.
- Tag a commit that is not on `origin/main`, or tag before `main` is pushed.
- Force-push, or move or delete an existing tag or release.
- Tag before the local suite is green.
- Release with a dirty worktree.
- Keep a release blocked on a medium/low security finding that the user has
  accepted in writing.
- Commit the `../skills` change into this repository.
