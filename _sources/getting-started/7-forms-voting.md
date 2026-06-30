# 7. Forms & Voting

Now we'll integrate everything into the main app with proper form handling.

## Create a Poll class

Add a `Poll` class to `_app.wren` to keep logic separate from presentation:

```wren
class Template {
  // ... your template methods
}

class Poll {
  construct new() {}
  options { `SELECT * FROM poll`.fetch() }
  vote(opt) { `UPDATE poll SET votes = votes + 1
               WHERE id = ?`.query(opt) }
}
```

- `construct new()` defines the constructor
- `options` is a getter — called like a property: `poll.options`
- `vote(opt)` takes the poll option ID and increments its vote count

## Build the vote form

In `index.wren`, import both classes and render the form:

```wren
import "_app" for Template, Poll

var poll = Poll.new()

return Template.layout(
  <form action="/" method="post">
    <h2>Has web development become overly complex?</h2>
    <ul>
      {{ poll.options.map {|opt| <li>
        <input type="radio" id="{{ opt["answer"] }}" name="vote"
               value="{{ opt["id"] }}" required />
        <label for="{{ opt["answer"] }}">
          <strong>{{ opt["answer"] }}</strong>
          <p>{{ opt["comment"] }}</p>
        </label>
      </li> } }}
    </ul>
    <button type="submit">Vote</button>
  </form>
)
```

## Handle the form POST

Handle form submission on the same page — show errors inline, then redirect
on success:

```wren
import "_app" for Template, Poll

var poll = Poll.new()

if (Request.isPost) {
  var vote = Request.post("vote")
  poll.vote(vote)
  Response.redirect("/results")
  return
}

return Template.layout(
  <form action="/" method="post">
    ...
  </form>
)
```

Key points:
- `Request.isPost` detects form submissions
- `Request.post("vote")` reads the submitted value
- `Response.redirect("/results")` sends the browser to the results page
- `return` after redirect stops further script execution

This is the standard Bialet form pattern: **check → process → redirect**.

---
**Previous:** [6. Migrations](6-migrations) &nbsp; | &nbsp; **Next:** [8. Results](8-results)
