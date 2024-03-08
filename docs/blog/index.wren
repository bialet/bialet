import "bialet" for Response
import "_app" for Template, Posts

Response.out(Template.layout("Blog Sample", '
  <h1>Blog Sample</h1>
  %( Posts.home().map{ |post| '
    <p>
      <a href="/post/%( post["slug"] )">%( post["title"] )</a>
    </p>'} )
'))
