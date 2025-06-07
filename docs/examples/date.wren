// Global UTC is zero by default
// We set it to -3 for GMT-3
Date.utc = -3

var date = Date.new()
var current = Date.now

// We use the `isPost` property to check if the request method is POST
if (Request.isPost) {
  // Update the date
  var d = Request.post("date")
  var t = Request.post("time")
  date = Date.new("%(d) %(t)", Num.fromString(Request.post("utc")))
}

// Log the dates
System.print("Date: %(date) - Current: %(current)")

// We use the `return` to finish the script and send the response to the client.
// The `{{ ... )` syntax is used to interpolate the properties of the date object
// and also any other string.
// Apart from the interpolation, the string is regular HTML.
return <!doctype html>
<html>
  <head>
    <meta charset="utf-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <link rel="stylesheet" href="style.css">
  </head>
  <body>
    <h1>Date and Time functions</h1>
    <p>Date and time: {{date}}</p>
    <p>Current date and time: {{current}}</p>
    <p>{{ date > current ? "Date is greater than current" : "" }}</p>
    <p>{{ date < current ? "Date is less than current" : "" }}</p>
    <p>{{ date == current ? "Date is equal to current" : "" }}</p>
    <p>{{ date != current ? "Date is not equal to current" : "" }}</p>
    <p>UTC: {{date.utc}}</p>
    <p>Year: {{date.year}}</p>
    <p>Month: {{date.month}}</p>
    <p>Day: {{date.day}}</p>
    <p>Hour: {{date.hour}}</p>
    <p>Minute: {{date.minute}}</p>
    <p>Second: {{date.second}}</p>
    <p>Timestamp: {{date.unix}}</p>
    <p>Date: {{date.date}}</p>
    <p>Time: {{date.time}}</p>
    <p>US Format: {{date.format('#m/#d/#Y')}}</p>
    <h2>Change date</h2>
    <form method="POST">
      <p>
        <label>
          Date
          <input type="date" name="date" value="{{ date.format('#Y-#m-#d') }}">
        </label>
      </p>
      <p>
        <label>
          Time
          <input type="input" name="time" value="{{ date.format('#H:#M:#S') }}">
        </label>
      </p>
      <p>
        <label>
          UTC
          <input type="input" name="utc" value="{{ date.utc }}">
        </label>
      </p
      <p>
        <button>Change</button>
      </p>
    </form>
    <p><a href=".">Back ↩️</a></p>
  </body>
</html>
