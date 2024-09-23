import "bialet" for Response, File
import "random" for Random

// Generate an emoji SVG
var random = Random.new()
var emojis = ["🎉", "🎂", "❤️", "🍔", "🍕"]
var svg = <svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 100 100">
  <text y=".9em" font-size="90">{{ random.sample(emojis) }}</text>
</svg>
// Save the svg in a file
var file = File.create("emoji.svg", "image/svg+xml", svg)
// Send the file
Response.file(file.id)
