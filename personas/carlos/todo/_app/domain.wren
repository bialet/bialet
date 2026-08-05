// The model layer. A Wren class with SQL methods — same habit as a PHP
// class wrapping PDO, minus the ceremony. Prepared statements with ?
// placeholders, exactly like PDO. No ORM magic to fight with.

class Task {
  static all() {
    `SELECT * FROM tasks ORDER BY id DESC`.fetch
  }

  static open() {
    `SELECT * FROM tasks WHERE done = 0 ORDER BY id DESC`.fetch
  }

  static done() {
    `SELECT * FROM tasks WHERE done = 1 ORDER BY id DESC`.fetch
  }

  static countOpen() {
    `SELECT COUNT(*) FROM tasks WHERE done = 0`.toNum
  }

  static add(text) {
    // same as: $stmt = $pdo->prepare("INSERT ..."); $stmt->execute([$text]);
    `INSERT INTO tasks (text) VALUES (?)`.query(text)
  }

  static toggle(id) {
    // 1 - done flips 0 <-> 1 in SQLite. Nice and short.
    `UPDATE tasks SET done = 1 - done WHERE id = ?`.query(id)
  }

  static remove(id) {
    `DELETE FROM tasks WHERE id = ?`.query(id)
  }
}
