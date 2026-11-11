import "_app" for Auth, Layout

var user = Auth.user
if (!user) return Response.redirect("/login")

var session = Session.new()
var id = Request.get("id")
var item = id ? `SELECT * FROM items WHERE id = ?`.first(id) : null
if (id && !item) return Response.notFound()

var error = null
var name = item ? item["name"] : ""
var sku = item ? (item["sku"] || "") : ""
var quantity = item ? item["quantity"] : "0"
var notes = item ? (item["notes"] || "") : ""
var photoId = item ? item["photo_id"] : null
var cancelUrl = item ? "/items/" + item["id"] : "/items"

if (Request.isPost) {
  name = (Request.post("name") || "").trim()
  sku = (Request.post("sku") || "").trim()
  notes = (Request.post("notes") || "").trim()
  quantity = (Request.post("quantity") || "").trim()
  if (quantity == "") quantity = "0"

  var qty = Num.fromString(quantity)
  if (!session.csrfOk) {
    error = "The form expired. Try again."
  } else if (name == "") {
    error = "A name is required."
  } else if (!qty || qty < 0) {
    error = "Quantity must be zero or more."
  } else {
    var file = Request.file("photo")
    if (file) {
      file.save
      photoId = file.id
    }
    if (item) {
      `UPDATE items SET name = ?, sku = ?, quantity = ?, notes = ?, photo_id = ?,
        updated_at = CURRENT_TIMESTAMP WHERE id = ?`.query([name, sku, qty, notes, photoId, item["id"]])
    } else {
      `INSERT INTO items (name, sku, quantity, notes, photo_id)
        VALUES (?, ?, ?, ?, ?)`.query([name, sku, qty, notes, photoId])
    }
    return Response.redirect("/items")
  }
}

return Layout.page(item ? "Edit item" : "New item", user,
  <form method="post" enctype="multipart/form-data" class="card narrow">
  {{ error && <p class="error">{{ error }}</p> }}
  {{ session.csrf }}
  <label>Name <input name="name" value="{{ name }}" required autofocus></label>
  <label>SKU <input name="sku" value="{{ sku }}"></label>
  <label>Quantity <input name="quantity" type="number" min="0" value="{{ quantity }}"></label>
  <label>Notes <textarea name="notes" rows="4">{{ notes }}</textarea></label>
  <label>Photo <input type="file" name="photo" accept="image/*"></label>
  {{ photoId ? <div><img class="photo" src="/photo?id={{ photoId }}" alt=""></div> : "" }}
  <div class="row">
    <button>{{ item ? "Save changes" : "Create item" }}</button>
    <a href="{{ cancelUrl }}">Cancel</a>
  </div>
</form>
)
