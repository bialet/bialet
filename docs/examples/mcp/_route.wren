import "gh:bialet/extra/mcp" for Mcp

#!doc = "A simple greeting tool"
class Greet {
  construct new(params) {
    _name = params["name"]
  } 
  #!doc = "Name of the person to greet"
  #!required
  name(name) { _name = name }

  call() { "Hello, %(_name)!" }
}

#!doc = "Search for available flights"
class SearchFlights {
  construct new(params) {
    _origin = params["origin"]
    _destination = params["destination"]
    _date = params["date"]
  }

  #!doc = "Departure city"
  #!type = String
  #!required
  origin{}

  #!doc = "Arrival city"
  #!type = String
  #!required
  destination{}

  #!doc = "Travel date"
  #!type = String
  #!format = "date"
  #!required
  date{}

  call() { {"from": _origin, "to": _destination, "date": _date, "flights": ["Flight 101", "Flight 202", "Flight 303"]} }
}

var mcp = Mcp.new('bialet-mcp-server', '1.0.0')
mcp.addTool(Greet)
mcp.addTool(SearchFlights)
mcp.addPrompt("You are a helpful assistant that can greet people and search for flights.")
mcp.serve
