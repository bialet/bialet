var data = Http.get("https://microsoftedge.github.io/Demos/json-dummy-data/64KB-min.json")
return data[0]["name"]
