// Fixture for the -T -q tests. Both lines print through message(): Db.migrate
// adds "Migration applied - quiet-migrations" and System.log adds "Log ...",
// and neither may reach stdout when the suite runs with -q.
Db.migrate("quiet-migrations", "CREATE TABLE IF NOT EXISTS things (id INTEGER PRIMARY KEY)")
System.log("quiet-migration-log")
