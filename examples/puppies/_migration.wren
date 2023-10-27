import "bialet" for Db

Db.migrate("Pictures Table", "
  CREATE TABLE pictures (
    id INTEGER PRIMARY KEY,
    file_type TEXT,
    file_extension TEXT,
    file_data BLOB,
    createdAt DATETIME DEFAULT CURRENT_TIMESTAMP
  )
")
