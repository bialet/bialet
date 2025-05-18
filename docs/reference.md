# Reference

## Request
A class to handle incoming HTTP requests, parsing their content, and providing convenient access to their components.

### init(message, route)
Initializes a new Request object with a given message and route.
- `message`: The raw HTTP request message.
- `route`: The route the request is targeting.

### parseQuery(query)
Parses the query string of the request.
- `query`: The query string part of the URL.

### method
Returns the HTTP method of the request.

### uri
Returns the URI of the request.

### body
Returns the body content of the request.

### isPost
Checks if the request method is POST.

### isJson
Checks if the request's content type is JSON.

### header(name)
Returns the value of a specified request header.
- `name`: The name of the header.

### get(name)
Returns the value of a specified query parameter.
- `name`: The name of the query parameter.

### post(name)
Returns the value of a specified POST parameter.
- `name`: The name of the POST parameter.

### route(pos)
Returns a specific part of the request route.
- `pos`: The position of the route segment.

### file(name)
Fetches a file from the database based on its name and returns it as a `File` object if found. If the file is not found, returns `null`.
- `name`: The name of the file to retrieve.

## Response
A class to construct and manage HTTP responses, including setting headers, cookies, and the response body.

### init()
Initializes a new Response object.

### out
Returns the current output buffer, trimmed.

### status
Returns the current HTTP status code.

### headers
Returns the formatted response headers as a string.

### out(out)
Appends a string to the response body.
- `out`: The string to append.

### status(status)
Sets the HTTP status code for the response.
- `status`: The HTTP status code.

### addCookieHeader(value)
Adds a 'Set-Cookie' header to the response.
- `value`: The cookie value.

### header(header, value)
Sets a response header.
- `header`: The name of the header.
- `value`: The value of the header.

### json(data)
Sends a JSON response.
- `data`: The data to be JSON-encoded.

### file(id)
Fetches the file type based on its ID and output the file, setting the appropriate headers.
- `id`: The ID of the file to fetch.

### page(title, message)
Generates a simple HTML page response.
- `title`: The title of the page.
- `message`: The message to display on the page.

### end(code, title, message)
Ends the response with a specific status code and a simple HTML page.
- `code`: The HTTP status code.
- `title`: The title for the HTML page.
- `message`: The message for the HTML page.

### redirect(url)
Redirects the client to a specified URL.
- `url`: The target URL.

### forbidden()
Sends a 403 Forbidden response.

### login()
Generates a login page response.

## Cookie
A class for managing cookies, including parsing, setting, and deleting them.

### init()
Initializes a new Cookie object.

### parseHeader(headerValue)
Parses a 'Set-Cookie' header value.
- `headerValue`: The 'Set-Cookie' header value.

### set(name, value, options)
Sets a cookie with options.
- `name`: The name of the cookie.
- `value`: The value of the cookie.
- `options`: Additional options for the cookie (e.g., path, domain, secure).

### set(name, value)
Sets a cookie without options.
- `name`: The name of the cookie.
- `value`: The value of the cookie.

### delete(name)
Deletes a cookie.
- `name`: The name of the cookie to delete.

### get(name, default)
Returns the value of a specified cookie, or a default value if the cookie is not found.
- `name`: The name of the cookie.
- `default`: The default value to return if the cookie is not found.

### get(name)
Returns the value of a specified cookie.
- `name`: The name of the cookie.

## Session
A class for managing session data, including setting and destroying sessions.

### name
Returns the name of the session cookie.

### name=(n)
Sets the name of the session cookie.
- `n`: The new name for the session cookie.

### destroy()
Destroys the current session.

## Json
A class for handling JSON data, including parsing and stringifying.

### parse(string)
Parses a JSON string.
- `string`: The JSON string to parse.

### stringify(object)
Converts an object to a JSON string.
- `object`: The object to stringify.

## Util
A utility class providing various static helper methods.

### randomString(length)
Generates a random string of a specified length.
- `length`: The desired length of the random string.

### hash(password)
Generates a hash for the given password.
- `password`: The password to hash.

### verify(password, hash)
Verifies if a given password matches a hash.
- `password`: The password to verify.
- `hash`: The hash to compare against the password.

### toNum(val)
Converts a given value to a numeric type.
- `val`: The value to convert.

### hexToDec(hexStr)
Converts a hexadecimal string to its decimal equivalent.
- `hexStr`: The hexadecimal string to convert.

### toHex(byte)
Converts a byte to its hexadecimal string representation.
- `byte`: The byte to convert.

### urlDecode(str)
Decodes a URL-encoded string.
- `str`: The URL-encoded string to decode.

### lpad(s, count, with)
Left-pads a string with a specified character to a specified length.
- `s`: The string to pad.
- `count`: The desired length of the string after padding.
- `with`: The character to pad the string with.

### reverse(str)
Reverses a given string.
- `str`: The string to reverse.

### getPositionForIndex(text, index)
Finds the line and column position in a text for a specified index.
- `text`: The text to search.
- `index`: The index in the text.

### htmlEscape(str)
Escapes special HTML characters in a string to prevent XSS attacks.
- `str`: The string to escape.

### urlEncode(str)
Encodes a string for use in a URL by replacing certain characters with their URL-safe equivalents.
- `str`: The string to encode.

### params(params)
Encodes a dictionary of parameters into a URL-encoded string.
- `params`: A dictionary of key-value pairs to encode.

### encodeBase64(input)
Encodes a given input string to Base64.
- `input`: The string to encode.

### decodeBase64(input)
Decodes a Base64 encoded string.
- `input`: The Base64 encoded string to decode.

## Config
A class for managing configuration settings, with methods to get, set, and delete configuration options.

### get(key)
Retrieves a configuration value by its key.
- `key`: The key of the configuration option.

### set(key, value)
Sets a configuration option to a given value.
- `key`: The key of the configuration option.
- `value`: The value to set.

### bool(key)
Retrieves a boolean configuration value.
- `key`: The key of the configuration option.

### num(key)
Retrieves a numeric configuration value.
- `key`: The key of the configuration option.

### delete(key)
Deletes a configuration option.
- `key`: The key of the configuration option to delete.

### json(key)
Retrieves a configuration value as JSON.
- `key`: The key of the configuration option.

### json(key, val)
Sets a configuration option to a JSON value.
- `key`: The key of the configuration option.
- `val`: The JSON value to set.

## Db
A class for database interactions, providing basic methods for migrations and data manipulation.

### init()
Initializes the database connection.

### migrate(version, schema)
Performs database migrations to a specified version using the given schema.
- `version`: The target version of the database schema.
- `schema`: The schema to use for the migration.

### clean
Removes expired sessions and temporary files from the database.

### save(table, values)
Saves data to a specified table.
- `table`: The name of the table to save data to.
- `values`: The data to save, typically as a key-value pair object.

## Http
A class for handling HTTP requests and responses, with methods to perform various types of HTTP requests.

### request(url, method, data, options)
Performs an HTTP request with the specified options.
- `url`: The URL to send the request to.
- `method`: The HTTP method (e.g., GET, POST).
- `data`: The data to send with the request.
- `options`: Additional request options.

### get(url, options)
Performs a GET request to the specified URL with the given options.
- `url`: The URL to send the request to.
- `options`: Additional request options.

### post(url, data, options)
Performs a POST request to the specified URL with the given data and options.
- `url`: The URL to send the request to.
- `data`: The data to send with the request.
- `options`: Additional request options.

### put(url, data, options)
Performs a PUT request to the specified URL with the given data and options.
- `url`: The URL to send the request to.
- `data`: The data to send with the request.
- `options`: Additional request options.

### delete(url, options)
Performs a DELETE request to the specified URL with the given options.
- `url`: The URL to send the request to.
- `options`: Additional request options.

### get(url)
Performs a simple GET request to the specified URL.
- `url`: The URL to send the request to.

### post(url, data)
Performs a simple POST request to the specified URL with the given data.
- `url`: The URL to send the request to.
- `data`: The data to send with the request.

### put(url, data)
Performs a simple PUT request to the specified URL with the given data.
- `url`: The URL to send the request to.
- `data`: The data to send with the request.

### delete(url)
Performs a simple DELETE request to the specified URL.
- `url`: The URL to send the request to.

(date-reference)=
## Date
A class for managing date and time operations, supporting multiple constructors and methods for formatting, comparing, and manipulating dates relative to UTC.

### new()
Creates a new `Date` object representing the current date and time in UTC.

### new(date)
Creates a new `Date` object from a provided date string.
- `date`: The date string to initialize the `Date` object.

### new(date, utc)
Creates a new `Date` object from a provided date string and a specified UTC offset.
- `date`: The date string to initialize the `Date` object.
- `utc`: The UTC offset to apply.

### new(year, month, day, hour, minute, second)
Creates a new `Date` object with the provided date and time components.
- `year`: The year component.
- `month`: The month component.
- `day`: The day component.
- `hour`: The hour component.
- `minute`: The minute component.
- `second`: The second component.

### new(year, month, day, hour, minute, second, utc)
Creates a new `Date` object with the provided date and time components and a specified UTC offset.
- `year`: The year component.
- `month`: The month component.
- `day`: The day component.
- `hour`: The hour component.
- `minute`: The minute component.
- `second`: The second component.
- `utc`: The UTC offset to apply.

### new(year, month, day)
Creates a new `Date` object with the provided date, setting the time to `00:00:00`.
- `year`: The year component.
- `month`: The month component.
- `day`: The day component.

### new(year, month, day, utc)
Creates a new `Date` object with the provided date and a specified UTC offset, setting the time to `00:00:00`.
- `year`: The year component.
- `month`: The month component.
- `day`: The day component.
- `utc`: The UTC offset to apply.

### utc=(utc)
Sets the UTC offset for the current `Date` object.
- `utc`: The UTC offset to apply.

### utc
Returns the UTC offset for the current `Date` object.

### format(format)
Formats the date according to the provided format string.
- `format`: The format string to apply.

### year
Returns the year component of the date.

### month
Returns the month component of the date.

### day
Returns the day component of the date.

### hour
Returns the hour component of the date.

### minute
Returns the minute component of the date.

### second
Returns the second component of the date.

### weekday
Returns the weekday number (0 for Sunday, 6 for Saturday).

### dayOfYear
Returns the day of the year (1-365).

### date
Returns the date in `YYYY-MM-DD` format.

### time
Returns the time in `HH:MM:SS` format.

### unix
Returns the Unix timestamp of the date.

### iso
Returns the ISO 8601 formatted date (`HH:MM:SS`).

### inUtc
Returns the date in UTC format.

### toString
Returns the date as an ISO 8601 string (`YYYY-MM-DDTHH:MM:SS`).

### +(plus)
Adds a date or time interval to the date.
- `plus`: The date or time interval to add.

### -(minus)
Subtracts a date or time interval from the date.
- `minus`: The date or time interval to subtract.

### diff(otherDate)
Returns the difference between the current date and `otherDate`.
- `otherDate`: The `Date` object to compare against.

## File

### new(data)
Constructor that sets the file data.
- `data`: The data Map representing the file.

### get(id)
Fetches a file from the database by its ID if it is not temporary and sets the file attributes.
- `id`: The ID of the file to retrieve.

### create(name, type, file, size)
Creates a new file in the database with the given name, type, content, and size.
- `name`: The name of the file.
- `type`: The MIME type of the file.
- `file`: The file content.
- `size`: The size of the file.

### create(name, type, file)
Creates a new file in the database with the given name, type, and content. The size is determined by the content.
- `name`: The name of the file.
- `type`: The MIME type of the file.
- `file`: The file content.

### destroy
Deletes the file from the database based on its ID.

### save
Marks the file as permanent by setting `isTemp` to false in the database.

### temp
Marks the file as temporary by setting `isTemp` to true in the database.

## Cron

### Cron.every(minutes)

Runs the job every `minutes`. It checks if the current minute is divisible by the given value.

- `minutes` (Number): Interval in minutes.
- Returns: `Bool` indicating if the job ran.

### Cron.at(hour, minute)

Runs the job at a specific hour and minute.

- `hour` (Number): Hour of the day (0–23).
- `minute` (Number): Minute of the hour (0–59).
- Returns: `Bool` indicating if the job ran.

### Cron.at(hour, minute, dayOfWeek)

Runs the job at a specific hour, minute, and day of the week.

- `hour` (Number): Hour of the day (0–23).
- `minute` (Number): Minute of the hour (0–59).
- `dayOfWeek` (Number): Day of the week (0=Sunday, 6=Saturday).
- Returns: `Bool` indicating if the job ran.
