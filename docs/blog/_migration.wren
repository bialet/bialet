Db.migrate("Posts Table", `
  CREATE TABLE posts (
    id INTEGER PRIMARY KEY,
    title TEXT,
    slug TEXT,
    content TEXT,
    createdAt DATETIME DEFAULT CURRENT_TIMESTAMP
  )
`)

Db.migrate("Posts Data", Fn.new{
  Db.save("posts", {
  "title": "Lorem ipsum",
  "slug": "lorem-ipsum",
  "content": <div>
      <p>Lorem ipsum dolor sit amet, consectetur adipiscing elit. Vivamus lacinia odio vitae vestibulum.</p>
      <p>Donec in efficitur leo. Suspendisse potenti. Fusce quis facilisis lorem.</p>
      <p>Aliquam erat volutpat. Nunc vitae fringilla massa, eget laoreet nunc.</p>
    </div>,
  })
  Db.save("posts", {
    "title": "Journey to the Cloud",
    "slug": "journey-to-the-cloud",
    "content": <div>
        <p>Cloud computing is transforming the way businesses operate. With scalability and cost-efficiency, many companies are making the switch.</p>
        <p>However, migrating to the cloud requires careful planning and strategy.</p>
        <p>Security, compliance, and downtime are just a few challenges that may arise during the process.</p>
      </div>,
  })
  Db.save("posts", {
    "title": "The Art of Coffee Brewing",
    "slug": "art-of-coffee-brewing",
    "content": <div>
        <p>Coffee brewing is an art form that has captivated people for centuries.</p>
        <p>Different methods like French Press, Aeropress, and Espresso offer unique flavors and experiences.</p>
        <p>Whether you’re a casual drinker or a connoisseur, there’s always something new to learn.</p>
      </div>,
  })
})
