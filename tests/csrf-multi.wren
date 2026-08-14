var s = Session.new()

if (Request.isPost) return s.csrfOk ? "OK" : "FAIL"

// A page with several forms must give every form a token that passes csrfOk.
return <main>
  <form method="post">{{ s.csrf }}<button>A</button></form>
  <form method="post">{{ s.csrf }}<button>B</button></form>
  <form method="post">{{ s.csrf }}<button>C</button></form>
</main>
