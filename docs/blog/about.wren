import "/_app" for Template

return Template.layout(
  "About Bialet",
  <main>
    {{ Markdown.html("[Back to home](/)") }}
    {{ Markdown.file("_about-bialet.md") }}
  </main>
)