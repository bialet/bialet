# Built with Bialet

Bialet is small on purpose, so the useful question is not "how does it scale to
a million users" but "does a real app survive on it". This page answers that
with one app and its actual numbers.

## home.dev.ar

[home.dev.ar](https://home.dev.ar/) is a free `.dev.ar` subdomain service. You
search for a name, sign in, and point the subdomain at whatever you want to
run — a redirect, a server, a project. It is server-rendered HTML with a
session login and a dashboard, built and maintained by one developer, and it
has been running on Bialet since 2023.

It is not a showcase app written for this page. It is the site that pays for
the framework's existence.

### Traffic, 20 October – 11 November 2026

| Metric                        | Value                                   |
| ----------------------------- | --------------------------------------- |
| Requests                      | ~160,000 (about 7,300 per day)          |
| Bytes served                  | 620 MB                                  |
| 5xx responses                 | under 0.1%                              |
| 200 / 302 / 403 / 404         | 24% / 10.5% / 27% / 38%                 |
| Logins (`/iniciar-sesion`)    | 545                                     |
| Dashboard views (`/dashboard`)| 508                                     |

Top paths over the same window:

| Path              | Requests |
| ----------------- | -------- |
| `/`               | 25,400   |
| `/redirect`       | 17,500   |
| `/robots.txt`     | 1,400    |
| `/js/scripts.js`  | 812      |
| `/.env`           | 648      |
| `/css/styles.css` | 592      |
| `/iniciar-sesion` | 545      |
| `/dashboard`      | 508      |

### What the numbers say

- **Most of the traffic is not users.** Two-thirds of requests are 403s and
  404s: scanners asking for `/.env`, `/.git/config`, and API paths that do not
  exist. Bialet answers them and keeps serving.
- **The 5xx rate is the number that matters.** Under 0.1% while roughly a
  hundred thousand hostile requests arrived is the useful result. A single
  worker with no keep-alive did not fall over.
- **Real usage is small.** 545 logins and 508 dashboard views in 22 days is a
  handful of people per day. That is exactly the size of app Bialet is for, and
  saying so is more useful than a synthetic load test.

### What runs it

nginx terminates TLS and proxies to Bialet; the app is server-rendered pages
and a SQLite file. There is no frontend build, no API gateway, and no
JavaScript framework. The one operational number worth repeating is the 5xx
rate: everything else is a scanner.

### Caveats

- The nginx log has no `$host` field, so requests were attributed to
  home.dev.ar by response signature and the Bialet app log. The wildcard
  `*.dev.ar` redirect count is an upper bound; a few other vhosts also answer
  with the same redirect.
- This is a small production app, not a benchmark. For latency, throughput,
  and memory, see [Performance](performance.md).

## Add yours

If you run something on Bialet and want it listed, open a thread in
[GitHub Discussions](https://github.com/bialet/bialet/discussions). Small
internal tools are as interesting as public sites — the point is the shape of
the app, not the visitor count.
