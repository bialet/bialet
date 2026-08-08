# HTTPS / TLS

Bialet serves HTTPS natively. No reverse proxy required for basic
deployments. HTTPS is optional: the default is plain HTTP, and you turn TLS
on with the `-s` flag.

## Requirements

The binary must be built with OpenSSL. The build detects it automatically:

- Linux: `libssl-dev` installed at build time
- macOS: OpenSSL found via `pkg-config`
- Windows (MinGW): OpenSSL headers available to the compiler

Check that your binary has TLS support:

```bash
bialet -s /path/to/app
# Error: cannot initialize HTTPS ... -> no OpenSSL, rebuild with it
# 🚲 bialet is riding on https://...    -> TLS works
```

## The `_keys/` Folder

When TLS is enabled, Bialet looks for the certificate and key in the app's
`_keys/` folder:

```text
myapp/
├── _keys/
│   ├── cert.pem        # TLS certificate
│   └── key.pem         # TLS private key
├── index.wren
└── _db.sqlite3
```

Files and folders starting with `_` are never served over HTTP, so your
private key stays private. Bialet refuses to start if the certificate or key
is missing, unreadable, or does not match.

## Enable HTTPS

```bash
bialet -s /path/to/myapp
# 🚲 bialet is riding on https://127.0.0.1:7001
```

The port is unchanged (default `7001`). Point your browser at
`https://127.0.0.1:7001` — use `-p 443` for the standard HTTPS port.

### Custom Certificate Paths

Pass explicit paths when the keys live elsewhere:

```bash
bialet -s -e /etc/ssl/fullchain.pem -k /etc/ssl/privkey.pem /path/to/myapp
```

- `-e cert` — certificate file (default `<root>/_keys/cert.pem`)
- `-k key` — private key file (default `<root>/_keys/key.pem`)

### `bialet dev`

Works the same way. `dev` opens `https://` in the browser when TLS is on:

```bash
bialet dev -s
```

> ⚠️ Pitfall: HTTPS and HTTP share the same port. When `-s` is set, the
> server only speaks TLS on that port. A plain `http://` request that reaches
> it (an old browser tab, a health check, a hand-typed URL) is answered with a
> `301` redirect to the same `https://` URL, so clients self-heal instead of
> hitting a confusing handshake error.

## Getting a Certificate

### Self-Signed (testing / internal tools)

```bash
mkdir _keys
openssl req -x509 -newkey rsa:2048 -nodes \
  -keyout _keys/key.pem -out _keys/cert.pem -days 365 \
  -subj "/CN=localhost"
```

Browsers will warn about the untrusted certificate. Test with `curl -k` to
skip verification:

```bash
curl -k https://127.0.0.1:7001/
```

> ⚠️ Pitfall: a self-signed certificate encrypts traffic but does not prove
> your identity. Use one only for local development and internal tools.

### Let's Encrypt (public sites)

Use `certbot` and point Bialet at the issued files:

```bash
sudo certbot certonly --standalone -d example.com -d www.example.com
```

```bash
bialet -s \
  -e /etc/letsencrypt/live/example.com/fullchain.pem \
  -k /etc/letsencrypt/live/example.com/privkey.pem \
  -p 443 -h 0.0.0.0 /www/myapp
```

`fullchain.pem` is the certificate (the chain plus your leaf cert);
`privkey.pem` is the private key. Bialet refuses to start if the two do not
match.

Certificates expire. `certbot renew` issues new files, but the server must
pick them up — restart it after renewal (cron the renewal and a restart, or
keep using a reverse proxy that handles renewal for you).

## What You Still Need a Proxy For

TLS is not the only reason to run a reverse proxy. Bialet is single-threaded:
it accepts and serves one connection at a time in a blocking loop. The proxy
absorbs slow clients, connection churn, and request buffering. If your app
faces the public internet, keep the proxy and let it handle TLS instead — or
bind Bialet behind the proxy as usual. See
[Deployment](deployment.md).

## Security Notes

- The key is loaded from disk at startup and stays in memory. Keep
  `_keys/key.pem` read-protected and out of version control.
- Cookie `Secure` attribute is set automatically when the request arrives
  over HTTPS (see [Security](security.md)).
- On Linux the server process is forked per connection cycle; the TLS context
  is created once in the parent and shared read-only with children.
- Handshakes, reads, and writes are bounded by the same socket timeouts as
  plain HTTP, so a stalled TLS client cannot park the server.
