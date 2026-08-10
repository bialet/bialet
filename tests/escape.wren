// Auto-escaping: `{{ }}` escapes plain strings but leaves HtmlNode markup raw.

var userText = "<script>alert('xss')</script>"
var strWithTags = "plain <b>bold</b> and <i>italic</i>"
var strWithAttr = "he said \"hi\" & <bye>"

// Map values that are HTML literals: rendered raw, never escaped.
var htmlMap = {
  "one": <b>first</b>,
  "two": <i>second</i>,
  "deep": <div class="card"><span>inner</span></div>
}

// Map values that are plain strings containing tags: escaped.
var strMap = {
  "a": "<b>alpha</b>",
  "b": "<i>beta</i>",
  "c": "safe & <unsafe>"
}

var blockOne = <section><h2>One</h2></section>
var blockTwo = <section><h2>Two</h2></section>
var blocks = [blockOne, blockTwo]

// Iterate maps through a fixed key list so the assertion order is stable
// regardless of the hash-bucket order the VM uses.
var htmlKeys = ["deep", "one", "two"]
var strKeys = ["a", "b", "c"]

return <main>
  <p class="esc">{{ userText }}</p>
  <p class="strtags">{{ strWithTags }}</p>
  <p class="strattr">{{ strWithAttr }}</p>
  <p class="htmlmap">{{ htmlMap["one"] }}</p>
  <p class="htmlmapdeep">{{ htmlMap["deep"] }}</p>
  <p class="strmap">{{ strMap["a"] }}</p>
  <p class="strmapc">{{ strMap["c"] }}</p>
  <ul class="htmlmaplist">{{ htmlKeys.map{|k| <li><b>{{ htmlMap[k] }}</b></li> } }}</ul>
  <ul class="strmaplist">{{ strKeys.map{|k| <li>{{ strMap[k] }}</li> } }}</ul>
  <div class="outer">
    <section>
      <article>
        <h2>{{ strMap["b"] }}</h2>
        <p>{{ htmlMap["two"] }}</p>
      </article>
    </section>
  </div>
  {{ blocks.map{|b| <article>{{ b }}</article> } }}
</main>
