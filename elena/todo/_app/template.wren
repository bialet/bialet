// Layout. Tailwind via CDN for dev (the docs say self-host for prod).
// Alpine.js for the client-side bits. This is the one part that feels
// like a real frontend stack again.

class Template {
  static layout(content) {
    return <!doctype html>
    <html lang="en">
      <head>
        <meta charset="utf-8" />
        <meta name="viewport" content="width=device-width, initial-scale=1.0" />
        <title>Dashboard — Tasks</title>
        <script src="https://cdn.tailwindcss.com"></script>
        <script defer src="https://cdn.jsdelivr.net/npm/alpinejs@3.x.x/dist/cdn.min.js"></script>
      </head>
      <body class="bg-slate-100 min-h-screen">
        <header class="bg-white shadow-sm">
          <div class="max-w-lg mx-auto py-4 px-4 flex items-center justify-between">
            <h1 class="font-bold text-lg text-slate-800">My Dashboard</h1>
            <span class="text-xs text-slate-400">bialet · no node_modules</span>
          </div>
        </header>
        <main class="max-w-lg mx-auto py-8 px-4">{{ content }}</main>
      </body>
    </html>
  }
}
