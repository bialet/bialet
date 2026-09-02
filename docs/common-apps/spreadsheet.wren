import "/_auth" for Auth
import "/_google" for Google
import "/_template" for Template

var guard = Auth.require
if (guard) return guard

var sess = Session.new()
var error = null
var info = null
var note = null

var sheetId = Config.get("sheet_id") || ""
var range = Config.get("sheet_range") || "Sheet1!A1:F25"
var sheetRows = []
var connected = Google.connected

if (Request.isPost) {
  if (!sess.csrfOk) {
    error = "Your session expired. Try again."
  } else {
    var action = Request.post("action") || ""
    sheetId = Google.sheetIdFrom(Request.post("sheet"))
    range = (Request.post("range") || "").trim()
    if (range == "") range = "Sheet1!A1:F25"
    if (sheetId == "") {
      error = "Paste a spreadsheet URL or id."
    } else {
      Config.set("sheet_id", sheetId)
      Config.set("sheet_range", range)
      if (action == "append") {
        var text = Request.post("values") || ""
        var rows = []
        for (line in text.split("\n")) {
          line = line.trim()
          if (line == "") continue
          var row = []
          for (cell in line.split(",")) {
            row.add(cell.trim())
          }
          rows.add(row)
        }
        if (rows.count == 0) {
          error = "Paste at least one row of comma-separated values."
        } else {
          var res = Google.sheetAppend(sheetId, range, rows)
          if (res["ok"]) {
            info = "Appended %(rows.count) row(s) to %(range)."
            var read = Google.sheetFetch(sheetId, range)
            if (read["ok"]) {
              var data = read["data"]
              sheetRows = (data != null && data["values"] != null) ? data["values"] : []
            }
          } else {
            error = res["error"]
          }
        }
      } else if (action == "read") {
        var res = Google.sheetFetch(sheetId, range)
        if (res["ok"]) {
          var data = res["data"]
          sheetRows = (data != null && data["values"] != null) ? data["values"] : []
        } else {
          error = res["error"]
        }
      }
    }
  }
} else if (connected && sheetId != "") {
  // Auto-load the saved range on a plain GET so the page doubles as a
  // read-only dashboard.
  var res = Google.sheetFetch(sheetId, range)
  if (res["ok"]) {
    var data = res["data"]
    sheetRows = (data != null && data["values"] != null) ? data["values"] : []
  } else {
    note = res["error"]
  }
}

var headRow = sheetRows.count > 0 ? sheetRows[0] : []
var bodyRows = []
var i = 1
while (i < sheetRows.count) {
  bodyRows.add(sheetRows[i])
  i = i + 1
}

var tableBlock
if (headRow.count > 0) {
  tableBlock = <div class="table-wrap">
    <table class="sheet">
      <thead>
        <tr>{{ headRow.map { |cell| <th>{{ cell == null ? "" : cell }}</th> } }}</tr>
      </thead>
      <tbody>
        {{ bodyRows.map { |row| <tr>{{ row.map { |cell| <td>{{ cell == null ? "" : cell }}</td> } }}</tr> } }}
      </tbody>
    </table>
  </div>
} else {
  tableBlock = <p class="muted">No data in that range yet.</p>
}

var content = <main>
  <h1>Spreadsheet</h1>

  {{ !connected && <div class="flash warn">This demo needs a connected Google
    account. <a href="/google/connect">Connect Google</a> first.</div> }}
  {{ info != null && <div class="flash ok">{{ info }}</div> }}
  {{ error != null && <div class="flash error">{{ error }}</div> }}

  <section class="card">
    <h2>Load a range</h2>
    <form method="post" action="/spreadsheet" class="grid">
      {{ sess.csrf }}
      <input type="hidden" name="action" value="read" />
      <label>Spreadsheet URL or id
        <input type="text" name="sheet" value="{{ sheetId }}" placeholder="https://docs.google.com/spreadsheets/d/1ABC.../edit" />
      </label>
      <label>Range
        <input type="text" name="range" value="{{ range }}" placeholder="Sheet1!A1:F25" />
      </label>
      <div class="actions">
        <button class="btn">Load</button>
      </div>
    </form>
  </section>

  <section class="card">
    <h2>Current content of {{ range }}</h2>
    {{ note != null && <p class="muted">{{ note }}</p> }}
    {{ tableBlock }}
  </section>

  <section class="card">
    <h2>Append rows</h2>
    <form method="post" action="/spreadsheet" class="stack">
      {{ sess.csrf }}
      <input type="hidden" name="action" value="append" />
      <label>Spreadsheet URL or id
        <input type="text" name="sheet" value="{{ sheetId }}" placeholder="https://docs.google.com/spreadsheets/d/1ABC.../edit" />
      </label>
      <label>Range to append below
        <input type="text" name="range" value="{{ range }}" placeholder="Sheet1!A1" />
      </label>
      <label>New rows (one per line, cells separated by commas)
        <textarea name="values" rows="4" placeholder="Ada,ada@example.com,true"></textarea>
      </label>
      <button class="btn">Append</button>
    </form>
  </section>
</main>

return Template.new().layout("Spreadsheet", content, Auth.user)
