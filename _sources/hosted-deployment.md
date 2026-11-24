# Hosted Deployment

[Deployment](deployment.md) explains each piece of a production install. This
page assembles them into one recipe for a **hosted** app: one host you control,
one unprivileged user, a release directory you replace wholesale, and the state
kept outside it.

## What "Hosted" Means Here

- **One host.** SQLite is a local file, so there is no database server to move
  to. Several workers here are covered in
  [Deployment](deployment.md#scaling-multiple-workers-on-one-host).
- **Immutable code.** Every deploy is a new `/srv/app/releases/<tag>/`; nothing
  is edited in place. `/srv/app/current` picks the release that should run.
- **State outside the code.** Database, WAL files, log, and key live in
  `/srv/app/shared/` and survive every swap.
- **One unprivileged user.** `app` reads the release and writes only `shared/`.
- **A restart per deploy.** Bialet resolves its app root once at startup, so
  changing `current` does nothing until the restart. That is the deploy
  boundary, the opposite of
  [No Restart Needed on Deploy](deployment.md#no-restart-needed-on-deploy); no
  watcher is needed (`-W`).

Not covered: native TLS (a proxy terminates it; [Security](security.md#tls)),
on-disk uploads (they are database rows; planned, not implemented;
[Files](file.md)), and Windows.

## Directory Layout

```text
/srv/app/
├── releases/
│   ├── v1.2.14/                 # previous release (bialet + app/)
│   └── v1.2.15/                 # current release
│       ├── bialet               # from the verified release archive
│       └── app/                 # app root: .wren files, _app/, _migration.wren
├── shared/
│   ├── app.sqlite3              # -d
│   ├── app.sqlite3-wal          # with -w
│   ├── app.sqlite3-shm
│   ├── app.log                  # -l
│   └── secret.key               # -k
└── current -> releases/v1.2.15
```

The binary sits **beside** `app/`, not inside it: the app root is also the static
root, so `GET /bialet` would serve the executable. Releases are root-owned and
read-only to the service; `shared/` is owned by `app`, mode `0700`.

## The User and the Umask

```bash
sudo useradd --system --user-group --home-dir /srv/app \
  --shell /usr/sbin/nologin app

sudo install -d -o root -g root -m 0755 /srv/app
sudo install -d -o root -g root -m 0755 /srv/app/releases
sudo install -d -o app  -g app  -m 0700 /srv/app/shared

sudo -u app sh -c 'umask 0077; openssl rand -hex 32 > /srv/app/shared/secret.key'
```

The key file holds 64 hex characters (the AES-256 key) or a passphrase, must
resolve outside the app root, and is refused at startup if it is empty, over
4 KB, or inside the tree ([Encrypted Values at Rest](security.md#encrypted-values-at-rest)).

Bialet creates a new database and a new `-l` log with mode `0600` itself, and
SQLite gives `-wal`/`-shm` the database's mode, so those do not depend on the
umask; an existing database keeps its mode. The umask covers everything else:
the key file above, directories a deploy script makes, and future files. Set it
in the deploy shell and as `UMask=0077` in the unit.

## Install a Release

The archive contains only the `bialet` binary, and `get.bialet.dev` installs it
into `~/.local/bin`; here it goes into a release directory with the app tree.

```bash
set -eu
umask 0077
VERSION=v1.2.26                  # the tag you are deploying
ARCH=linux-x86_64                # or linux-arm64
RELEASE=/srv/app/releases/$VERSION
ARCHIVE="bialet-$VERSION-$ARCH.tar.gz"
BASE="https://github.com/bialet/bialet/releases/download/$VERSION"
test -e "$RELEASE" && { echo "$RELEASE already exists"; exit 1; }
tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT
curl -fsSL "$BASE/$ARCHIVE" -o "$tmp/$ARCHIVE"
curl -fsSL "$BASE/SHA256SUMS" -o "$tmp/SHA256SUMS"
(cd "$tmp" && sha256sum --check --ignore-missing SHA256SUMS)
sudo install -d -o root -g root -m 0755 "$RELEASE"
sudo tar xzf "$tmp/$ARCHIVE" -C "$RELEASE"
sudo install -d -o root -g root -m 0755 "$RELEASE/app"
sudo rsync -a --delete \
  --exclude '_db.sqlite3*' --exclude '*.log' \
  /srv/src/app/ "$RELEASE/app/"
```

`/srv/src/app/` is wherever the app tree is built or checked out. The excludes
matter: a database or log copied into a release is a stale copy nothing updates
and it ends up in backups. See [Pitfalls](#pitfalls).

**What verification proves.** `install.sh` treats a missing `SHA256SUMS`, a
missing entry, or a mismatch as fatal. The checksum comes from the same release,
so it catches corruption, not a compromised publisher. Newer releases also carry
a build-provenance attestation: `install.sh` verifies it when `gh` is installed
and otherwise warns; `BIALET_REQUIRE_ATTESTATION=1` makes a missing or
unverifiable attestation fatal ([Installation](installation.md#releases)).

Do not start the release yet. It must pass the gate:

```bash
# Compile every .wren file the walk visits (a top-level _tests/ suite is
# checked by -T). No database is opened or created.
"$RELEASE/bialet" check "$RELEASE/app"

# If the release ships _tests/, run them. They use a temporary database.
"$RELEASE/bialet" -T "$RELEASE/app"
```

`check` exits `0` only when every visited file compiles and every backtick SQL
literal parses, `1` otherwise. The walk skips a `_tests/` directory at any
depth, because those files only compile under `-T`; an explicitly named file is
still checked. `-T` uses a throwaway database, never the production one, so it
cannot write to `shared/` ([Checking a whole app tree](usage.md#check-a-whole-app-tree),
[Testing](tests.md)).

## The Runtime Command

The unit below runs this line. The flag reference is
[Command Line Usage](usage.md#options).

```bash
/srv/app/current/bialet -p 7001 -h 127.0.0.1 -w -W -x -m 1024 -M 2048 \
  -b 1100 -u 1MB -f on -s normal -d /srv/app/shared/app.sqlite3 \
  -l /srv/app/shared/app.log -k /srv/app/shared/secret.key /srv/app/current/app
```

| Flag | Why it is here |
| --- | --- |
| `-p 7001 -h 127.0.0.1` | The proxy's upstream. The host defaults to loopback; spelling it out keeps a copied unit from publishing the port. |
| `-d`, `-l`, `-k` | Absolute paths outside the app root. A relative `-d` is joined onto the root, and a database or log inside the root must have a private name (leading `_` or `.`) or Bialet exits with status 11 ([Usage](usage.md#options)). |
| `-w` | WAL. It creates `-wal` and `-shm` beside the database, so the unit must write all of `shared/` ([SQLite WAL Mode](deployment.md#sqlite-wal-mode-production)). |
| `-W`, `-x` | No watcher; migrations and cron still run once at startup. In-memory bytecode cache, cleared by a restart and safe because the tree is immutable ([Live Reload](live-reload.md#file-watching)). |
| `-b 1100 -u 1MB -m 1024 -M 2048` | About 1 MB per upload. `-b` binds the body (the `-m`/512 ceiling is 2 MB), so the file cap is the smaller of `-u` and `-b` minus ~8 KB of framing: 1 MB here, versus ~120 KB at the defaults ([File Size Limits](file.md)). |
| `-f on -s normal` | The defaults, written out. `-s normal` keeps WAL durable across a process crash but can lose the last transactions on power loss; `-s full` narrows that at an fsync per commit. |
| `/srv/app/current/app` | The app root, resolved through the symlink once at startup. |

`-m`/`-M` are enforced on Linux only (`RLIMIT_AS` on the HTTP child); elsewhere
they only size the body cap. CPU limits and worker scaling are in
[Deployment](deployment.md#server-resource-limits).

## systemd Unit

**`/etc/systemd/system/bialet-app.service`**

```ini
[Unit]
Description=app.example.com in Bialet
After=network.target

[Service]
Type=simple
User=app
Group=app
WorkingDirectory=/srv/app/current/app
UMask=0077
ExecStart=/srv/app/current/bialet -p 7001 -h 127.0.0.1 -w -W -x -m 1024 -M 2048 -b 1100 -u 1MB -f on -s normal -d /srv/app/shared/app.sqlite3 -l /srv/app/shared/app.log -k /srv/app/shared/secret.key /srv/app/current/app
Restart=on-failure

NoNewPrivileges=yes
ProtectSystem=strict
ProtectHome=yes
PrivateTmp=yes
ReadWritePaths=/srv/app/shared

[Install]
WantedBy=multi-user.target
```

Run `sudo systemctl daemon-reload`, then
`sudo systemctl enable --now bialet-app`.

`Type=simple` is right because the supervising parent stays in the foreground;
the HTTP child is its fork. A restart that never becomes healthy is caught by
the `/_health` probe in the deploy gate.

- **`ProtectSystem=strict`** makes the filesystem read-only except `/dev`,
  `/proc`, `/sys`; without `ReadWritePaths=/srv/app/shared`, SQLite cannot create
  `-wal`/`-shm`, the database is unwritable, and `-l` cannot be opened: startup
  fails. The release tree needs no write access.
- **`PrivateTmp=yes`** gives the unit a writable `/tmp`; without it, a query that
  spills to disk fails under `ProtectSystem=strict`.
- **`ProtectHome=yes`** is safe only while nothing lives under `/home`;
  **`NoNewPrivileges=yes`** does nothing for Bialet but is worth setting.
- **`UMask=0077`** covers files the unit creates, such as a new log.

## Reverse Proxy

TLS terminates at nginx; Bialet speaks plain HTTP on loopback. The body limit
matches `-b 1100` (nginx's `k` is 1024 bytes, so `1100k` is the same number of
bytes Bialet accepts).

```nginx
server {
    listen 443 ssl;
    server_name app.example.com;

    ssl_certificate     /etc/letsencrypt/live/app.example.com/fullchain.pem;
    ssl_certificate_key /etc/letsencrypt/live/app.example.com/privkey.pem;

    # Same limit as -b 1100. Raise both together.
    client_max_body_size 1100k;
    client_body_timeout 10s;

    # Compression is the proxy's job: Bialet never compresses.
    gzip on;
    gzip_vary on;
    gzip_min_length 1024;
    gzip_comp_level 5;
    gzip_types text/css text/plain text/xml application/json
               text/javascript application/javascript application/wasm
               image/svg+xml;

    location / {
        proxy_set_header Host $host;
        proxy_set_header X-Real-IP $remote_addr;
        proxy_set_header X-Forwarded-For $proxy_add_x_forwarded_for;
        # Bialet decides the cookie Secure flag from these two.
        proxy_set_header X-Forwarded-Proto $scheme;
        proxy_set_header Forwarded "";
        proxy_pass http://127.0.0.1:7001;
    }
}
```

Bialet sends no `Cache-Control`, `ETag` or `Last-Modified` on static files and
never compresses a body: both are the proxy's job, which is why the block above
sets `gzip`. Do not compress a page that renders a secret next to reflected
input (see the compression pitfall in
[Static Files](deployment.md#static-files)). To cache too, either serve a public
asset directory from nginx — releases are immutable, so hashed asset filenames
can take a long `max-age` — or enable `proxy_cache`;
[Static Files](deployment.md#static-files) has the two shapes and the
configuration that is wrong. Do not add
`proxy_ignore_headers Cache-Control`: Bialet marks every Wren response
`no-store`, and overriding that caches sessions and error pages.

Bialet already refuses `_`/`.` names, including `/_health`, so this block can
proxy every path. For defense in depth, add the private-name deny and the
`/.well-known/` exception from
[Hardening the Proxy](deployment.md#hardening-the-proxy), which also has the
Apache and Caddy equivalents.

`Host` and `X-Forwarded-Proto` also drive `Request.baseUrl` and `Request.scheme`;
set `BIALET_BASE_URL` if links and emails need a fixed origin
([Public URLs behind a proxy](deployment.md#public-urls-behind-a-proxy)). Bialet
sends `Connection: close`, so the proxy reconnects per request: no keep-alive and
no HTTP/2 upstream.

## Health Check and Deploy Gate

```bash
curl -fsS http://127.0.0.1:7001/_health
```

The reply is `200` with `{"status":"ok",...,"database":"ok"}`, or `503` with
`"database":"error"` when `SELECT 1` fails. `curl` sends `Host: 127.0.0.1:7001`,
which is accepted. A forwarding header (`X-Real-IP`, `Forwarded`, `Via`, any
`X-Forwarded-*`) marks the request as proxied and gets `403`, so do not add one.
`curl -f` turns that `503` into a non-zero exit
([Health Endpoint](usage.md#health-endpoint)).

A deploy has three gates: `check`/`-T` on the new release, the swap and restart,
then `/_health`. If the probe fails, roll back. The swap is atomic, so `current`
never disappears:

```bash
VERSION=v1.2.26                  # the release you just installed
sudo ln -sfn "releases/$VERSION" /srv/app/.current.new
sudo mv -Tf /srv/app/.current.new /srv/app/current
sudo systemctl restart bialet-app
curl -fsS --retry 15 --retry-connrefused --retry-delay 1 \
  http://127.0.0.1:7001/_health
```

`mv -T` is GNU coreutils; `ln -sfn` alone unlinks and recreates the link, leaving
a short window with no `current`. The running process keeps serving the real path
it resolved at startup until the restart.

## Backups

The database is the whole state, uploads included. Take a consistent snapshot
while the service runs:

```bash
sudo install -d -o app -g app -m 0700 /var/backups/app
sudo -u app sqlite3 /srv/app/shared/app.sqlite3 \
  ".backup '/var/backups/app/app-$(date +%F).sqlite3'"
```

From cron, escape the `%` characters or cron eats them:

```text
17 3 * * * app sqlite3 /srv/app/shared/app.sqlite3 ".backup '/var/backups/app/app-$(date +\%F).sqlite3'"
```

`.backup` uses SQLite's online backup API and, in WAL mode, reads a consistent
snapshot that includes committed frames still in `-wal`, without blocking
requests. Do not use `cp` while the service runs: a lone `app.sqlite3` can miss
WAL writes; with the service stopped, `cp` is safe. Keep a week off the host, and
keep the key in your secret manager, *not* beside the backups: a key next to the
ciphertext defeats the encryption. Uploads need no separate step.

Restore:

```bash
sudo systemctl stop bialet-app
sudo install -o app -g app -m 600 /var/backups/app/app-2026-11-23.sqlite3 \
  /srv/app/shared/app.sqlite3
sudo rm -f /srv/app/shared/app.sqlite3-wal /srv/app/shared/app.sqlite3-shm
sudo systemctl start bialet-app
```

Removing `-wal`/`-shm` matters: a stale WAL applied to the restored file loses
data, and the explicit `-m 600` keeps the restored mode private. Test a restore
into a scratch directory first ([Backups](deployment.md#backups)).

## Upgrades and Rollback

An upgrade repeats the install section with the new tag: download, verify,
unpack, copy the tree, run `check`/`-T`, swap, restart, probe.

Back up the database **before** the restart. Migrations run at startup and are
normally one-way: pointing `current` back does not undo one. Restoring the
pre-upgrade database is the rollback for a bad migration.

Rollback is the same swap with the previous tag:

```bash
sudo ln -sfn releases/v1.2.14 /srv/app/.current.new
sudo mv -Tf /srv/app/.current.new /srv/app/current
sudo systemctl restart bialet-app
curl -fsS --retry 15 --retry-connrefused --retry-delay 1 \
  http://127.0.0.1:7001/_health
```

Keep two or three old releases and delete the rest by name, after checking which
one is live:

```bash
readlink -f /srv/app/current          # keep this one
ls -1dt /srv/app/releases/*/          # newest first
sudo rm -rf /srv/app/releases/v1.2.11 # explicit, one at a time
```

Do not prune by age: after a rollback the live release can be an older one, and
deleting it leaves `current` dangling, so the next start fails with `app
directory not found`. The binary and tree roll back together; only `shared/`
spans releases.

## Pitfalls

> ⚠️ Pitfall: a database or log inside the app root is public unless its name
> starts with `_` or `.` — Bialet exits with status 11 otherwise, and
> `.well-known` counts as public. That is why `-d`, `-l`, and `-k` are absolute
> paths into `shared/`; do not make them relative.

> ⚠️ Pitfall: a `_db.sqlite3` in a release is a stale second database: lose `-d`
> and Bialet writes there instead. The `rsync` excludes prevent it. `bialet -r`
> from a directory with no app root also creates an empty `_db.sqlite3`, and
> `make clean` deletes **every** `_db.sqlite3*` under a source checkout, so never
> keep production data under one.

> ⚠️ Pitfall: the app root is the static root too. `.wren` runs, `_`/`.` names are
> refused, and everything else is served: keep `README`s, SQL dumps, and notes
> outside it or rename them with `_` ([Private Files](security.md#private-files)).

> ⚠️ Pitfall: uploads cap at ~120 KB until `-b` and `-m` are raised together;
> `-u 8MB` alone changes nothing because the body gets `413` first. Raise `-b`,
> `-m`/`-M`, `-u`, and nginx's `client_max_body_size` as a set.

> ⚠️ Pitfall: `-l` only appends, so rotate with `copytruncate` (Bialet keeps the
> file open). A line written between the copy and the truncate can be lost; drop
> `-l` and let journald cap the output to avoid the window.

> ⚠️ Pitfall: running `sqlite3` or `bialet` as root leaves a root-owned `0600`
> database or log. The service user then cannot write it, WAL cannot be created,
> and the unit restarts until it gives up. `chown -R app:app /srv/app/shared`
> after any root-level repair, `-wal`/`-shm` included.

> ⚠️ Pitfall: do not route `/_health` to the internet. Bialet returns `403` to
> any request whose peer or `Host` is not loopback, and that check is its only
> access control; give a public liveness URL its own app route.
