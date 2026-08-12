# Deployment

Bialet is a single binary — copy it to your server alongside your `.wren`
files and you're deployed. This guide covers setting up a production-ready
environment with a reverse proxy, systemd, and Docker.

## Why a Reverse Proxy?

Bialet speaks **HTTP/1.0** and does not support HTTPS natively. Running it
behind a reverse proxy (nginx, Apache, or Caddy) gives you:

- **TLS termination** — HTTPS for your users
- **HTTP/1.1 or HTTP/2** between the client and the proxy
- **Static file caching and gzip** — the proxy can compress and cache assets
- **Security** — Bialet only needs to listen on `127.0.0.1`, not the public
  internet

You *can* expose Bialet directly with `-h 0.0.0.0`, but this is not
recommended for production.

Bialet is **single-threaded**: it accepts and serves one connection at a time,
in a blocking loop. The reverse proxy is therefore not just for TLS — it is
the layer that absorbs slow clients, connection churn, and request buffering.
Treat the proxy as part of your security posture, not a convenience (see
[Security](security.md) and the hardening section below).

## No Restart Needed on Deploy

When you push new code to your server, the Bialet server **does not need to
be restarted**. It watches `.wren` files for changes and picks them up
automatically.

### Auto-Migration

If your app has a `_migration.wren` file (or `/_app/migration.wren`), any
pending database migrations run automatically **each time the migration file
is created or modified**. Just deploy the new file and Bialet runs the
migrations before serving the next request. No manual steps, no downtime.

This means your deploy flow is:

```bash
scp -r ./src/* user@your-server:/www/your-app/
# Done. Migrations run, code reloads, server keeps running.
```

## Static Files

Bialet serves static files (CSS, JS, images, fonts) directly. For better
performance, configure your reverse proxy to:

- **Cache** static assets (add `Cache-Control` headers)
- **Gzip** or **brotli** compress text-based assets (CSS, JS, HTML, SVG)

It's also safe to serve static files from the proxy directly instead of
proxying to Bialet — just add a `location` block pointing to your app
directory.

## Reverse Proxy Setups

### nginx

```nginx
server {
    listen 80;
    listen [::]:80;
    server_name example.com;

    # Serve static files directly from disk (optional, faster)
    root /www/example.com;
    location / {
        # Try static file first, fall back to Bialet
        try_files $uri @bialet;
    }

    # Gzip static assets
    location ~* \.(css|js|svg|woff2|png|jpg|jpeg|gif|ico|html)$ {
        expires 7d;
        add_header Cache-Control "public, immutable";
        try_files $uri =404;
    }

    location @bialet {
        proxy_set_header Host $host;
        proxy_set_header X-Real-IP $remote_addr;
        proxy_set_header X-Forwarded-For $proxy_add_x_forwarded_for;
        proxy_set_header X-Forwarded-Proto $scheme;
        proxy_pass http://127.0.0.1:7001;
    }
}
```

For Let's Encrypt TLS, wrap with:

```nginx
server {
    listen 443 ssl;
    listen [::]:443 ssl;
    server_name example.com;

    ssl_certificate     /etc/letsencrypt/live/example.com/fullchain.pem;
    ssl_certificate_key /etc/letsencrypt/live/example.com/privkey.pem;

    # ... same location blocks as above ...
}

server {
    listen 80;
    listen [::]:80;
    server_name example.com;
    return 301 https://$host$request_uri;
}
```

### Apache

Enable the required modules:

```bash
sudo a2enmod proxy proxy_http rewrite headers expires
```

Virtual host configuration:

```apache
<VirtualHost *:80>
    ServerName example.com

    # Gzip
    <IfModule mod_deflate.c>
        AddOutputFilterByType DEFLATE text/html text/css text/javascript
        AddOutputFilterByType DEFLATE application/javascript application/json
        AddOutputFilterByType DEFLATE image/svg+xml
    </IfModule>

    # Cache static assets
    <IfModule mod_expires.c>
        ExpiresActive On
        ExpiresByType text/css "access plus 7 days"
        ExpiresByType text/javascript "access plus 7 days"
        ExpiresByType image/png "access plus 7 days"
        ExpiresByType image/jpeg "access plus 7 days"
        ExpiresByType image/svg+xml "access plus 7 days"
        ExpiresByType font/woff2 "access plus 30 days"
    </IfModule>

    # Serve static files directly
    DocumentRoot /www/example.com

    RewriteEngine On
    # If the file exists, serve it
    RewriteCond %{DOCUMENT_ROOT}%{REQUEST_URI} -f
    RewriteRule ^ - [L]
    # Otherwise proxy to Bialet
    RewriteRule ^/(.*)$ http://127.0.0.1:7001/$1 [P,L]

    ProxyPreserveHost On
    ProxyPassReverse / http://127.0.0.1:7001/
</VirtualHost>
```

### Caddy

Caddy is the simplest option — it handles TLS automatically:

```
example.com {
    encode gzip zstd

    @static {
        file
        path *.css *.js *.svg *.woff2 *.png *.jpg *.jpeg *.gif *.ico *.html
    }
    header @static Cache-Control "public, max-age=604800, immutable"

    reverse_proxy 127.0.0.1:7001
}
```

That's it. Caddy obtains and renews certificates automatically.

## Hardening the Proxy

The proxy is your first line of defense. Because Bialet is single-threaded
and blocking, a single slow or hostile client can stall every other request
unless the proxy caps body size, sets read deadlines, and buffers requests.

Key directives, regardless of which proxy you run:

- **Cap the request body.** Bialet rejects request bodies larger than the
  `-b` limit (default 128 KB) with `413 Payload Too Large` *before* parsing
  them, so a body can never stall the Wren runtime. The effective cap is the
  smaller of `-b` and a memory-safe ceiling (`-m` / 512) so a body can never
  push the fork()ed child past its `RLIMIT_AS` budget — at the default 50 MB
  soft limit that ceiling is 100 KB, so `-b 128` is fully effective once the
  soft limit reaches 64 MB. Set the proxy's body limit to the smallest your
  app needs — 1 MB is a sane default for the proxy.
- **Set a body-read deadline.** Bialet's 5-second socket timeout is per
  `recv()` call, so a peer that dribbles bytes slowly can keep a connection
  open indefinitely. Make the proxy enforce a *total* read timeout and reject
  clients that stall mid-body.
- **Buffer the body at the proxy.** Read the full request before forwarding
  it, so the upstream socket is never held open by a slow client.
- **Bind Bialet to `127.0.0.1`** and only expose the proxy to the internet.
- **Deny private files at the proxy too.** Bialet already returns 403 for
  `_`/`.`-prefixed files, but blocking them at the proxy adds a second layer
  in case an application or symlink ever serves one (see [Security](security.md)).

### nginx

```nginx
server {
    listen 80;
    listen [::]:80;
    server_name example.com;

    # Cap body size and enforce a total read deadline.
    client_max_body_size 1m;
    client_body_timeout 10s;

    # Deny private files at the proxy (defense in depth).
    location ~ (^|/)[_.] {
        return 403;
    }

    # Serve static files directly from disk (optional, faster)
    root /www/example.com;
    location / {
        # Try static file first, fall back to Bialet
        try_files $uri @bialet;
    }

    # Gzip static assets
    location ~* \.(css|js|svg|woff2|png|jpg|jpeg|gif|ico|html)$ {
        expires 7d;
        add_header Cache-Control "public, immutable";
        try_files $uri =404;
    }

    location @bialet {
        proxy_request_buffering on;
        proxy_set_header Host $host;
        proxy_set_header X-Real-IP $remote_addr;
        proxy_set_header X-Forwarded-For $proxy_add_x_forwarded_for;
        proxy_set_header X-Forwarded-Proto $scheme;
        proxy_pass http://127.0.0.1:7001;
    }
}
```

Note: the `location ~ (^|/)[_.]` block matches any path segment that starts
with `_` or `.`, which covers `_db.sqlite3`, `_route.wren`, and dotfiles such
as `.env`. If your app legitimately serves files with such names, drop this
block — but prefer renaming those files.

### Apache

```apache
<VirtualHost *:80>
    ServerName example.com

    # Cap body size (bytes) and set a total read timeout (needs mod_reqtimeout).
    LimitRequestBody 1048576
    RequestReadTimeout header=20-40,MinRate=500 body=30,MinRate=500

    # Gzip
    <IfModule mod_deflate.c>
        AddOutputFilterByType DEFLATE text/html text/css text/javascript
        AddOutputFilterByType DEFLATE application/javascript application/json
        AddOutputFilterByType DEFLATE image/svg+xml
    </IfModule>

    # Cache static assets
    <IfModule mod_expires.c>
        ExpiresActive On
        ExpiresByType text/css "access plus 7 days"
        ExpiresByType text/javascript "access plus 7 days"
        ExpiresByType image/png "access plus 7 days"
        ExpiresByType image/jpeg "access plus 7 days"
        ExpiresByType image/svg+xml "access plus 7 days"
        ExpiresByType font/woff2 "access plus 30 days"
    </IfModule>

    # Deny private files at the proxy (defense in depth).
    RewriteRule ^(.*)(^|/)[_.][^/]*$ - [F,L]

    # Serve static files directly
    DocumentRoot /www/example.com

    RewriteEngine On
    # If the file exists, serve it
    RewriteCond %{DOCUMENT_ROOT}%{REQUEST_URI} -f
    RewriteRule ^ - [L]
    # Otherwise proxy to Bialet
    RewriteRule ^/(.*)$ http://127.0.0.1:7001/$1 [P,L]

    ProxyPreserveHost On
    ProxyPassReverse / http://127.0.0.1:7001/
</VirtualHost>
```

`RequestReadTimeout` is provided by `mod_reqtimeout`, enabled with
`a2enmod reqtimeout`. The `MinRate` clause stops a slow-drip client that would
otherwise never trip the absolute timeout.

### Caddy

Caddy does not have a per-location body-size directive, but you can cap
request bodies with a request matcher:

```
example.com {
    encode gzip zstd

    @static {
        file
        path *.css *.js *.svg *.woff2 *.png *.jpg *.jpeg *.gif *.ico *.html
    }
    header @static Cache-Control "public, max-age=604800, immutable"

    @private {
        path_regexp private (^|/)[_.]
    }
    respond @private 403

    reverse_proxy 127.0.0.1:7001 {
        flush_interval -1
    }
}
```

Caddy reads request bodies before proxying by default. For a total body
deadline, front Caddy with a timeout-capable proxy or enforce it at the app
level.

## Cloudflare Tunnel (Alternative)

If you do not control a server or a domain, [Cloudflare Tunnel](https://developers.cloudflare.com/cloudflare-one/connections/connect-networks/)
exposes a local Bialet server over a public HTTPS URL **without opening a
port, configuring a proxy, or buying DNS**. `cloudflared` keeps an outbound
connection to Cloudflare's edge; requests arrive at your machine through that
tunnel. Cloudflare terminates TLS and buffers slow clients, which covers most
of what a reverse proxy does.

Install `cloudflared` ([official packages](https://developers.cloudflare.com/cloudflare-one/connections/connect-networks/downloads/)):

```bash
sudo apt-get install -y cloudflared
```

Start Bialet bound to localhost, then point a quick tunnel at it:

```bash
bialet -p 7001 /www/myapp
cloudflared tunnel --url http://127.0.0.1:7001
```

`cloudflared` prints a `https://<random>.trycloudflare.com` URL. Open it and
your app is live. No account, no configuration.

> **Pitfall: the URL is temporary.** A quick tunnel dies when `cloudflared`
> stops, and the next run gets a **new** random hostname. Fine for demos and
> previews; useless as a stable endpoint.

For a stable hostname you need a [named tunnel](https://developers.cloudflare.com/cloudflare-one/connections/connect-networks/get-started/create-remote-tunnel/),
which requires a Cloudflare account and a domain on their DNS:

```bash
cloudflared tunnel login
cloudflared tunnel create myapp
cloudflared tunnel route dns myapp app.example.com
cloudflared tunnel run --url http://127.0.0.1:7001 myapp
```

A named tunnel persists across restarts and can run as a systemd unit with
`cloudflared service install`.

Security notes that still apply:

- **Bind Bialet to `127.0.0.1`.** The tunnel forwards to localhost; do not
  add `-h 0.0.0.0`. Only Cloudflare's edge ever talks to your server.
- **Cap the request body.** Cloudflare does not enforce a body-size limit for
  you. Keep Bialet's `-b` default (128 KB) or lower it.
- **Private files are still private.** Bialet returns 403 for `_`/`.`
  prefixed files; a tunnel does not change that.

## Running Bialet as a Service

### systemd (Ubuntu / Debian)

Create a unit file to keep Bialet running and auto-start on boot.

First, create a group service (optional — useful if you run multiple apps):

**`/etc/systemd/system/bialet.service`**

```ini
[Unit]
Description=Bialet group

[Service]
Type=oneshot
ExecStart=/bin/true
RemainAfterExit=yes

[Install]
WantedBy=multi-user.target
```

Then create one unit per app:

**`/etc/systemd/system/example.com.service`**

```ini
[Unit]
Description=example.com in Bialet
PartOf=bialet.service
After=bialet.service

[Service]
User=root
WorkingDirectory=/www/example.com
ExecStart=bialet -p 7001 -m 1024 -M 2048 -l /var/log/bialet/example.com.log -d /www/example.com.sqlite /www/example.com
Restart=on-failure

[Install]
WantedBy=bialet.service
```

Enable and start:

```bash
sudo mkdir -p /var/log/bialet /www/example.com
sudo systemctl daemon-reload
sudo systemctl enable bialet.service example.com.service
sudo systemctl start bialet.service example.com.service
```

### Multiple Apps on One Server

Run each app on a different port and proxy by hostname:

```bash
bialet -p 7001 /www/site-a     # proxied from site-a.com
bialet -p 7002 /www/site-b     # proxied from site-b.com
```

Point each nginx `proxy_pass` / Apache `ProxyPass` / Caddy `reverse_proxy` to
the matching port.

## Docker

Use the [Bialet Skeleton](https://github.com/bialet/skeleton) as a starting
point:

```bash
git clone --depth 1 https://github.com/bialet/skeleton.git myapp
cd myapp
```

The skeleton includes a `Dockerfile` and `compose.yml`. Build and run:

```bash
docker compose up -d
```

### Custom Port and App Directory

```bash
BIALET_PORT=8080 BIALET_DIR=/app/mysite docker compose up -d
```

### Dockerfile (from scratch)

If you prefer a minimal setup:

```dockerfile
FROM ubuntu:22.04

RUN apt-get update && apt-get install -y libsqlite3-0 libcurl4 && \
    rm -rf /var/lib/apt/lists/*

COPY bialet /usr/local/bin/bialet
COPY . /app
WORKDIR /app

EXPOSE 7001
CMD ["bialet", "-p", "7001", "/app"]
```

Build:

```bash
docker build -t myapp .
docker run -d -p 127.0.0.1:7001:7001 -v ./data:/www/myapp.sqlite myapp
```

### Docker Compose

```yaml
services:
  app:
    image: myapp
    ports:
      - "127.0.0.1:7001:7001"
    volumes:
      - ./data:/www/myapp.sqlite
    restart: unless-stopped
```

(sqlite-wal-mode-production)=
## SQLite WAL Mode (Production)

Enable WAL with the `-w` flag:

```bash
bialet -w -p 7001 /www/myapp
```

### What Is WAL?

WAL (Write-Ahead Logging) is an alternative journal mode for SQLite.
Instead of writing changes directly to the main database file, SQLite
appends them to a separate `-wal` file first, then periodically merges
them back (a process called *checkpointing*).

### Why It Matters for a Web Server

By default, SQLite uses a *rollback journal*. In that mode, **readers block
writers and writers block readers**. On a web server handling concurrent
requests, this means:

- A page loading data (`SELECT`) blocks a form submission (`INSERT`)
- Two simultaneous form submissions queue up behind each other
- Under any real traffic, you get `SQLITE_BUSY` errors

WAL fixes this by decoupling reads from writes:

|                     | Rollback Journal (default) | WAL mode (`-w`)     |
|---------------------|---------------------------|---------------------|
| Readers vs. writers | Block each other          | Work concurrently   |
| Multiple readers    | OK                        | OK                  |
| Multiple writers    | One at a time (queued)    | One at a time (queued) |
| Write performance   | Baseline                  | Often faster        |

In practice, this means a user submitting a comment form won't stall another
user browsing the page, and your cron tasks (which may write to the DB) won't
interfere with normal traffic.

### Disk Footprint

WAL mode creates two extra files alongside your database:

- `_db.sqlite3-wal` — the write-ahead log (grows during writes)
- `_db.sqlite3-shm` — shared memory index (small, fixed size)

The WAL file shrinks automatically on checkpoint. For most apps, it stays
under a few megabytes.

### When You Can Skip It

WAL is not strictly necessary if:

- Your app is read-only or single-user
- Traffic is extremely low (a handful of requests per minute)
- You're prototyping locally

For anything serving real users, enable it with `-w`. The overhead is
negligible and the concurrency benefit is immediate.

## Server Resource Limits

Use CLI flags to constrain resources per app:

| Flag | Description            | Default | Example          |
|------|------------------------|---------|------------------|
| `-m` | Soft memory limit (MB) | 50      | `-m 1024`        |
| `-M` | Hard memory limit (MB) | 100     | `-M 2048`        |
| `-c` | Soft CPU limit (%)     | 15      | `-c 25`          |
| `-C` | Hard CPU limit (%)     | 30      | `-C 50`          |
| `-b` | Max request body (KB)  | 128     | `-b 512`         |
| `-w` | SQLite WAL mode        | off     | `-w`             |

The request-body cap is the smaller of `-b` and the memory-safe ceiling
(`-m` / 512, about 100 KB at the default 50 MB soft limit). Bodies over the
cap are rejected with `413` before parsing; raising `-m` raises the ceiling.

Example for a production app:

```bash
bialet -p 7001 -m 1024 -M 2048 -c 25 -C 50 -w -l /var/log/bialet/app.log /www/myapp
```

## Quick Deploy Checklist

1. **Copy the binary** — `scp bialet user@host:/usr/local/bin/`
2. **Copy your app** — `scp -r *.wren static/ user@host:/www/myapp/`
3. **Set up the reverse proxy** — nginx, Apache, or Caddy (see above),
   including the body-size cap, read timeout, and private-file deny from the
   hardening section
4. **Create the systemd unit** — so it starts on boot and restarts on crash
5. **Point your DNS** — A/AAAA record to your server's IP
6. **Add TLS** — Let's Encrypt via certbot (nginx/Apache) or Caddy's auto-TLS
