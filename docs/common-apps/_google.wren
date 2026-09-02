// Google integration. Bialet has no built-in OAuth2 or SMTP, so everything
// talks to Google over HTTPS with the Http class:
//
//   * OAuth2 authorization-code flow (web application). The refresh token is
//     stored in the config store once the user authorizes the app.
//   * Google Sheets API v4 to read a range or append rows.
//   * Gmail API to send email through the connected account.
//
// Credentials and tokens are kept in BIALET_CONFIG:
//   google_client_id      OAuth client ID (web application)
//   google_client_secret  OAuth client secret
//   google_user           email of the connected account
//   google_access_token   short-lived access token (refreshed automatically)
//   google_refresh_token  long-lived refresh token from the consent flow
//   google_expires        unix time when the access token stops being valid
class Google {
  static clientId { Config.get("google_client_id") }
  static clientSecret { Config.get("google_client_secret") }
  static refreshToken { Config.get("google_refresh_token") }
  static user { Config.get("google_user") }
  static lastError { Config.get("google_last_error") }

  static appUrl {
    var url = Config.get("app_url")
    return url != null && url != "" ? url : "http://127.0.0.1:7001"
  }

  // Must match the "Authorized redirect URIs" registered in Google Cloud.
  static redirectUri {
    return "%(appUrl)/google/callback"
  }

  static configured {
    var id = clientId
    var secret = clientSecret
    return id != null && id != "" && secret != null && secret != ""
  }

  static connected {
    var rt = refreshToken
    return rt != null && rt != ""
  }

  static authorizeUrl(state) {
    var params = {
      "client_id": clientId,
      "redirect_uri": redirectUri,
      "response_type": "code",
      "scope": "https://www.googleapis.com/auth/spreadsheets https://www.googleapis.com/auth/gmail.send https://www.googleapis.com/auth/userinfo.email",
      "access_type": "offline",
      "prompt": "consent",
      "include_granted_scopes": "true",
      "state": state
    }
    return "https://accounts.google.com/o/oauth2/v2/auth?" + Http.query(params)
  }

  // Returns a usable access token, refreshing it first when it is missing or
  // close to expiring. null when there is nothing to refresh from.
  static accessToken {
    var token = Config.get("google_access_token")
    var expires = Config.get("google_expires")
    if (token != null && token != "" && expires != null) {
      var exp = Num.fromString(expires)
      if (exp != null && exp > Date.now.unix) return token
    }
    if (!connected) {
      Config.set("google_last_error", "No Google account connected yet.")
      return null
    }
    if (refresh_()) return Config.get("google_access_token")
    return null
  }

  static refresh_() {
    var res = tokenCall_({
      "client_id": clientId,
      "client_secret": clientSecret,
      "refresh_token": refreshToken,
      "grant_type": "refresh_token"
    })
    if (!res["ok"]) {
      Config.set("google_last_error", res["error"])
      return false
    }
    saveTokenData_(res["data"])
    Config.delete("google_last_error")
    return true
  }

  // Runs the code from /google/callback. Stores the tokens on success.
  static exchangeCode(code) {
    var res = tokenCall_({
      "code": code,
      "client_id": clientId,
      "client_secret": clientSecret,
      "redirect_uri": redirectUri,
      "grant_type": "authorization_code"
    })
    if (!res["ok"]) return res
    saveTokenData_(res["data"])
    var token = res["data"]["access_token"]
    if (token != null) {
      var me = Http.get("https://www.googleapis.com/oauth2/v2/userinfo",
                        {"token": token})
      if (me != null && me["email"] != null) {
        Config.set("google_user", me["email"])
      }
    }
    Config.delete("google_last_error")
    return {"ok": true}
  }

  // POST https://oauth2.googleapis.com/token with a form body.
  static tokenCall_(form) {
    var http = Http.new()
    http.method = "POST"
    var ok = http.call("https://oauth2.googleapis.com/token", {"form": form})
    if (!ok) {
      return {"ok": false, "error": "Network error talking to Google: %(http.errorMessage)"}
    }
    var data = parseJson_(http.body)
    if (http.status >= 200 && http.status < 300) {
      return {"ok": true, "data": data}
    }
    var msg = "HTTP %(http.status)"
    if (data != null) {
      if (data["error"] != null && data["error"] is String) msg = data["error"]
      if (data["error_description"] != null) {
        msg = "%(msg): %(data["error_description"])"
      }
    }
    return {"ok": false, "error": msg}
  }

  static parseJson_(raw) {
    if (raw == null || raw == "") return null
    var fiber = Fiber.new { Json.parse(raw) }
    var value = fiber.try()
    return fiber.error ? null : value
  }

  static saveTokenData_(data) {
    if (data == null) return
    if (data["access_token"] != null) {
      Config.set("google_access_token", data["access_token"])
    }
    if (data["refresh_token"] != null) {
      Config.set("google_refresh_token", data["refresh_token"])
    }
    if (data["expires_in"] != null) {
      Config.set("google_expires", Date.now.unix + data["expires_in"] - 60)
    }
  }

  static disconnect() {
    Config.delete("google_access_token")
    Config.delete("google_refresh_token")
    Config.delete("google_expires")
    Config.delete("google_user")
    Config.delete("google_last_error")
  }

  // Accepts a spreadsheet URL or a bare spreadsheet id.
  static sheetIdFrom(value) {
    value = (value || "").trim()
    var marker = "/spreadsheets/d/"
    var idx = value.indexOf(marker)
    if (idx >= 0) {
      var rest = value.substring(idx + marker.count)
      var end = rest.indexOf("/")
      var id = end >= 0 ? rest[0...end] : rest
      var query = id.indexOf("?")
      return query >= 0 ? id[0...query] : id
    }
    return value
  }

  // Authenticated request to a Google API. Returns {"ok": true, "data": ...}
  // on 2xx, otherwise {"ok": false, "error": "..."} with Google's message.
  static authedRequest_(method, url, data) {
    var token = accessToken
    if (token == null) {
      return {"ok": false, "error": lastError}
    }
    var http = Http.new()
    http.method = method
    if (data != null) http.postData = data
    var ok = http.call(url, {"token": token})
    if (!ok) {
      return {"ok": false, "error": "Network error: %(http.errorMessage)"}
    }
    var parsed = parseJson_(http.body)
    if (http.status >= 200 && http.status < 300) {
      return {"ok": true, "data": parsed}
    }
    var msg = "HTTP %(http.status)"
    if (parsed != null) {
      if (parsed["error"] != null && parsed["error"] is String) {
        msg = parsed["error"]
      }
      if (parsed["error"] != null && parsed["error"] is Map &&
          parsed["error"]["message"] != null) {
        msg = parsed["error"]["message"]
      }
    }
    return {"ok": false, "error": msg}
  }

  static sheetFetch(id, range) {
    var url = "https://sheets.googleapis.com/v4/spreadsheets/%(id)/values/%(range)"
    return authedRequest_("GET", url.replace(" ", "\%20"), null)
  }

  static sheetAppend(id, range, rows) {
    var url = "https://sheets.googleapis.com/v4/spreadsheets/%(id)/values/%(range):append"
    var body = {"majorDimension": "ROWS", "values": rows}
    return authedRequest_("POST", url.replace(" ", "\%20") + "?valueInputOption=RAW", body)
  }

  static mailSend(to, subject, body) {
    var from = user
    if (from == null || from == "") {
      return {"ok": false, "error": "No Gmail account connected."}
    }
    var raw = "From: %(from)\r\nTo: %(to)\r\nSubject: %(subject)\r\nMIME-Version: 1.0\r\nContent-Type: text/plain; charset=UTF-8\r\n\r\n%(body)"
    var payload = {"raw": base64url_(raw)}
    return authedRequest_("POST",
                          "https://gmail.googleapis.com/gmail/v1/users/me/messages/send",
                          payload)
  }

  // Gmail expects a base64url (RFC 4648) payload: no padding, and `-`/`_`
  // instead of `+`/`/`. Input is treated as raw UTF-8 bytes.
  static base64url_(input) {
    var table = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/"
    var bytes = input.bytes
    var n = bytes.count
    var out = []
    var i = 0
    while (i < n) {
      var b1 = bytes[i]
      var b2 = i + 1 < n ? bytes[i + 1] : 0
      var b3 = i + 2 < n ? bytes[i + 2] : 0
      out.add(table[b1 >> 2])
      out.add(table[((b1 & 3) << 4) | (b2 >> 4)])
      out.add(i + 1 < n ? table[((b2 & 15) << 2) | (b3 >> 6)] : "=")
      out.add(i + 2 < n ? table[b3 & 63] : "=")
      i = i + 3
    }
    var s = out.join("").replace("+", "-").replace("/", "_")
    while (s.endsWith("=")) {
      s = s[0...s.count - 1]
    }
    return s
  }
}
