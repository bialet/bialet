# Model Context Protocol (MCP)

Bialet includes built-in support for the
[Model Context Protocol (MCP)](https://modelcontextprotocol.io/), allowing you
to create MCP servers that expose tools, resources, and prompts to AI assistants
like Claude, GitHub Copilot, and others.

## What is MCP?

The Model Context Protocol is an open standard that enables seamless integration
between AI applications and external data sources. With Bialet's MCP support,
you can:

- **Expose Tools**: Create callable functions that AI assistants can invoke
- **Provide Resources**: Share data and content that assistants can access
- **Define Prompts**: Offer pre-configured prompts for specific tasks

## Creating an MCP Server

To create an MCP server in Bialet, you need a `_route.wren` file that defines
your tools and serves them using the `Mcp` class.

**Important**: The `_route.wren` file is always needed for MCP servers. You
don't need to place it inside a specific `mcp` folder - any directory can host
an MCP server through its `_route.wren` file.

### Basic Structure

```wren
import "gh:bialet/extra/mcp" for Mcp

// Define your tools as classes
class MyTool {
  construct new(params) {
    // Initialize with parameters
  }

  // Define the tool's logic
  call() {
    // Return the result
  }
}

// Create and configure the MCP server
var mcp = Mcp.new('my-server-name', '1.0.0')
mcp.addTool(MyTool)
mcp.addPrompt("System prompt for the AI assistant")
mcp.serve
```

## Defining Tools

Tools are defined as Wren classes with specific annotations that describe their
parameters and behavior.

### Tool Annotations

Use special comment annotations to describe your tool and its parameters:

- `#!doc = "description"`: Describes the tool or parameter
- `#!required`: Marks a parameter as required
- `#!type = TypeName`: Specifies the parameter type (String, Number, Boolean,
  etc.)
- `#!format = "format"`: Specifies the format for strings (e.g., "date",
  "email", "uri")

### Example 1: Simple Greeting Tool

```wren
import "gh:bialet/extra/mcp" for Mcp

#!doc = "A simple greeting tool"
class Greet {
  construct new(params) {
    _name = params["name"]
  }

  #!doc = "Name of the person to greet"
  #!required
  name(name) { _name = name }

  call() { "Hello, %(_name)!" }
}

var mcp = Mcp.new('greeter', '1.0.0')
mcp.addTool(Greet)
mcp.addPrompt("You are a friendly assistant that greets people.")
mcp.serve
```

### Example 2: Flight Search Tool

```wren
import "gh:bialet/extra/mcp" for Mcp

#!doc = "Search for available flights"
class SearchFlights {
  construct new(params) {
    _origin = params["origin"]
    _destination = params["destination"]
    _date = params["date"]
  }

  #!doc = "Departure city"
  #!type = String
  #!required
  origin{}

  #!doc = "Arrival city"
  #!type = String
  #!required
  destination{}

  #!doc = "Travel date"
  #!type = String
  #!format = "date"
  #!required
  date{}

  call() {
    // Query your database or external API
    var flights = `SELECT * FROM flights
                   WHERE origin = ? AND destination = ? AND date = ?`
                   .fetch([_origin, _destination, _date])

    return {
      "from": _origin,
      "to": _destination,
      "date": _date,
      "flights": flights
    }
  }
}

var mcp = Mcp.new('flight-search', '1.0.0')
mcp.addTool(SearchFlights)
mcp.addPrompt("You are a travel assistant that helps users find flights.")
mcp.serve
```

### Example 3: Database Query Tool

```wren
import "gh:bialet/extra/mcp" for Mcp

#!doc = "Query user information from the database"
class GetUser {
  construct new(params) {
    _userId = params["userId"]
  }

  #!doc = "User ID to query"
  #!type = Number
  #!required
  userId{}

  call() {
    var user = `SELECT * FROM users WHERE id = ?`.first(_userId)

    if (user == null) {
      return {"error": "User not found"}
    }

    return {
      "id": user["id"],
      "name": user["name"],
      "email": user["email"]
    }
  }
}

var mcp = Mcp.new('user-api', '1.0.0')
mcp.addTool(GetUser)
mcp.addPrompt("You are an assistant that provides user information.")
mcp.serve
```

## Tool Parameters

### Parameter Types

The following types are supported:

- `String`: Text values
- `Number`: Numeric values
- `Boolean`: True/false values
- `Array`: Lists of values
- `Object`: Key-value pairs (Map)

### Parameter Formats

For `String` type parameters, you can specify formats:

- `"date"`: ISO 8601 date format (YYYY-MM-DD)
- `"date-time"`: ISO 8601 datetime format
- `"email"`: Email address format
- `"uri"`: URI/URL format
- `"uuid"`: UUID format

### Optional vs Required Parameters

By default, parameters are optional unless marked with `#!required`:

```wren
class MyTool {
  construct new(params) {
    _required = params["required"]
    _optional = params["optional"]
  }

  #!doc = "This parameter is required"
  #!required
  required{}

  #!doc = "This parameter is optional"
  optional{}

  call() {
    // Handle both required and optional parameters
  }
}
```

## Mcp Class Methods

### Mcp.new(name, version)

Creates a new MCP server instance.

- `name`: The name of your MCP server (String)
- `version`: The version number (String)

```wren
var mcp = Mcp.new('my-server', '1.0.0')
```

### addTool(toolClass)

Adds a tool class to the MCP server.

- `toolClass`: The class that implements the tool

```wren
mcp.addTool(Greet)
mcp.addTool(SearchFlights)
```

### addPrompt(prompt)

Sets the system prompt for the MCP server. This prompt helps guide the AI
assistant on how to use your tools.

- `prompt`: The system prompt text (String)

```wren
mcp.addPrompt("You are a helpful assistant that can greet people and search for flights.")
```

### serve

Starts the MCP server and begins listening for requests. This must be the last
call in your `_route.wren` file.

```wren
mcp.serve
```

## Returning Data from Tools

Tools can return various types of data:

### Simple Values

```wren
call() { "Hello, World!" }
call() { 42 }
call() { true }
```

### Maps (Objects)

```wren
call() {
  return {
    "status": "success",
    "data": "Some value",
    "timestamp": Date.now
  }
}
```

### Lists (Arrays)

```wren
call() {
  return ["item1", "item2", "item3"]
}
```

### Database Results

```wren
call() {
  var users = `SELECT * FROM users`.fetch
  return users
}
```

## Complete Example

Here's a complete example that combines database queries, file handling, and
multiple tools:

```wren
import "gh:bialet/extra/mcp" for Mcp

#!doc = "Get user profile information"
class GetUserProfile {
  construct new(params) {
    _userId = params["userId"]
  }

  #!doc = "User ID"
  #!type = Number
  #!required
  userId{}

  call() {
    var user = `SELECT u.*, f.id as avatar_id
                FROM users u
                LEFT JOIN files f ON u.avatar_file_id = f.id
                WHERE u.id = ?`.first(_userId)

    if (user == null) {
      return {"error": "User not found"}
    }

    return {
      "id": user["id"],
      "name": user["name"],
      "email": user["email"],
      "avatar_url": user["avatar_id"] ? "/files/%(user["avatar_id"])" : null
    }
  }
}

#!doc = "Create a new user"
class CreateUser {
  construct new(params) {
    _name = params["name"]
    _email = params["email"]
  }

  #!doc = "User's full name"
  #!type = String
  #!required
  name{}

  #!doc = "User's email address"
  #!type = String
  #!format = "email"
  #!required
  email{}

  call() {
    var userData = {
      "name": _name,
      "email": _email,
      "created_at": Date.now
    }

    var userId = Db.save("users", userData)

    return {
      "status": "success",
      "userId": userId,
      "message": "User created successfully"
    }
  }
}

#!doc = "Search users by name or email"
class SearchUsers {
  construct new(params) {
    _query = params["query"]
  }

  #!doc = "Search query"
  #!type = String
  #!required
  query{}

  call() {
    var searchTerm = "%%(_query)%"
    var users = `SELECT id, name, email
                 FROM users
                 WHERE name LIKE ? OR email LIKE ?
                 LIMIT 10`.fetch([searchTerm, searchTerm])

    return {
      "count": users.count,
      "users": users
    }
  }
}

// Set up the MCP server
var mcp = Mcp.new('bialet-user-api', '1.0.0')
mcp.addTool(GetUserProfile)
mcp.addTool(CreateUser)
mcp.addTool(SearchUsers)
mcp.addPrompt("You are a user management assistant that can retrieve, create, and search for users.")
mcp.serve
```

## Connecting to AI Assistants

Once your MCP server is running in Bialet, AI assistants can connect to it using
the standard MCP protocol. The server will be accessible at the URL where your
Bialet application is running.

### Configuration Example

For Claude Desktop or other MCP clients, you would configure the connection
like:

```json
{
  "mcpServers": {
    "bialet-server": {
      "url": "http://localhost:7001/api/mcp",
      "transport": "http"
    }
  }
}
```

Replace the URL with your actual Bialet server address and the path to your
`_route.wren` file.

## Best Practices

1. **Use Descriptive Documentation**: Always include `#!doc` annotations to help
   AI assistants understand your tools
2. **Validate Input**: Check parameters in your `call()` method before using
   them
3. **Handle Errors Gracefully**: Return error objects instead of crashing
4. **Keep Tools Focused**: Each tool should do one thing well
5. **Use Database Efficiently**: Leverage SQLite queries for data operations
6. **Return Structured Data**: Use Maps and Lists for complex return values
7. **Test Your Tools**: Verify each tool works correctly before deployment

## Debugging

To debug your MCP server, you can use `System.log()` to output messages:

```wren
call() {
  System.log("Received parameters: %(_userId)")
  var user = `SELECT * FROM users WHERE id = ?`.first(_userId)
  System.log("Found user: %(user)")
  return user
}
```

These messages will appear in the Bialet server logs, helping you track down
issues.

## Security Considerations

1. **Validate User Input**: Always validate and sanitize parameters
2. **Use Parameterized Queries**: Never concatenate user input into SQL queries
3. **Limit Data Exposure**: Only return necessary information
4. **Implement Rate Limiting**: Consider adding rate limits to prevent abuse
5. **Authenticate Requests**: Add authentication if your MCP server handles
   sensitive data

## Further Reading

- [Model Context Protocol Specification](https://modelcontextprotocol.io/)
- [Database Documentation](database.md) - Learn more about SQLite in Bialet
- [File Handling](file.md) - Work with files in your MCP tools
- [Structure](structure.md) - Understanding `_route.wren` and routing
