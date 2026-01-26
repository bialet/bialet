// Example: Safe sorting with user-controlled parameters
// This example demonstrates how to use the .order() method to safely handle
// user input for sorting while preventing SQL injection.

// Create sample table (in production, use migrations)
`CREATE TABLE IF NOT EXISTS products (
  id INTEGER PRIMARY KEY AUTOINCREMENT,
  name TEXT NOT NULL,
  price REAL NOT NULL,
  category TEXT,
  created_at DATETIME DEFAULT CURRENT_TIMESTAMP
)`.query

// Insert sample data if table is empty
var count = `SELECT COUNT(*) as count FROM products`.toNum
if (count == 0) {
  var products = [
    ["Laptop", "999.99", "Electronics"],
    ["Mouse", "29.99", "Electronics"],
    ["Keyboard", "79.99", "Electronics"],
    ["Desk Chair", "299.99", "Furniture"],
    ["Standing Desk", "599.99", "Furniture"],
    ["Monitor", "349.99", "Electronics"],
    ["Webcam", "89.99", "Electronics"],
    ["Headphones", "149.99", "Electronics"]
  ]
  
  for (product in products) {
    `INSERT INTO products (name, price, category) VALUES (?, ?, ?)`.query(product)
  }
}

// Get sort parameters from query string (e.g., ?sort=price&order=desc)
var sort = Request.get("sort") || "name"
var order = Request.get("order") || "asc"
var category = Request.get("category") || ""

// Define allowed columns for sorting (prevents SQL injection)
var allowedSorts = ["id", "name", "price", "category", "created_at"]

// Build query with safe sorting
var query = `
  SELECT * FROM products 
  WHERE (? = '' OR category = ?)
`

// Use .order() method to safely add ORDER BY clause
var products = query.order(sort, order, allowedSorts).fetch([category, category])

// Current sort parameters for the UI
var isNameSort = sort == "name"
var isPriceSort = sort == "price"
var isCategorySort = sort == "category"
var isAsc = order == "asc"

return <!doctype html>
<html>
  <head>
    <meta charset="utf-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <link rel="stylesheet" href="style.css">
    <style>
      table { width: 100%; border-collapse: collapse; margin: 20px 0; }
      th, td { padding: 12px; text-align: left; border-bottom: 1px solid #ddd; }
      th { background-color: #f4f4f4; font-weight: bold; cursor: pointer; }
      th a { text-decoration: none; color: #333; display: block; }
      th a:hover { color: #0066cc; }
      .sort-indicator { float: right; }
      .active-sort { color: #0066cc; font-weight: bold; }
      .filter { margin: 20px 0; }
      .filter label { margin-right: 10px; }
      .filter select { padding: 5px; }
    </style>
  </head>
  <body>
    <h1>Product List with Sorting</h1>
    
    <div class="filter">
      <label for="category">Filter by category:</label>
      <select id="category" onchange="window.location.href='?category=' + this.value + '&sort={{ sort }}&order={{ order }}'">
        <option value="">All Categories</option>
        <option value="Electronics" {{ category == "Electronics" ? "selected" : ""}}>Electronics</option>
        <option value="Furniture" {{ category == "Furniture" ? "selected" : ""}}>Furniture</option>
      </select>
    </div>
    
      <table>
        <tr>
          <th>
            <a href="?sort=id&order={{ isAsc && sort == "id" ? "desc" : "asc" }}&category={{ category }}" class="{{ sort == "id" ? "active-sort" : ""}}">
              ID {{ sort == "id" ? <span class="sort-indicator">{{ isAsc ? "▲" : "▼" }}</span> : ""}}
            </a>
          </th>
          <th>
            <a href="?sort=name&order={{ isAsc && isNameSort ? "desc" : "asc" }}&category={{ category }}" class="{{ isNameSort ? "active-sort" : ""}}">
              Name {{ isNameSort ? <span class="sort-indicator">{{ isAsc ? "▲" : "▼" }}</span> : "" }}
            </a>
          </th>
          <th>
            <a href="?sort=price&order={{ isAsc && isPriceSort ? "desc" : "asc" }}&category={{ category }}" class="{{ isPriceSort ? "active-sort" : ""}}">
              Price {{ isPriceSort ? <span class="sort-indicator">{{ isAsc ? "▲" : "▼" }}</span> : "" }}
            </a>
          </th>
          <th>
            <a href="?sort=category&order={{ isAsc && isCategorySort ? "desc" : "asc" }}&category={{ category }}" class="{{ isCategorySort ? "active-sort" : ""}}">
              Category {{ isCategorySort ? <span class="sort-indicator">{{ isAsc ? "▲" : "▼" }}</span> : ""}}
            </a>
          </th>
        </tr>
        {{ products.map{|product| <tr>
            <td>{{ product["id"] }}</td>
            <td>{{ product["name"] }}</td>
            <td>${{ product["price"] }}</td>
            <td>{{ product["category"] }}</td>
          </tr> } }}
      </table>
    
    <div style="margin-top: 20px; padding: 15px; background: #f9f9f9; border-left: 4px solid #0066cc;">
      <h3>How it works:</h3>
      <ul>
        <li>Click on column headers to sort</li>
        <li>The <code>.order()</code> method validates sort columns against the allowed list</li>
        <li>Only columns in <code>allowedSorts</code> can be used for sorting</li>
        <li>Invalid columns default to "name"</li>
        <li>Sort direction is validated (asc/desc) and normalized</li>
        <li>This prevents SQL injection through column name manipulation</li>
      </ul>
      <p><strong>Current query:</strong> <code>sort={{ sort }}&order={{ order }}</code></p>
      <p><strong>Allowed columns:</strong> {{ allowedSorts.join(", ") }}</p>
    </div>
    
    <p><a href=".">Back ↩️</a></p>
  </body>
</html>