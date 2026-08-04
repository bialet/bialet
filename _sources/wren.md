# Wren Language Guide

Wren is a small object-oriented scripting language. See the full documentation
at [wren.io](https://wren.io).

Bialet embeds a heavily modified dialect of Wren. The syntax itself has been
extended — not through libraries, but directly in the parser — to support native
SQL queries and rich string interpolation:

- Native queries: `SELECT * FROM users`.fetch compiles into a query execution
  call. See the [Database](database.md) guide.
- Template strings: var output = <p>Hello {{ name }}</p> is syntax that
  evaluates to a string with interpolation. See [Templates](template.md) for
  details.

This is a deliberate dialect, not standard Wren. Code written for stock Wren
will not necessarily run unchanged in Bialet, and the reverse is true. Treat the
Bialet dialect as its own thing.

## Quick Guide

```wren
// Variables
var name = "Alice"
var count = 42

// Classes and constructors
class User {
  construct new(data) { _data = data }
  name { _data["name"] }
}

// Methods return the last expression — no `return` needed
class Math {
  static square(n) { n * n }
  static foo {
    var x = 2
    return x         // only needed for early return
  }
}

// Functions and closures
var doubled = [1, 2, 3].map { |n| n * 2 }

// String interpolation
System.log("Hello %(name)!")

// Control flow
if (count > 10) { System.print("high") }
for (item in [1, 2, 3]) { System.print(item) }
for (i in 0...100) { System.print(i) }

// Conditions are expressions
var label = count > 10 ? "yes" : "no"

// Lists and Maps
var list = [1, 2, 3]
list.add(4)
var map = { "key": "value" }
map["name"] = "Bob"

// Filters and chains
var positives = [1, -2, 3].where { |n| n > 0 }
var names = users.map { |u| u.name }.join(", ")
```

## Why Wren?

**Familiar syntax.** If you've used C, Java, JavaScript, or Python, Wren reads
naturally. Classes, methods, `if`/`while`/`for` — it's all there without
surprises.

**Truly object-oriented.** Wren doesn’t split the world into primitives and
objects. Everything is an object — classes, instances, booleans, null, even the
number -3.5. You can ask anything for its type: both `Num.name` and
`(-3.5).type.name` output Num. No primitives, no wrappers; it’s objects all the
way down, which keeps the language small, consistent, and predictable.

**Designed for embedding.** The Wren VM is clean, well-documented C with no
dependencies. Bialet creates and destroys a fresh VM for every request — no
state leaks between users, no garbage-collection surprises under load.

## Try Wren Online

The upstream language has an interactive playground where you can experiment
with Wren syntax outside of Bialet:

**[wren.io/try/](https://wren.io/try/)**

## Values and Types

Wren has a small, predictable set of built-in types.

### Booleans

```wren
true
false
```

`null` is false; everything else (including `0` and `""`) is true.

### Numbers

```wren
var a = 42
var b = 3.14
var c = 0xff      // hex literal
```

All numbers are 64-bit floats (IEEE 754). Arithmetic: `+`, `-`, `*`, `/`, `%`,
plus bitwise `&`, `|`, `^`, `~`, `<<`, `>>`.

Convert strings to numbers with `Num.fromString()` — essential in Bialet since
database values come back as strings:

```wren
var votes = Num.fromString(row["votes"])
```

### Strings

```wren
var s = "hello"
var t = s + " world"   // "hello world"
```

Strings are immutable. Index with `s[i]` to get a single-character string at
byte offset `i`. `.count` is the byte length.

### Null

```wren
var val = null
```

In Bialet, `null` is safe: accessing a key or calling a method on null returns
null instead of throwing an error. This makes template interpolation forgiving
when data is missing.

### Lists

```wren
var list = [1, 2, 3, 4]
list.add(5)
list[0]       // 1
list.count    // 5
```

### Maps

```wren
var map = { "name": "Alice", "age": "30" }
map["name"]                     // "Alice"
map.containsKey("email")        // false
```

### Ranges

```wren
1..5      // inclusive: 1, 2, 3, 4, 5
1...5     // exclusive: 1, 2, 3, 4
```

## Variables and Blocks

```wren
var x = 10
var y = x + 1
```

Variables are block-scoped. A block is delimited by `{ }`:

```wren
{
  var inside = "visible here"
}
// System.print(inside) — ERROR, not accessible
```

Closures capture variables from enclosing scopes:

```wren
var counter = 0
var increment = Fn.new { counter = counter + 1 }
increment.call()
System.print(counter)   // 1
```

## Control Flow

### If / Else

```wren
if (score > 10) {
  System.print("high")
} else if (score > 5) {
  System.print("medium")
} else {
  System.print("low")
}
```

### While

```wren
var i = 0
while (i < 5) {
  System.print(i)
  i = i + 1
}
```

### For

Wren's `for` iterates over any sequence — lists, maps, ranges, strings:

```wren
for (item in [1, 2, 3]) {
  System.print(item)
}

for (i in 0...100) {
  System.print(i)   // 0 through 99
}
```

### Ternary (Conditional)

Wren doesn't have `?:` as a dedicated operator. But `if`/`else` is an expression
— the condition with a `?` prefix:

```wren
var label = totalVotes > 0 ? "active" : "empty"
```

### Break and Continue

```wren
for (item in list) {
  if (item == null) continue
  if (item == "stop") break
  System.print(item)
}
```

No `break` with a label, no `switch`/`case`.

## Functions

Functions are first-class values created with `Fn.new`:

```wren
var add = Fn.new { |a, b| a + b }
add.call(3, 4)   // 7
```

The parameter list uses pipe syntax: `|a, b|`.

### Closures as Blocks

Wren has a sugary shorthand for closures passed as the last argument to a method
— called a _block argument_:

```wren
[1, 2, 3].map { |n| n * 2 }          // [2, 4, 6]

items.where { |item| item != null }   // filter in place
```

Use `_` for the argument name when you don't need it:

```wren
items.where { |_| true }
```

This is the same `Fn` you'd create with `Fn.new` — just more readable when
chaining.

## Classes

```wren
class Animal {
  construct new(name) {
    _name = name
  }

  name { _name }
  name=(n) { _name = n }

  speak { "(silence)" }
}

class Dog is Animal {
  construct new(name, breed) {
    super(name)
    _breed = breed
  }

  speak { "Woof!" }
}
```

Wren is single-inheritance. Use `super(...)` to call the parent constructor and
`super.method()` to call overridden methods.

### Getters and Setters

Methods with zero arguments that are accessed without `()` are getters:

```wren
var d = Dog.new("Rex", "Labrador")
d.name          // getter, returns "Rex"
d.name = "Max"  // setter
```

### Static Methods

```wren
class MathUtil {
  static square(n) { n * n }
}
MathUtil.square(5)   // 25
```

### Privacy Convention

Wren has no `private` keyword. Prefix field and method names with an underscore
to signal they're internal:

```wren
class Poll {
  votes_(opt) { Num.fromString(opt["votes"]) }
}
```

### The `is` Operator

Check if an object is an instance of a class:

```wren
var d = Dog.new("Rex", "Labrador")
d is Dog       // true
d is Animal    // true
d is String    // false
```

## Single-Expression Blocks

This is Wren's most distinctive feature and the one that makes Bialet code
unusually compact.

A block (method body or class body) that ends with an expression returns that
expression automatically — no `return` keyword needed:

```wren
class Poll {
  construct new() {
    _opts = null
  }

  // Single-expression getter: caches and returns query results
  options { _opts || (_opts = `SELECT * FROM poll`.fetch) }

  // Multi-statement method: performs a side effect (UPDATE), clears cache.
  // Last expression (_opts = null) evaluates to null — no return needed.
  vote(opt) {
    `UPDATE poll SET votes = votes + 1 WHERE id = ?`.query(opt)
    _opts = null
  }

  // Reduce over cached options — reads like a description, not plumbing
  totalVotes { options.reduce(0) { |sum, opt| sum + votes_(opt) } }

  // Ternary in a single expression
  percentage(opt) { totalVotes > 0 ? ((votes_(opt) / totalVotes) * 100).round : 0 }

  votes_(opt) { Num.fromString(opt["votes"]) }
}
```

This is a complete domain class. No ORM, no annotations, no configuration — just
Wren methods that express the logic directly.

### When You Need `return`

For multiline methods that branch early, use explicit `return`:

```wren
static footer {
  if (requestHasVoted) {
    return <footer>You voted. Thanks!</footer>
  }
  return <footer>Cast your vote above.</footer>
}
```

Bialet's template system leans on this: a template function is typically a
single-expression method whose body is an HTML string block — no `return`
clutter.

## String Interpolation

Use `%(expr)` anywhere in a string:

```wren
var name = "Alice"
"Hello, %(name)!"   // "Hello, Alice!"

var total = 42
System.log("Found %(total) results")
```

Any expression works, not just variables:

```wren
"Users online: %(onlineUsers.count - idleUsers.count)"

"<a href='/user/%(user["id"])'>%(user["name"])</a>"
```

### Templates: HTML String Blocks

String interpolation is the foundation of Bialet's template syntax. When you
write:

```html
<main>
  <h1>{{ title }}</h1>
  <ul>
    {{ items.map { |item|
    <li>{{ item }}</li>
    } }}
  </ul>
</main>
```

The `<main>...</main>` is a string expression — Bialet transforms HTML inline
blocks into string literals with `%(...)` interpolation. `{{ }}` is syntactic
sugar for `%(...)` inside HTML blocks.

The result is a seamless flow between HTML markup and Wren logic. Iterate over
database rows and emit `<li>` tags without context-switching to a separate
template language. See the [Templates](template.md) guide for the full template
system including layout patterns, partials, and the `_app.wren` convention.

## Collections and Sequences

`Sequence` is the parent class of all iterable types (List, Map, Range, String).
It provides a rich set of higher-order methods:

### Transform

```wren
[1, 2, 3].map { |n| n * n }           // [1, 4, 9]
```

### Filter

```wren
[1, null, 3].where { |n| n != null }  // [1, 3]
```

### Reduce

```wren
[1, 2, 3].reduce(0) { |sum, n| sum + n }  // 6
```

### Test

```wren
[1, 2, 3].any { |n| n > 2 }   // true
[1, 2, 3].all { |n| n > 0 }   // true
items.contains("target")        // true/false
```

### Accumulate

```wren
[1, 2, 3].join(", ")           // "1, 2, 3"
[1, 2, 3].count                // 3
items.isEmpty                   // true/false
```

### Paginate

```wren
[1, 2, 3, 4, 5].take(2)        // [1, 2]
[1, 2, 3, 4, 5].skip(2)        // [3, 4, 5]
```

### Materialize

```wren
(1..5).toList                    // [1, 2, 3, 4, 5]
```

### Bialet Extensions

Bialet adds a few convenience methods:

```wren
[1, 2, 3].first                  // 1 (null for empty lists)
"<script>".safe                   // "&lt;script&gt;" (HTML escape)
"hello".upper                     // "HELLO"
"HELLO".lower                     // "hello"
"42".toNum                        // 42 (as Number)
"1".toBool                        // true
list.to(User)                     // map list of Maps into User instances
```

The `.to(Class)` method on Sequence and Map is especially useful with database
results:

```wren
var posts = `SELECT * FROM posts`.fetch.to(Post)
var post  = `SELECT * FROM posts WHERE id = ?`.first(1).to(Post)
```

## Imports

```wren
import "shared" for Shared
import "math" for Math, PI
import "liquids" for Water
import "beverages" for Coffee, Water as H2O, Tea
```

Use `as` to import a variable under a different name:

```wren
import "beverages" for Water as H2O
// var water = H2O.new()
```

Module paths are relative to the importing file. In Bialet you can also import
from GitHub:

```wren
import "gh:user/repo/module" for ClassName
```

See the [advanced routing guide](advanced-routing.md) for details on external imports and file-based
module resolution.

## Bialet vs. Standard Wren

Bialet uses a modified Wren VM with additions for web development. Here's what
differs from upstream Wren.

### No Concurrency

Bialet is single-threaded per request. Each HTTP request gets a fresh Wren VM —
created when the request arrives, destroyed after the response is sent. There is
no shared state between requests (use [SQLite](database.md) or
[Session](reference.md) for persistence).

Wren's cooperative multitasking features (`Fiber`) depend on VMs persisting
across yield points. Since Bialet tears down the VM after every request, the
concurrency model doesn't apply.

### Fiber Is Not Tested

The `Fiber` class exists in the embedded VM and `Fiber.abort()` is used
internally for error signaling. However, `Fiber.yield()`, `Fiber.call()`,
`Fiber.transfer()`, and all cooperative multitasking features are **untested and
unsupported** in Bialet.

Do not rely on Fibers for application logic. Use the database or session store
when you need to persist state across request cycles.

### Bialet-Specific Classes

Bialet adds these classes on top of standard Wren:

| Class      | Purpose                                                   |
| ---------- | --------------------------------------------------------- |
| `Query`    | Backtick SQL: `` `SELECT * FROM users`.fetch `` — see [Database](database.md) |
| `Request`  | Incoming HTTP request (method, URI, headers, body, files) — see [Reference](reference.md) |
| `Response` | Outgoing HTTP response (status, headers, body, redirect) — see [Reference](reference.md) |
| `Cookie`   | Parse, set, and delete cookies — see [Reference](reference.md) |
| `Session`  | Server-side session storage with CSRF protection — see [Reference](reference.md) |
| `Db`       | Database migrations and ORM-like save/delete — see [Reference](reference.md) |
| `Http`     | Outbound HTTP requests (GET/POST/PUT/DELETE) — see [Reference](reference.md) |
| `Date`     | Date and time with formatting, arithmetic, and timezones — see [Reference](reference.md) |
| `File`     | File uploads stored in SQLite — see [Reference](reference.md) |
| `Markdown` | Render Markdown to HTML — see [Reference](reference.md) |
| `Json`     | JSON parse and stringify — see [Reference](reference.md) |
| `Config`   | Key-value configuration store — see [Reference](reference.md) |
| `Cron`     | Scheduled tasks — see [Reference](reference.md) |
| `System`   | Logging and stdout output — see [Reference](reference.md) |
| `Util`     | Helpers: hashing, encoding, random strings, URL encoding — see [Reference](reference.md) |

The full API reference with method signatures and examples is on the
[Reference](reference.md) page.
