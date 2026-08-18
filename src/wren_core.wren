class Bool {
  [key] { null }
  map(f) { List.new() }
  count { 0 }
  safe { toString.safe }
}
class Fiber { safe { toString.safe } }
class Fn { safe { toString.safe } }
class Num { safe { toString.safe } }

// Null is widely used in Bialet in the templates.
// This way allow calling methods on null like a List or a Map
// and don't crash the program.
class Null {
  [key] { null }
  map(f) { List.new() }
  count { 0 }
  to(Class) { this }
  safe { toString.safe }
}

class Sequence {
  all(f) {
    var result = true
    for (element in this) {
      result = f.call(element)
      if (!result) return result
    }
    return result
  }

  any(f) {
    var result = false
    for (element in this) {
      result = f.call(element)
      if (result) return result
    }
    return result
  }

  contains(element) {
    for (item in this) {
      if (element == item) return true
    }
    return false
  }

  count {
    var result = 0
    for (element in this) {
      result = result + 1
    }
    return result
  }

  count(f) {
    var result = 0
    for (element in this) {
      if (f.call(element)) result = result + 1
    }
    return result
  }

  each(f) {
    for (element in this) {
      f.call(element)
    }
  }

  isEmpty { iterate(null) ? false : true }

  map(transformation) { MapSequence.new(this, transformation) }
  to(Class) { map{ |element| Class.new(element) } }

  skip(count) {
    if (!(count is Num) || !count.isInteger || count < 0) {
      Fiber.abort("Count must be a non-negative integer.")
    }

    return SkipSequence.new(this, count)
  }

  take(count) {
    if (!(count is Num) || !count.isInteger || count < 0) {
      Fiber.abort("Count must be a non-negative integer.")
    }

    return TakeSequence.new(this, count)
  }

  where(predicate) { WhereSequence.new(this, predicate) }

  reduce(acc, f) {
    for (element in this) {
      acc = f.call(acc, element)
    }
    return acc
  }

  reduce(f) {
    var iter = iterate(null)
    if (!iter) Fiber.abort("Can't reduce an empty sequence.")

    // Seed with the first element.
    var result = iteratorValue(iter)
    while (iter = iterate(iter)) {
      result = f.call(result, iteratorValue(iter))
    }

    return result
  }

  join() { join("") }

  join(sep) {
    var strings = []
    for (element in this) {
      strings.add(element.toString)
    }
    return strings.joinNative(sep)
  }

  joinInt_() {
    var strings = []
    for (element in this) {
      if (element.type != Bool || element) strings.add(element.toString)
    }
    return strings.joinNative("")
  }

  slice(start) { slice(start, -1) }
  slice(start, end) {
    var list = toList
    if (end < 0) {
      end = list.count
    }
    var result = []
    for (index in start...end) {
      result.add(list[index])
    }
    return result
  }

  toList {
    var result = List.new()
    for (element in this) {
      result.add(element)
    }
    return result
  }

  safe { toString.safe }
}

class MapSequence is Sequence {
  construct new(sequence, fn) {
    _sequence = sequence
    _fn = fn
  }

  iterate(iterator) { _sequence.iterate(iterator) }
  iteratorValue(iterator) { _fn.call(_sequence.iteratorValue(iterator)) }
  safe { join("") }
  toString { join("") }
}

class SkipSequence is Sequence {
  construct new(sequence, count) {
    _sequence = sequence
    _count = count
  }

  iterate(iterator) {
    if (iterator) {
      return _sequence.iterate(iterator)
    } else {
      iterator = _sequence.iterate(iterator)
      var count = _count
      while (count > 0 && iterator) {
        iterator = _sequence.iterate(iterator)
        count = count - 1
      }
      return iterator
    }
  }

  iteratorValue(iterator) { _sequence.iteratorValue(iterator) }
}

class TakeSequence is Sequence {
  construct new(sequence, count) {
    _sequence = sequence
    _count = count
  }

  iterate(iterator) {
    if (!iterator) _taken = 1 else _taken = _taken + 1
    return _taken > _count ? null : _sequence.iterate(iterator)
  }

  iteratorValue(iterator) { _sequence.iteratorValue(iterator) }
}

class WhereSequence is Sequence {
  construct new(sequence, fn) {
    _sequence = sequence
    _fn = fn
  }

  iterate(iterator) {
    while (iterator = _sequence.iterate(iterator)) {
      if (_fn.call(_sequence.iteratorValue(iterator))) break
    }
    return iterator
  }

  iteratorValue(iterator) { _sequence.iteratorValue(iterator) }
}

class String is Sequence {
  bytes { StringByteSequence.new(this) }
  codePoints { StringCodePointSequence.new(this) }

  split(delimiter) {
    if (!(delimiter is String) || delimiter.isEmpty) {
      Fiber.abort("Delimiter must be a non-empty string.")
    }

    var result = []

    var last = 0
    var index = 0

    var delimSize = delimiter.byteCount_
    var size = byteCount_

    while (last < size && (index = indexOf(delimiter, last)) != -1) {
      result.add(this[last...index])
      last = index + delimSize
    }

    if (last < size) {
      result.add(this[last..-1])
    } else {
      result.add("")
    }
    return result
  }

  replace(from, to) {
    if (!(from is String) || from.isEmpty) {
      Fiber.abort("From must be a non-empty string.")
    } else if (!(to is String)) {
      Fiber.abort("To must be a string.")
    }

    var result = ""

    var last = 0
    var index = 0

    var fromSize = from.byteCount_
    var size = byteCount_

    while (last < size && (index = indexOf(from, last)) != -1) {
      result = result + this[last...index] + to
      last = index + fromSize
    }

    if (last < size) result = result + this[last..-1]

    return result
  }

  trim() { trim_("\t\r\n ", true, true) }
  trim(chars) { trim_(chars, true, true) }
  trimEnd() { trim_("\t\r\n ", false, true) }
  trimEnd(chars) { trim_(chars, false, true) }
  trimStart() { trim_("\t\r\n ", true, false) }
  trimStart(chars) { trim_(chars, true, false) }

  trim_(chars, trimStart, trimEnd) {
    if (!(chars is String)) {
      Fiber.abort("Characters must be a string.")
    }

    var codePoints = chars.codePoints.toList

    var start
    if (trimStart) {
      while (start = iterate(start)) {
        if (!codePoints.contains(codePointAt_(start))) break
      }

      if (start == false) return ""
    } else {
      start = 0
    }

    var end
    if (trimEnd) {
      end = byteCount_ - 1
      while (end >= start) {
        var codePoint = codePointAt_(end)
        if (codePoint != -1 && !codePoints.contains(codePoint)) break
        end = end - 1
      }

      if (end < start) return ""
    } else {
      end = -1
    }

    return this[start..end]
  }

  *(count) {
    if (!(count is Num) || !count.isInteger || count < 0) {
      Fiber.abort("Count must be a non-negative integer.")
    }

    var result = ""
    for (i in 0...count) {
      result = result + this
    }
    return result
  }

  // Add lower and upper case from https://github.com/wren-lang/wren/issues/1134
  lower {
    var output = ""
    for (c in codePoints) {
        if ((c >= 65 && c <= 90) || (c >= 192 && c <= 214) || (c >= 216 && c <= 222)) {
            c = c + 32
        }
        output = output + String.fromCodePoint(c)
    }
    return output
  }
  upper {
    var output = ""
    for (c in codePoints) {
        if ((c >= 97 && c <= 122) || (c >= 224 && c <= 246) || (c >= 248 && c <= 254)) {
            c = c - 32
        }
        output = output + String.fromCodePoint(c)
    }
    return output
  }

  safe {
    var output = ""
    for (c in codePoints) {
        if (c == 38) {
            output = output + "&amp;"
        } else if (c == 60) {
            output = output + "&lt;"
        } else if (c == 62) {
            output = output + "&gt;"
        } else if (c == 34) {
            output = output + "&quot;"
        } else if (c == 39) {
            output = output + "&apos;"
        } else {
            output = output + String.fromCodePoint(c)
        }
    }
    return output
  }
  raw { HtmlNode.new(this) }
  toNum { Num.fromString(this) }
  toBool { toNum != 0 }
}

// A wrapper for a string of already-rendered HTML. HTML string literals and
// the output of `{{ }}` interpolation produce HtmlNodes so the escape
// machinery leaves them alone, while interpolated user data is escaped.
class HtmlNode is Sequence {
  construct new(string) {
    _string = string
  }

  toString { _string }
  raw { this }
  safe { this }
  +(other) { HtmlNode.new(_string + other.toString) }
  count { _string.count }
  iterate(iterator) { _string.iterate(iterator) }
  iteratorValue(iterator) { _string.iteratorValue(iterator) }
}

class StringByteSequence is Sequence {
  construct new(string) {
    _string = string
  }

  [index] { _string.byteAt_(index) }
  iterate(iterator) { _string.iterateByte_(iterator) }
  iteratorValue(iterator) { _string.byteAt_(iterator) }

  count { _string.byteCount_ }
}

class StringCodePointSequence is Sequence {
  construct new(string) {
    _string = string
  }

  [index] { _string.codePointAt_(index) }
  iterate(iterator) { _string.iterate(iterator) }
  iteratorValue(iterator) { _string.codePointAt_(iterator) }

  count { _string.count }
}

class List is Sequence {
  addAll(other) {
    for (element in other) {
      add(element)
    }
    return other
  }

  sort() { sort {|low, high| low < high } }

  sort(comparer) {
    if (!(comparer is Fn)) {
      Fiber.abort("Comparer must be a function.")
    }
    quicksort_(0, count - 1, comparer)
    return this
  }

  quicksort_(low, high, comparer) {
    if (low < high) {
      var p = partition_(low, high, comparer)
      quicksort_(low, p - 1, comparer)
      quicksort_(p + 1, high, comparer)
    }
  }

  partition_(low, high, comparer) {
    var p = this[high]
    var i = low - 1
    for (j in low..(high-1)) {
      if (comparer.call(this[j], p)) {
        i = i + 1
        var t = this[i]
        this[i] = this[j]
        this[j] = t
      }
    }
    var t = this[i+1]
    this[i+1] = this[high]
    this[high] = t
    return i+1
  }

  toString { "[%(join(", "))]" }

  +(other) {
    var result = this[0..-1]
    for (element in other) {
      result.add(element)
    }
    return result
  }

  *(count) {
    if (!(count is Num) || !count.isInteger || count < 0) {
      Fiber.abort("Count must be a non-negative integer.")
    }

    var result = []
    for (i in 0...count) {
      result.addAll(this)
    }
    return result
  }
  first { count > 0 ? this[0] : null }

  // Adds [node] to an HTML interpolation list. HtmlNodes are already-safe and
  // stored raw; other values are escaped. Sequences (except String and Map)
  // are flattened so `{{ list.map{...} }}` renders each item without escaping
  // the markup produced by nested HTML literals.
  addHtml_(node) {
    if (node is HtmlNode) return addCore_(node)
    if (node.type == Bool && !node) return this
    if (node is String || node is Map) return addCore_(HtmlNode.new(node.toString.safe))
    if (node is Sequence) {
      for (element in node) addHtml_(element)
      return this
    }
    return addCore_(HtmlNode.new(node.toString.safe))
  }

  // Joins the elements of an HTML interpolation. Every element is an HtmlNode
  // by the time this runs, so it concatenates their raw strings.
  joinHtml_() {
    var res = ""
    for (element in this) res = res + element.toString
    return HtmlNode.new(res)
  }
}

class Map is Sequence {
  keys { MapKeySequence.new(this) }
  values { MapValueSequence.new(this) }
  to(Class) { Class.new(this) }

  toString {
    var first = true
    var result = "{"

    for (key in keys) {
      if (!first) result = result + ", "
      first = false
      result = result + "%(key): %(this[key])"
    }

    return result + "}"
  }

  iteratorValue(iterator) {
    return MapEntry.new(
        keyIteratorValue_(iterator),
        valueIteratorValue_(iterator))
  }
}

class MapEntry {
  construct new(key, value) {
    _key = key
    _value = value
  }

  key { _key }
  value { _value }

  toString { "%(_key):%(_value)" }
  safe { toString.safe }
}

class MapKeySequence is Sequence {
  construct new(map) {
    _map = map
  }

  iterate(n) { _map.iterate(n) }
  iteratorValue(iterator) { _map.keyIteratorValue_(iterator) }
}

class MapValueSequence is Sequence {
  construct new(map) {
    _map = map
  }

  iterate(n) { _map.iterate(n) }
  iteratorValue(iterator) { _map.valueIteratorValue_(iterator) }
}

class Range is Sequence {}

class System {
  static print() {
  }

  static print(obj) {
    writeObject_(obj)
    return obj
  }

  static printAll(sequence) {
    for (object in sequence) writeObject_(object)
  }

  static write(obj) {
    writeObject_(obj)
    return obj
  }

  static log(obj) {
    writeObject_(obj)
    return obj
  }

  static writeAll(sequence) {
    for (object in sequence) writeObject_(object)
  }

  static writeObject_(obj) {
    var string = obj.toString
    if (string is String) {
      writeString_(string)
    } else {
      writeString_("[invalid toString]")
    }
  }
}

class ClassAttributes {
  self { _attributes }
  methods { _methods }
  construct new(attributes, methods) {
    _attributes = attributes
    _methods = methods
  }
  toString { "attributes:%(_attributes) methods:%(_methods)" }
}

class Date {
  static init(tz) { __tz = tz }
  static tz { __tz }

  static fromString(date) { Date.fromString(date, Date.tz) }
  construct fromString(date, tz) {
    _tz = tz
    var f = date.split(" ")
    var d = f[0].split("-")
    var t = (f.count > 1 ? f[1] : "00:00:00").split(":")
    _seconds = t[2].toNum
    _minutes = t[1].toNum
    _hours = t[0].toNum
    _day = d[2].toNum
    _month = d[1].toNum
    _year = d[0].toNum
  }
  construct new(year, month, day, hours, minutes, seconds, tz) {
    _tz = tz
    _year = toNum_(year)
    _month = toNum_(month)
    _day = toNum_(day)
    _hours = toNum_(hours)
    _minutes = toNum_(minutes)
    _seconds = toNum_(seconds)
  }
  static new() { Date.fromString(Date.current_(Date.tz), Date.tz) }
  static new(date) { Date.new(date, Date.tz) }
  static new(date, tz) { date is Date ? date : Date.fromString(date, tz) }
  static new(year, month, day, hours, minutes, seconds) { Date.new(year, month, day, hours, minutes, seconds, Date.tz) }
  static new(year, month, day) { Date.new(year, month, day, 0, 0, 0, Date.tz) }
  static new(year, month, day, tz) { Date.new(year, month, day, 0, 0, 0, tz) }
  static now { Date.new() }

  toNum_(n) { n is Num ? n : (!n ? 0 : "%(n)".toNum) }

  seconds { _seconds }
  minutes { _minutes }
  hours { _hours }
  day { _day }
  month { _month }
  year { _year }
  tz { _tz }

  yyyy { _year.toString }
  yy { _year.toString.substring(2, 4) }
  mo { _month > 9 ? _month.toString : "0%(month)" }
  dd { _day > 9 ? _day.toString : "0%(day)" }
  hh { _hours > 9 ? _hours.toString : "0%(hours)" }
  mi { _minutes > 9 ? _minutes.toString : "0%(minutes)" }
  ss { _seconds > 9 ? _seconds.toString : "0%(seconds)" }

  dayOfWeek { Date.format_("\%w", year, month, day, hours, minutes, seconds, tz).toNum }
  weekOfYear { Date.format_("\%V", year, month, day, hours, minutes, seconds, tz).toNum }
  dayOfYear { Date.format_("\%j", year, month, day, hours, minutes, seconds, tz).toNum }
  unix { Date.unix_(year, month, day, hours, minutes, seconds, tz) }
  format(format) { Date.format_(format.replace("#", "\%"), year, month, day, hours, minutes, seconds, tz) }
  iso { toString }
  toString { "%(year)-%(mo)-%(dd) %(hh):%(mi):%(ss)" }
  diff(otherDate) { unix - otherDate.unix }
  cmp_(o) { diff(o) }
  < (o) { cmp_(o) <  0 }
  > (o) { cmp_(o) >  0 }
  <=(o) { cmp_(o) <= 0 }
  >=(o) { cmp_(o) >= 0 }
  ==(o) { cmp_(o) == 0 }
  !=(o) { cmp_(o) != 0 }
}

class Query {
  construct new() {}

  // The native layer only binds null/bool/num/string parameters. Anything else
  // (HtmlNode, Date, ...) is stringified with toString so it binds as text
  // instead of being silently dropped, which would shift every later `?`
  // placeholder left and corrupt the row.
  static bindParams_(params) {
    if (params == null) return []
    var result = []
    for (param in params) {
      if (param == null || param is Bool || param is Num || param is String) {
        result.add(param)
      } else {
        result.add(param.toString)
      }
    }
    return result
  }

  static fromString(string, params) { Query.new().query_(string, params) }
  static fetchFromString(string, params) { Query.new().fetch_(string, params) }
  // Stringify non-primitive params, then delegate to the native backends.
  query_(string, params) { queryRaw_(string, Query.bindParams_(params)) }
  fetch_(string, params) { fetchRaw_(string, Query.bindParams_(params)) }
  // Query methods, return last inserted ID
  query { query_(this, []) }
  query() { query_(this, []) }
  query(param) { query_(this, param is List ? param : [param]) }
  query(p1, p2) { query_(this, [p1, p2]) }
  query(p1, p2, p3) { query_(this, [p1, p2, p3]) }
  // Fetch methods, return result as List
  fetch { fetch_(this, []) }
  fetch() { fetch_(this, []) }
  fetch(param) { fetch_(this, param is List ? param : [param]) }
  fetch(p1, p2) { fetch_(this, [p1, p2]) }
  fetch(p1, p2, p3) { fetch_(this, [p1, p2, p3]) }
  // First methods, return first result as Object
  first_(params) {
    // Only SELECT statements can take a trailing "LIMIT 1". Appending it to an
    // UPDATE/DELETE/INSERT is a syntax error in SQLite >= 3.46 (UPDATE/DELETE
    // LIMIT support was removed there), and to an "UPDATE ... RETURNING" it
    // lands after the RETURNING clause, which most versions reject.
    var sql = "%(this)"
    if (sql.trim().upper.startsWith("SELECT")) sql = sql + " LIMIT 1"
    var res = fetch_(sql, params)
    return res is List && res.count > 0 ? res[0] : null
  }
  first { first_([]) }
  first() { first_([]) }
  first(param) { first_(param is List ? param : [param]) }
  first(p1, p2) { first_([p1, p2]) }
  first(p1, p2, p3) { first_([p1, p2, p3]) }
  val { val([]) }
  val() { val([]) }
  val(param) {
    var res = first_(param is List ? param : [param])
    return res is Map ? res.values.join() : null
  }
  val(p1, p2) { first_([p1, p2]).values.join() }
  val(p1, p2, p3) { first_([p1, p2, p3]).values.join() }
  toNum { Num.fromString(val) }
  toNum(param) { Num.fromString(val(param)) }
  toNum(p1, p2) { Num.fromString(val(p1, p2)) }
  toNum(p1, p2, p3) { Num.fromString(val(p1, p2, p3)) }
  toBool { toNum != 0 }
  toBool(param) { toNum(param) != 0 }
  toBool(p1, p2) { toNum(p1, p2) != 0 }
  toBool(p1, p2, p3) { toNum(p1, p2, p3) != 0 }
  order(col,direction, allowwedCols) { order(col, direction, allowwedCols, -1) }
  order(col, direction, allowedCols, limit) {
    if (allowedCols != null && allowedCols is List && !allowedCols.contains(col)) col = allowedCols[0]
    var dir = direction is String ? direction.lower : "asc"
    if (dir != "asc" && dir != "desc") dir = "asc"
    if (limit > 0) {
      return Query.new("%(this) ORDER BY %(col) %(dir.upper) LIMIT %(limit)")
    }
    return Query.new("%(this) ORDER BY %(col) %(dir.upper)")
  }
  save(values) {
    if (this.toString.trim().upper.startsWith("BIALET_")) return false
    var keys = []
    var bind = []
    var params = []
    var v
    for (val in values) {
      v = val
      if (v is MapEntry) {
        v = val.value
        keys.add(val.key)
      }
      if (val is Date) {
        v = val.toString.replace("T", " ")
      }
      if (v is Query) {
        bind.add(v.toString)
      } else {
        bind.add("?")
        params.add(v)
      }
    }
    var k = keys.count > 0 ? "(%(keys.join(",")))" : ""
    return Query.fromString("REPLACE INTO `%(this)` %(k) VALUES (%(bind.join(',')))", params)
  }
}
