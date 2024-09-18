# Inline HTML Strings

Inline HTML Strings in Bialet offer a powerful way to embed HTML directly in your Wren code. These strings behave similarly to JSX in React but are purely for string interpolation, not object creation. Bialet's inline HTML allows you to dynamically create and manipulate HTML in a safe, predictable manner while avoiding common pitfalls like mismatched tags or invalid tag names.

## Basics

Inline HTML Strings are delimited by angle brackets `<` and `>`. The string content must begin and end with the same tag. Additionally, the tag name must be in lowercase, start with a letter, and can only include letters and numbers.

### Example

```wren
// Basic example of an Inline HTML String
var str = <p>Hello World</p>
System.print(str) // Outputs: "<p>Hello World</p>"
```

You can also use interpolation inside the string:

```wren
var str = <p>{{ 5 + 3 }}</p>
System.print(str) // Outputs: "<p>8</p>"
```

## Interpolation

The handlebars `{{ }}` provide string interpolation. You can place any Wren expression inside handlebars, and it will be evaluated and inserted into the string.

```wren
var name = "John"
var greeting = <h1>Hello, {{ name }}!</h1>
System.print(greeting) // Outputs: "<h1>Hello, John!</h1>"
```

You can also include conditional logic and ternary operators within the interpolation.

### Example with Ternary Operator:

```wren
var isLoggedIn = true
var message = <p>{{ isLoggedIn ? "Welcome back!" : "Please log in." }}</p>
System.print(message) // Outputs: "<p>Welcome back!</p>"
```

> **Note:** The ternary operator must be written on a single line. Multi-line ternary operators will result in a syntax error.

## Attributes

You can include HTML attributes as you would in a regular HTML document. Attributes must be valid, lowercase HTML names.

### Example:

```wren
var link = <a href="#">Click me</a>
System.print(link) // Outputs: "<a href="#">Click me</a>"
```

## Common Errors

### Mismatched Tags

Inline HTML Strings must start and end with the same tag. Mismatching tags will result in a parsing error.

```wren
// This will fail
var wrong = <div><span>Hello</div>
```

### Invalid Tag Names

Tag names must only contain letters and numbers. Special characters like hyphens (`-`) are not allowed.

```wren
// This will fail
var invalidTag = <custom-tag>Invalid</custom-tag>
```

### Multi-line Attributes

Multi-line attributes are not supported. Attributes must be defined on a single line.

```wren
// This will fail
var multiLineAttr = <div class="example"
                         data-value="example"></div>
```

## Advanced Features

### Nesting Interpolations

You can nest up to 9 levels of interpolations inside each other. This allows for more complex dynamic HTML structures.

```wren
var name = "John"
var nested = <p>{{ true && <span>{{ name }}</span> }}</p>
System.print(nested) // Outputs: "<p><span>John</span></p>"
```

### Iteration with `map`

You can use Wren's `map` function to generate lists or other repetitive structures.

```wren
var list = ["Apple", "Banana", "Cherry"]
var htmlList = <ul>{{ list.map { |item| <li>{{ item }}</li> } }}</ul>
System.print(htmlList) 
// Outputs: "<ul><li>Apple</li><li>Banana</li><li>Cherry</li></ul>"
```

> **Note:** The `map` block must be written on a single line.

## Error Handling

Here are some common issues you may encounter:

1. **Invalid Tag Names**: Tags must begin with a letter and can only include alphanumeric characters.
2. **Mismatched Tags**: Ensure that your opening and closing tags are the same.
3. **Ternary Operators on Multiple Lines**: Ternary operators must be fully on one line inside interpolations.
4. **Incorrect Interpolation**: Ensure spaces are present between code blocks and handlebars in `map` or other functions.

### Example of Correct and Incorrect Usages:

```wren
// Correct
var list = ["Item1", "Item2"]
var correctMap = <ul>{{ list.map { |item| <li>{{ item }}</li> } }}</ul>

// Incorrect
var incorrectMap = <ul>{{ list.map { |item| 
    <li>{{ item }}</li> 
} }}</ul>
```

## Conclusion

Inline HTML Strings in Bialet give you the flexibility to generate dynamic HTML content with ease. By following the rules of tag names, interpolation, and attributes, you'll be able to avoid common pitfalls and create robust HTML templates. Remember to take advantage of Wren's built-in features like `map` and ternary operators to simplify your code.e can use HTML strings in our bialet.
