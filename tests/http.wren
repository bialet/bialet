import "bialet" for Response, Http

var data = Http.get("https://microsoftedge.github.io/Demos/json-dummy-data/64KB-min.json")
Response.out(data[0]["name"])
