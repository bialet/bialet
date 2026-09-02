# Common Apps

A Bialet starter that combines three things every small business or intranet
app ends up needing, behind a single config-based login:

1. A **login form** whose user and password hash live in the app config store.
2. A **Google Sheets** demo that reads a range or appends rows (Sheets API v4).
3. A **Gmail** demo that sends email from the connected account (Gmail API).

There is no SMTP server and no OAuth helper in Bialet itself: the Google
integrations are plain HTTPS calls through the built-in `Http` class, and the
access token is obtained with the standard OAuth2 authorization-code flow.
The refresh token is stored in the config store and rotated into fresh access
tokens automatically.

## Run

```bash
bialet docs/common-apps
# open http://127.0.0.1:7001
```

Default login: `admin` / `admin123`. Change it before going anywhere near
production:

```bash
bialet -r 'Config.set("login_user", "you@example.com")' docs/common-apps
bialet -r 'Config.set("login_password", Util.hash("a-long-passphrase"))' docs/common-apps
```

## Connect a Google account

1. In [Google Cloud Console](https://console.cloud.google.com) create an
   OAuth client of type **Web application**.
2. Enable the **Google Sheets API** and the **Gmail API** for the project.
3. Add the redirect URI `http://127.0.0.1:7001/google/callback` (or your real
   `app_url + /google/callback`) to the client.
4. Open the app, go to **Google** in the top bar, paste the client ID/secret,
   and click **Connect to Google**.

After consent, `/google/callback` stores the tokens in the config store and
redirects back to the connection page.

### Config keys

| Key                     | Meaning                                             |
| ----------------------- | --------------------------------------------------- |
| `login_user`            | Login username                                      |
| `login_password`        | Salted SHA-256 hash, from `Util.hash()`             |
| `app_url`               | Public base URL, used to build the redirect URI     |
| `google_client_id`      | OAuth client ID (Web application)                   |
| `google_client_secret`  | OAuth client secret                                 |
| `google_access_token`   | Short-lived access token (refreshed automatically)  |
| `google_refresh_token`  | Long-lived refresh token from the consent flow      |
| `google_expires`        | Unix time when the access token stops being valid   |
| `google_user`           | Email of the connected account                      |
| `sheet_id`              | Last spreadsheet id used on the spreadsheet page    |
| `sheet_range`           | Last range used on the spreadsheet page             |

## Notes

- Spreadsheet access works because the connected account owns (or has been
  shared on) the spreadsheet. To read a spreadsheet owned by someone else,
  open its sharing settings and grant edit access to the connected account.
- Email is always sent **as the connected account** (`From:` is set to its
  address, never to a user-supplied value).
- Defaults are seeded in `_migration.wren` with `INSERT OR IGNORE`, so values
  provisioned earlier with `bialet -r` always win.
- Keep `_db.sqlite3` out of version control: it holds the config, sessions,
  and Google tokens.
