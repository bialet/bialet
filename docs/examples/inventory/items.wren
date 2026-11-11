import "_app" for Auth, Layout

var user = Auth.user
if (!user) return Response.redirect("/login")

// /items/<id> is the detail page; /items is the list. One file serves both.
var id = Request.route(0)
if (id) {
  var item = `SELECT * FROM items WHERE id = ?`.first(id)
  if (!item) return Response.notFound()

  var title = item["name"]
  var photoId = item["photo_id"]
  return Layout.page(title, user,
  <article class="card">
    <dl class="facts">
      <dt>SKU</dt><dd>{{ item["sku"] || "—" }}</dd>
      <dt>Quantity</dt><dd>{{ item["quantity"] }}</dd>
    </dl>
    {{ photoId ? <div><img class="photo" src="/photo?id={{ photoId }}" alt="{{ title }}"></div> : "" }}
    {{ item["notes"] ? <p class="notes">{{ item["notes"] }}</p> : "" }}
    <div class="row">
      <a class="button" href="/item?id={{ item["id"] }}">Edit</a>
      <form method="post" action="/item-delete" class="inline">
        {{ Session.new().csrf }}
        <input type="hidden" name="id" value="{{ item["id"] }}">
        <button class="danger">Delete</button>
      </form>
    </div>
    <p><a href="/items">Back to stock</a></p>
  </article>
)
}

var q = (Request.get("q") || "").trim()
var page = Num.fromString(Request.get("page") || "")
if (!page || page < 1) page = 1
var limit = 10
var offset = (page - 1) * limit

// One static query: an empty search matches every row.
var total = `
  SELECT COUNT(*) FROM items
  WHERE (? = '' OR name LIKE '%' || ? || '%' OR sku LIKE '%' || ? || '%')
`.toNum(q, q, q)
var items = `
  SELECT id, name, sku, quantity FROM items
  WHERE (? = '' OR name LIKE '%' || ? || '%' OR sku LIKE '%' || ? || '%')
  ORDER BY name LIMIT ? OFFSET ?
`.fetch([q, q, q, limit, offset])
var pages = ((total + limit - 1) / limit).floor

return Layout.page("Stock", user,
  <section>
  <form method="get" class="row search">
    <input name="q" value="{{ q }}" placeholder="Search name or SKU" aria-label="Search">
    <button>Search</button>
    <a class="button" href="/item">New item</a>
  </form>

  {{ items.count == 0 ?
    <p class="empty">No items{{ q != "" ? " match “" + q + "”" : " yet." }}</p> :
    <table>
      <thead><tr><th>Name</th><th>SKU</th><th class="num">Qty</th></tr></thead>
      <tbody>
        {{ items.map { |it| <tr>
          <td><a href="/items/{{ it["id"] }}">{{ it["name"] }}</a></td>
          <td>{{ it["sku"] || "—" }}</td>
          <td class="num">{{ it["quantity"] }}</td>
        </tr> } }}
      </tbody>
    </table>
  }}

  {{ pages > 1 ?
    <nav class="pager">
      {{ page > 1 ? <a href="/items?q={{ Util.urlEncode(q) }}&page={{ page - 1 }}">Previous</a> : "" }}
      <span>Page {{ page }} of {{ pages }}</span>
      {{ page < pages ? <a href="/items?q={{ Util.urlEncode(q) }}&page={{ page + 1 }}">Next</a> : "" }}
    </nav> : ""
  }}
</section>
)
