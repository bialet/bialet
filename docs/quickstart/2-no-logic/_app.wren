class Template {
  // But wait, if we start a string, we can have multilines as well
  static layout(content) { '
<!DOCTYPE html>
  <head class="h-full dark:bg-gray-100">
    <meta charset="utf-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Poll</title>
    <link href="https://cdnjs.cloudflare.com/ajax/libs/flowbite/2.3.0/flowbite.min.css" rel="stylesheet" />
  </head>
  <body class="h-full dark">
    <script src="https://cdnjs.cloudflare.com/ajax/libs/flowbite/2.3.0/flowbite.min.js"></script>
    <div class="h-screen md:container md:mx-auto bg-white dark:bg-gray-900 px-4">
      %( header )
      <main>
        <div class="mx-auto max-w-7xl py-6 sm:px-6 lg:px-8">
          %( content )
        </div>
      </main>
      %( footer )
    </div>
  </body>
</html>
    ' }
  static header { '
      <header>
        <div class="py-8 px-4 mx-auto max-w-screen-xl text-center lg:py-16">
          <h1><a href="." class="mb-4 text-4xl font-extrabold tracking-tight leading-none text-gray-900 md:text-5xl lg:text-6xl dark:text-white">Poll</a></h1>
        </div>
      </header>
  ' }
  static footer { '
      <footer class="py-8 px-4 mx-auto max-w-screen-xl text-center lg:py-16 text-gray-500">
        <p>Example made with <a href="https://bialet.dev" class="text-blue-600 hover:underline dark:text-blue-500">Bialet</a></p>
      </footer>
  ' }

}

