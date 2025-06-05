
`CREATE TABLE IF NOT EXISTS users (first_name text, last_name text)`.query
Db.save('users', {
  "first_name": "John",
  "last_name": "Doe",
})
var user = `SELECT * FROM users`.first
Response.out("%(user['first_name']) %(user['last_name'])")
