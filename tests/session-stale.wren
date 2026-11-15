// An existing Session instance must follow the live cookie on reads too.
// rotate() retires the identifier it was built with and destroy() clears the
// cookie, but get()/id used to keep serving the constructor snapshot until the
// next set(). After rotation the old values are gone under the new id; after
// destroy the instance names no session and reads are null.
var session = Session.new()
session.set("k", "v")
var before = session.get("k")
Session.rotate()
var rotated = session.get("k")
var idMatches = session.id == Session.id
session.set("k2", "v2")
Session.destroy()
var destroyed = session.get("k2")
var noId = session.id == null
return "before:%(before)|rotated:%(rotated ? rotated : "null")|idMatches:%(idMatches)|destroyed:%(destroyed ? destroyed : "null")|noId:%(noId)"
