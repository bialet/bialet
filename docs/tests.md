# Testing in Bialet

Bialet includes a built-in testing framework for HTTP endpoint testing. Tests are written in Wren and can verify responses from your application endpoints.

## Running Tests

Use the `-T` flag to run all tests:

```bash
bialet -T                    # Run all tests in _tests/ folder
bialet -T docs/examples      # Run tests in specific directory
```

## Test Directory Structure

Tests are located in the `_tests/` folder within your application:

```
myapp/
├── _app.wren
├── _migration.wren
├── hi.wren
├── _tests/
│   ├── _init.wren           # Optional: runs before test files
│   ├── hi.wren              # Tests for hi.wren
│   └── password.wren        # Tests for password.wren
```

**Important:** The `_tests/` folder name starts with `_` to prevent direct HTTP access, following Bialet's convention for private files.

## Test Initialization

### `_tests/_init.wren` (Optional)

If present, this file runs once before each test file. Use it for common setup:

```wren
// _tests/_init.wren
System.log("Setting up test environment")

// Insert test data
`INSERT INTO users (name, email) VALUES (?, ?)`.query("Test User", "test@example.com")
```

## Writing Tests

Tests use the `Test` class with a fluent API:

```wren
// Basic GET request test
Test.get("/hi").status(200).contains("Hello World")

// POST request test with form data
Test.post("/password", {"password":"123", "password-check":"123"})
    .status(200)
    .contains("The passwords are the same")
```

## Test API Reference

### HTTP Request Methods

| Method | Description |
|--------|-------------|
| `Test.get(route)` | Send GET request |
| `Test.post(route, params)` | Send POST request with form data |
| `Test.apiGet(route)` | Send GET with JSON Content-Type header |
| `Test.apiPost(route, params)` | Send POST with JSON body |
| `Test.apiPut(route, params)` | Send PUT with JSON body |
| `Test.apiDelete(route)` | Send DELETE request |

### Request Configuration

Chain these before assertions:

```wren
Test.get("/api/user")
    .setHeader("Authorization", "Bearer token123")  // Add custom header
    .method("GET")                                     // Explicitly set method
    .postData({"key": "value"})                       // Set POST data
```

### Assertions

Chain these after the request method:

| Assertion | Description |
|-----------|-------------|
| `.status(code)` | Assert HTTP status code |
| `.contains(str)` | Assert body contains string |
| `.equals(str)` | Assert body exactly equals string |
| `.notContains(str)` | Assert body doesn't contain string |
| `.header(name)` | Assert header exists |
| `.header(name, value)` | Assert header equals value |
| `.json()` | Return parsed JSON body (Map or List) |
| `.jsonContains(key)` | Assert JSON object has key |
| `.jsonEquals(key, value)` | Assert JSON key equals value |
| `.jsonNotContains(key)` | Assert JSON object doesn't have key |
| `.assert(condition, message)` | Custom assertion with message |

**Note:** Assertions are lazy - the request is executed when the first assertion is called.

### Body Assertions

```wren
// Exact match
Test.get("/health").status(200).equals("OK")

// Contains substring
Test.get("/page").contains("Welcome")

// Negative assertion (security checks)
Test.get("/public").notContains("password")

// Multiple assertions chained
Test.get("/api")
    .status(200)
    .contains("success")
    .notContains("error")
```

### JSON Assertions

For API endpoints that return JSON:

```wren
// Parse JSON body for custom validation
var data = Test.apiGet("/users").status(200).json()
Test.assert(data.count > 0, "Expected at least one user")

// Fluent JSON key assertions
Test.apiPost("/users", {"name": "Alice"})
    .status(201)
    .jsonContains("id")
    .jsonContains("createdAt")
    .jsonEquals("name", "Alice")

// Security - ensure sensitive fields not exposed
Test.get("/public/user/1")
    .status(200)
    .jsonNotContains("password")
    .jsonNotContains("ssn")
```

### Custom Assertions

Use `assert()` for complex validation logic:

```wren
// Static method
var users = Test.apiGet("/users").json()
Test.assert(users.count >= 5, "Expected at least 5 users")

// Instance method (chained)
Test.apiGet("/stats")
    .status(200)
    .assert(json()["active"] > 0, "Expected active users")
```

### Multiple Assertions

Chain multiple assertions on a single request:

```wren
Test.get("/api/users")
    .status(200)
    .header("Content-Type", "application/json")
    .contains("user@example.com")
```

### Multiple Tests in One File

Write multiple independent tests in a single file:

```wren
// _tests/api.wren

// Test 1: List users
Test.get("/api/users").status(200).contains("users")

// Test 2: Create user
Test.apiPost("/api/users", {"name": "John"})
    .status(201)
    .contains("John")

// Test 3: Invalid request
Test.post("/api/users", {})
    .status(400)
    .contains("name is required")
```

## API Testing Examples

### JSON Response Testing

```wren
// Test JSON endpoint (from _tests/json.wren)
// Endpoint returns array of counters from database

// Validate JSON structure
var data = Test.apiGet("/json").status(200).json()
Test.assert(data is List, "Expected JSON array")

// Insert test data
`INSERT OR REPLACE INTO counter (name, value) VALUES ('test1', 10)`.query()

// Test with assertions
var counters = Test.apiGet("/json").status(200).json()
Test.assert(counters.count >= 1, "Expected at least 1 counter")

// Validate specific values
for (counter in counters) {
  if (counter["name"] == "test1") {
    Test.assert(counter["value"] == "10", "Expected value '10'")
  }
}
```

### Multiple Assertions

```wren
// Test with chained assertions (from _tests/assertions.wren)
Test.get("/hi")
    .status(200)
    .contains("Hello")
    .contains("World")
    .notContains("Error")
    .equals("<h1>👋 Hello World</h1>")

// Security testing
Test.get("/public")
    .status(200)
    .notContains("password")
    .notContains("secret")
```

### Form Testing

```wren
// Test form submission
Test.post("/password", {"password": "test123", "password-check": "test123"})
    .status(200)
    .contains("The passwords are the same")
    .contains("Hash:")

// Test validation
Test.post("/password", {"password": "abc", "password-check": "xyz"})
    .status(200)
    .contains("The passwords are different")
```

## Best Practices

### Use Appropriate Assertion Methods

```wren
// ✅ Good - specific assertions
Test.get("/health").equals("OK")
Test.apiGet("/users").jsonContains("id")

// ❌ Avoid - too generic
Test.get("/health").contains("OK")  // Could match "NOT OK"
```

### Security Testing

Always check that sensitive data isn't exposed:

```wren
// Ensure passwords/keys not in responses
Test.get("/public/user")
    .notContains("password")
    .jsonNotContains("ssn")
    .jsonNotContains("api_key")
```

### Use Custom Assertions for Complex Logic

```wren
// Instead of manual Fiber.abort()
var data = Test.apiGet("/stats").json()
Test.assert(data["active"] > data["inactive"], "More active expected")
Test.assert(data["total"] == data["active"] + data["inactive"], "Count mismatch")
```

### Chain Related Assertions

```wren
// Group related checks together
Test.apiPost("/users", {"name": "Alice"})
    .status(201)
    .jsonContains("id")
    .jsonContains("createdAt")
    .jsonEquals("name", "Alice")
    .jsonEquals("active", true)
```

### Test Both Success and Failure Cases

```wren
// Success case
Test.post("/login", {"user": "admin", "pass": "correct"})
    .status(200)
    .contains("Welcome")

// Failure case
Test.post("/login", {"user": "admin", "pass": "wrong"})
    .status(401)
    .contains("Invalid credentials")
```

## Database Isolation

Tests run against a **temporary database file** that is created fresh for each test run:

1. A temporary database file is created
2. Migrations are automatically run (`_migration.wren` or `/_app/migration.wren`)
3. `_tests/_init.wren` runs (if exists)
4. Test files execute
5. Temporary database is deleted

This ensures tests don't pollute your development or production database.

## Complete Example

**Application file:** `hi.wren`
```wren
return <h1>👋 Hello World</h1>
```

**Test file:** `_tests/hi.wren`
```wren
Test.get("/hi")
    .status(200)
    .contains("Hello World")
```

**Run tests:**
```bash
$ bialet -T
Running tests in _tests/...

✓ _tests/hi.wren: Test.get("/hi").status(200).contains("Hello World")

1 passed, 0 failed
```

## Exit Codes

- `0`: All tests passed
- `1`: One or more tests failed

This makes it easy to integrate with CI/CD pipelines:

```yaml
# GitHub Actions example
- name: Run tests
  run: bialet -T
```

## How It Works

1. **Request Building**: The `Test` class builds an HTTP request (method, headers, body)
2. **Internal Routing**: Instead of making actual HTTP calls, the test runner directly invokes the Wren handler
3. **Response Capture**: The response (status, headers, body) is captured and wrapped
4. **Assertions**: Each assertion checks the response and reports failures
5. **Database**: All database operations happen on a temp DB, isolated from your main data

This approach is fast (no network overhead), reliable (no port conflicts), and safe (isolated database).

## Best Practices

1. **One test file per route/handler**: `password.wren` → `_tests/password.wren`
2. **Test success and failure cases**: Verify both happy paths and error handling
3. **Use `_init.wren` for shared setup**: Common test data, authentication tokens
4. **Clean up not needed**: Database is temporary and auto-deleted
5. **Run tests before committing**: `bialet -T` should be part of your workflow
