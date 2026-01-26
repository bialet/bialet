# Building REST APIs

This guide shows you how to create RESTful APIs with Bialet, including routing, CORS, and authentication.

## Quick Start

Create an API endpoint by making a `.wren` file in your project:

```wren
// api/users.wren - Handles /api/users

// Enable CORS for browser access
if (Response.cors) return

// Get ID from query parameter (e.g., /api/users?id=1)
var id = Request.get("id")

if (id) {
  // Get single user
  var user = `SELECT id, name, email FROM users WHERE id = ?`.first([id])
  
  if (!user) {
    Response.status(404)
    return Response.json({"error": "User not found"})
  }
  
  return Response.json(user)
}

// List all users
var users = `SELECT id, name, email FROM users`.fetch()
Response.json(users)
```

Visit `http://localhost:7001/api/users` to list all users, or `http://localhost:7001/api/users?id=1` to get a specific user.

## Project Structure

For a typical API, organize your files like this:

```
my-api/
├── _db.sqlite3           # Database file
├── _migration.wren       # Database migrations
├── api/
│   ├── users.wren       # /api/users endpoint
│   └── products.wren    # /api/products endpoint
└── _app.wren            # Shared utilities (optional)
```

## Handling Different HTTP Methods

Handle different HTTP methods in a single file using query parameters:

```wren
// api/users.wren - Handles /api/users

// Enable CORS for all methods
if (Response.cors) return

// Get ID from query parameter
var id = Request.get("id")

// GET /api/users?id=1 - Get a single user
if (id && Request.method == "GET") {
  var user = `SELECT id, name, email, created_at FROM users WHERE id = ?`.first([id])
  
  if (!user) {
    Response.status(404)
    return Response.json({"error": "User not found"})
  }
  
  return Response.json(user)
}

// GET /api/users - List all users
if (!id && Request.method == "GET") {
  var users = `SELECT id, name, email, created_at FROM users`.fetch()
  return Response.json(users)
}

// POST /api/users - Create a new user
if (Request.method == "POST") {
  var data = Request.json()
  var name = data["name"]
  var email = data["email"]
  
  // Validation
  if (!name || !email) {
    Response.status(400)
    return Response.json({"error": "Name and email are required"})
  }
  
  // Insert new user
  var userId = `INSERT INTO users (name, email) VALUES (?, ?)`.query([name, email])
  
  Response.status(201)
  return Response.json({"id": userId, "name": name, "email": email})
}

// PUT /api/users?id=1 - Update a user
if (id && Request.method == "PUT") {
  var data = Request.json()
  var name = data["name"]
  var email = data["email"]
  
  // Check if user exists
  var user = `SELECT id FROM users WHERE id = ?`.first([id])
  if (!user) {
    Response.status(404)
    return Response.json({"error": "User not found"})
  }
  
  // Update user
  `UPDATE users SET name = ?, email = ? WHERE id = ?`.query([name, email, id])
  
  return Response.json({"id": id, "name": name, "email": email})
}

// DELETE /api/users?id=1 - Delete a user
if (id && Request.method == "DELETE") {
  var result = `DELETE FROM users WHERE id = ?`.query([id])
  
  Response.status(204)
  return Response.json({})
}

// 404 for unknown operations
Response.status(404)
Response.json({"error": "Invalid request"})
```

## CORS Configuration

Enable Cross-Origin Resource Sharing to allow browser access from different domains:

```wren
// Allow all origins (simplest form)
if (Response.cors) return

// Allow specific origin
if (Response.cors("https://myapp.com")) return

// Full control over CORS
if (Response.cors("https://myapp.com", "GET, POST, PUT, DELETE", "Content-Type, Authorization")) return
```

The `Response.cors` method:
- Sets appropriate CORS headers
- Automatically handles OPTIONS preflight requests
- Returns `true` for OPTIONS (so you return early)
- Returns `false` for other methods (so processing continues)

## Request Parsing

### JSON Request Body

```wren
// Using the convenience method
var data = Request.json()
var name = data["name"]
var email = data["email"]

// Or check content type first
if (Request.isJson) {
  var data = Request.json()
  // ... use the data
}
```

### Form Data

```wren
var name = Request.post("name")
var email = Request.post("email")
```

### Query Parameters

```wren
// GET /api/users?page=2&limit=10
var page = Request.get("page")
var limit = Request.get("limit")
```

### Multiple Query Parameters

```wren
// For /api/users?id=123&include=posts
var userId = Request.get("id")
var include = Request.get("include")
```

## Response Formats

### JSON Response

```wren
Response.json({"message": "Success", "data": users})
```

### Status Codes

```wren
Response.status(200)  // OK
Response.status(201)  // Created
Response.status(204)  // No Content
Response.status(400)  // Bad Request
Response.status(401)  // Unauthorized
Response.status(403)  // Forbidden
Response.status(404)  // Not Found
Response.status(500)  // Internal Server Error
```

### Error Responses

```wren
Response.status(400)
Response.json({
  "error": "Validation failed",
  "details": {
    "name": "Name is required",
    "email": "Invalid email format"
  }
})
```

## Authentication

### API Key Authentication

```wren
// api/_route.wren
users.wren

// Enable CORS
if (Response.cors) return

// Check API key
var apiKey = Request.header("x-api-key")
if (!apiKey) {
  Response.status(401)
  return Response.json({"error": "API key is required"})
}

// Validate API key
var user = `SELECT * FROM api_keys WHERE key = ? AND active = 1`.first([apiKey])
if (!user) {
  Response.status(401)
  return Response.json({"error": "Invalid API key"})
}

// Continue with your API logic...
var id = Request.get("id"
```

### Bearer Token Authentication

```wren
// Get authorization header
var authHeader = Request.header("authorization")
if (!authHeader) {
  Response.status(401)
  return Response.json({"error": "Authorization required"})
}

// Extract token (format: "Bearer <token>")
var parts = authHeader.split(" ")
if (parts.count != 2 || parts[0] != "Bearer") {
  Response.status(401)
  return Response.json({"error": "Invalid authorization format"})
}

var token = parts[1]

// Validate token
var session = `SELECT * FROM sessions WHERE token = ? AND expires_at > datetime('now')`.first([token])
if (!session) {
  Response.status(401)
  return Response.json({"error": "Invalid or expired token"})
}
```

## Pagination

```wren
// GET /api/users?page=1&limit=20

var page = Util.toNum(Request.get("page")) || 1
var limit = Util.toNum(Request.get("limit")) || 20
var offset = (page - 1) * limit

// Get total count
var total = `SELECT COUNT(*) as count FROM users`.first()["count"]

// Get paginated results
var users = `SELECT id, name, email FROM users LIMIT ? OFFSET ?`.fetch([limit, offset])

// Return with pagination metadata
Response.json({
  "data": users,
  "pagination": {
    "page": page,
    "limit": limit,
    "total": Util.toNum(total),
    "pages": ((Util.toNum(total) + limit - 1) / limit).floor
  }
})
```

## Filtering and Sorting

```wren
// GET /api/users?search=john&status=active&sort=name&order=asc

var search = Request.get("search") || ""
var status = Request.get("status") || ""
var sort = Request.get("sort") || "id"
var order = Request.get("order") || "asc"

// Define allowed columns for sorting
var allowedSorts = ["id", "name", "email", "created_at"]

// Use parameterized queries with conditional logic and safe sorting
var users = `
  SELECT * FROM users 
  WHERE (? = '' OR name LIKE '%' || ? || '%')
    AND (? = '' OR status = ?)
`.order(sort, order, allowedSorts).fetch([search, search, status, status])

Response.json(users)
```

The `.order()` method validates the sort column against the allowed list and normalizes the direction (case-insensitive "asc" or "desc"), preventing SQL injection through column name manipulation. You can also pass an optional fourth parameter to add a LIMIT clause:

```wren
// Top 10 users by score
var topUsers = `SELECT * FROM users`
  .order("score", "desc", ["score", "name"], 10)
  .fetch
```

## Database Migrations

Create a `_migration.wren` file for your API database:

```wren
// _migration.wren

Db.migrate("001_create_users", `
  CREATE TABLE users (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    name TEXT NOT NULL,
    email TEXT UNIQUE NOT NULL,
    created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    updated_at DATETIME DEFAULT CURRENT_TIMESTAMP
  );
  
  CREATE INDEX idx_users_email ON users(email);
`)

Db.migrate("002_create_api_keys", `
  CREATE TABLE api_keys (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    key TEXT UNIQUE NOT NULL,
    user_id INTEGER NOT NULL,
    active INTEGER DEFAULT 1,
    created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    FOREIGN KEY (user_id) REFERENCES users(id)
  );
  
  CREATE INDEX idx_api_keys_key ON api_keys(key);
`)

Db.migrate("003_create_products", `
  CREATE TABLE products (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    name TEXT NOT NULL,
    description TEXT,
    price REAL NOT NULL,
    stock INTEGER DEFAULT 0,
    created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    updated_at DATETIME DEFAULT CURRENT_TIMESTAMP
  );
`)
```

## Complete Example: Products API

Here's a complete REST API for managing products:

```wren
// api/_route.wren
products.wren

// Enable CORS
if (Response.cors) return

// Authentication (optional)
var apiKey = Request.header("x-api-key")
if (!apiKey) {
  Response.status(401)
  return Response.json({"error": "API key required"})
}

var user = `SELECT * FROM api_keys WHERE key = ? AND active = 1`.first([apiKey])
if (!user) {
  Response.status(401)
  return Response.json({"error": "Invalid API key"})
}

var id = Request.get("id")

// GET /api/products?id=1 - Get single product
if (id && Request.method == "GET") {
  var product = `SELECT * FROM products WHERE id = ?`.first([id])
  
  if (!product) {
    Response.status(404)
    return Response.json({"error": "Product not found"})
  }
  
  return Response.json(product)
}

// GET /api/products - List products with pagination
if (!id && Request.method == "GET") {
  var page = Util.toNum(Request.get("page")) || 1
  var limit = Util.toNum(Request.get("limit")) || 20
  var offset = (page - 1) * limit
  
  var total = `SELECT COUNT(*) as count FROM products`.first()["count"]
  var products = `SELECT * FROM products ORDER BY created_at DESC LIMIT ? OFFSET ?`.fetch([limit, offset])
  
  return Response.json({
    "data": products,
    "pagination": {
      "page": page,
      "limit": limit,
      "total": Util.toNum(total),
      "pages": ((Util.toNum(total) + limit - 1) / limit).floor
    }
  })
}

// POST /api/products - Create product
if (Request.method == "POST") {
  var data = Request.json()
  var name = data["name"]
  var description = data["description"]
  var price = data["price"]
  var stock = data["stock"] || 0
  
  // Validation
  if (!name || !price) {
    Response.status(400)
    return Response.json({"error": "Name and price are required"})
  }
  
  var productId = `
    INSERT INTO products (name, description, price, stock) 
    VALUES (?, ?, ?, ?)
  `.query([name, description, price, stock])
  
  Response.status(201)
  return Response.json({
    "id": productId,
    "name": name,
    "description": description,
    "price": price,
    "stock": stock
  })
}

// PUT /api/products?id=1 - Update product
if (id && Request.method == "PUT") {
  var product = `SELECT * FROM products WHERE id = ?`.first([id])
  if (!product) {
    Response.status(404)
    return Response.json({"error": "Product not found"})
  }
  
  var data = Request.json()
  var name = data["name"] || product["name"]
  var description = data["description"] || product["description"]
  var price = data["price"] || product["price"]
  var stock = data["stock"] || product["stock"]
  
  `
    UPDATE products 
    SET name = ?, description = ?, price = ?, stock = ?, updated_at = CURRENT_TIMESTAMP 
    WHERE id = ?
  `.query([name, description, price, stock, id])
  
  return Response.json({
    "id": id,
    "name": name,
    "description": description,
    "price": price,
    "stock": stock
  })
}

// DELETE /api/products?id=1 - Delete product
if (id && Request.method == "DELETE") {
  var product = `SELECT id FROM products WHERE id = ?`.first([id])
  if (!product) {
    Response.status(404)
    return Response.json({"error": "Product not found"})
  }
  
  `DELETE FROM products WHERE id = ?`.query([id])
  
  Response.status(204)
  return Response.json({})
}

// 404 for unknown operations
Response.status(404)
Response.json({"error": "Invalid request

## Testing Your API

### Using curl

```bash
# List products
curl http://localhost:7001/api/products

# Get single product
curl http://localhost:7001/api/products/1

# Cre"http://localhost:7001/api/products?id=1"

# Create product
curl -X POST http://localhost:7001/api/products \
  -H "Content-Type: application/json" \
  -H "X-API-Key: your-api-key" \
  -d '{"name":"Widget","price":19.99,"stock":100}'

# Update product
curl -X PUT "http://localhost:7001/api/products?id=1" \
  -H "Content-Type: application/json" \
  -H "X-API-Key: your-api-key" \
  -d '{"price":24.99}'

# Delete product
curl -X DELETE "http://localhost:7001/api/products?id=1"
# With pagination
curl "http://localhost:7001/api/products?page=2&limit=10"
```

### Using JavaScript (fetch)

```javascript
// List products
const products = await fetch('http://localhost:7001/api/products')
  .then(res => res.json());

// Create product
const newProduct = await fetch('http://localhost:7001/api/products', {
  mGet single product
const product = await fetch('http://localhost:7001/api/products?id=1')
  .then(res => res.json());

// Create product
const newProduct = await fetch('http://localhost:7001/api/products', {
  method: 'POST',
  headers: {
    'Content-Type': 'application/json',
    'X-API-Key': 'your-api-key'
  },
  body: JSON.stringify({
    name: 'Widget',
    price: 19.99,
    stock: 100
  })
}).then(res => res.json());

// Update product
const updated = await fetch('http://localhost:7001/api/products?id=1', {
  method: 'PUT',
  headers: {
    'Content-Type': 'application/json',
    'X-API-Key': 'your-api-key'
  },
  body: JSON.stringify({
    price: 24.99
  })
}).then(res => res.json());

// Delete product
await fetch('http://localhost:7001/api/products?id=
    'X-API-Key': 'your-api-key'
  }
});
```

## Next Steps

- Learn about [Database operations](database.md)
- Explore [Session management](reference)
- Read about [File uploads](file.md)
- Check out [External imports](structure) for using external libraries
