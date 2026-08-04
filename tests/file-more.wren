var created = File.create("file-more.txt", "text/plain", "file-more-content")
var id = created.id
var fetched = File.get(id)
var name = fetched.name
var type = fetched.type
var size = fetched.size
var freshCreatedAt = created.createdAt
var dbCreatedAt = fetched.createdAt
created.temp
var afterTemp = File.get(id)
created.save
var afterSave = File.get(id)
created.destroy
var afterDestroy = File.get(id)

return "id:" + id.toString + "|name:" + name + "|type:" + type + "|size:" + size.toString +
  "|freshCreatedAt:" + (freshCreatedAt == null).toString +
  "|dbCreatedAt:" + (dbCreatedAt != null).toString +
  "|tempBlocksGet:" + (afterTemp.id == null).toString +
  "|saveRestores:" + (afterSave.id == id).toString +
  "|destroy:" + (afterDestroy.id == null).toString
