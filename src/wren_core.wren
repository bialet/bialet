class Bool {
  [key] { null }
  map(f) { List.new() }
  count { 0 }
}
class Fiber {}
class Fn {}
class Num {}

// Null is widely used in Bialet in the templates.
// This way allow calling methods on null like a List or a Map
// and don't crash the program.
class Null {
  [key] { null }
  map(f) { List.new() }
  count { 0 }
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
    var first = true
    var result = ""

    for (element in this) {
      if (!first) result = result + sep
      first = false
      result = result + element.toString
    }

    return result
  }


  joinInt_() {
    var res = ""
    for (element in this) {
      if (element.type != Bool || element) res = res + "" + element.toString
    }
    return res
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
}

class MapSequence is Sequence {
  construct new(sequence, fn) {
    _sequence = sequence
    _fn = fn
  }

  iterate(iterator) { _sequence.iterate(iterator) }
  iteratorValue(iterator) { _fn.call(_sequence.iteratorValue(iterator)) }
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
}

class Map is Sequence {
  keys { MapKeySequence.new(this) }
  values { MapValueSequence.new(this) }

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

class Query {
  construct new() {}
  static fromString(string, params) { Query.new().query_(string, params) }
  static fetchFromString(string, params) { Query.new().fetch_(string, params) }
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
    var res = fetch_("%(this) LIMIT 1", params)
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
  toNumber { Num.fromString(val) }
  toNumber(param) { Num.fromString(val(param)) }
  toNumber(p1, p2) { Num.fromString(val(p1, p2)) }
  toNumber(p1, p2, p3) { Num.fromString(val(p1, p2, p3)) }
}
