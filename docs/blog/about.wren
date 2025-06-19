import "/_app" for Template

return Template.layout(
  "About Bialet",
  <div>
    {{ Markdown.html("[Back to home](/)") }}

    {{ Markdown.file("about.md") }}
  </div>
)
