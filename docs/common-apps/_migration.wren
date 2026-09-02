// Seeds the defaults that make this app run out of the box. Everything lives
// in the config store (BIALET_CONFIG), so nothing here creates tables.
//
// INSERT OR IGNORE keeps values that were already provisioned with
// `bialet -r` (they run before the server starts, and therefore before this
// migration) and only fills the gaps. To set your own credentials:
//
//   bialet -r 'Config.set("login_user", "you")' docs/common-apps
//   bialet -r 'Config.set("login_password", Util.hash("s3cret"))' docs/common-apps
//
Db.migrate("Common Apps defaults", Fn.new {
  `INSERT OR IGNORE INTO BIALET_CONFIG (key, val) VALUES ('login_user', 'admin')`.query
  `INSERT OR IGNORE INTO BIALET_CONFIG (key, val) VALUES ('app_url', 'http://127.0.0.1:7001')`.query
  `INSERT OR IGNORE INTO BIALET_CONFIG (key, val) VALUES ('login_password', ?)`.query(Util.hash("admin123"))
})
