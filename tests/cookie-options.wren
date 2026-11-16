// Cookie.set with an options map: SameSite and HttpOnly can be overridden.
Cookie.set("pref", "dark", {"SameSite": "Strict", "HttpOnly": false})
return "cookie-options"
