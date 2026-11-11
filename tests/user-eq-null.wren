// A user class's == may dereference its operand, but `value == null` must not
// reach it: the VM compares a null operand by identity first.
class Item {
  construct new(id) { _id = id }
  id { _id }
  ==(o) { _id == o.id }
  !=(o) { _id != o.id }
}

var item = Item.new(7)
var nothing

var values = [
  item == null,        // false
  item != null,        // true
  item == nothing,     // false
  null == item,        // false
  null != item,        // true
  item == Item.new(7)  // true
]

var out = []
for (value in values) out.add(value.toString)
return out.join(",")
