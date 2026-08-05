// Model. Where I'd put a TypeScript type + a Prisma call, here it's a
// Wren class with SQL strings. No types, no autocomplete, no checking —
// I miss TypeScript more than I expected.

class Task {
  static all() {
    `SELECT * FROM tasks ORDER BY id DESC`.fetch
  }

  static add(text) {
    `INSERT INTO tasks (text) VALUES (?)`.query(text)
  }

  static toggle(id) {
    `UPDATE tasks SET done = 1 - done WHERE id = ?`.query(id)
  }

  static remove(id) {
    `DELETE FROM tasks WHERE id = ?`.query(id)
  }
}
