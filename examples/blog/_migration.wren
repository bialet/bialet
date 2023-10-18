import "bialet" for Db

Db.migrate("Posts Table", "
  CREATE TABLE posts (
    id INTEGER PRIMARY KEY,
    title TEXT,
    slug TEXT,
    content TEXT,
    createdAt DATETIME DEFAULT CURRENT_TIMESTAMP
  )
")

Db.migrate("Posts Data", "

INSERT INTO posts VALUES(1,'Lorem ipsum','lorem-ipsum','<p>Lorem ipsum dolor sit amet, consectetur adipiscing elit. Vivamus lacinia odio vitae vestibulum.</p><p>Donec in efficitur leo. Suspendisse potenti. Fusce quis facilisis lorem.</p><p>Aliquam erat volutpat. Nunc vitae fringilla massa, eget laoreet nunc.</p>','2023-10-18 19:00:00');

INSERT INTO posts VALUES(2,'Journey to the Cloud','journey-to-the-cloud','<p>Cloud computing is transforming the way businesses operate. With scalability and cost-efficiency, many companies are making the switch.</p><p>However, migrating to the cloud requires careful planning and strategy.</p><p>Security, compliance, and downtime are just a few challenges that may arise during the process.</p>','2023-10-18 19:05:00');

INSERT INTO posts VALUES(3,'The Art of Coffee Brewing','art-of-coffee-brewing','<p>Coffee brewing is an art form that has captivated people for centuries.</p><p>Different methods like French Press, Aeropress, and Espresso offer unique flavors and experiences.</p><p>Whether you’re a casual drinker or a connoisseur, there’s always something new to learn.</p>','2023-10-18 19:10:00');

")
