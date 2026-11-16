// The Secure attribute follows the request scheme, including the reverse-proxy
// headers documented in security.md.
Cookie.set("secure-probe", Request.header("X-Forwarded-Proto") || "http")
return "cookie-secure"
