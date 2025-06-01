import "bialet" for Request, Response
import "_app" for Template, Poll

var poll = Poll.new()

if (Request.isPost) {
  var vote = Request.post("vote")
  poll.vote(vote)
  System.print("Voted for {{vote)")
  // Redirect to the results page and stop the script
  Response.redirect("/results")
  return
}

return Template.layout(
  <form action="/" method="post">
    <h2 class="mb-5 text-2xl font-medium text-gray-900 dark:text-white">Has web development become overly complex?</h2>
    <div class="mb-6">
      <ul class="grid w-full gap-6 md:grid-cols-2">
        {{ poll.options.map { |opt| <li>
          <input type="radio" id="{{ opt["answer"] }}" name="vote" value="{{ opt["id"] }}" class="hidden peer" required>
          <label for="{{ opt["answer"] }}" class="inline-flex items-center justify-between w-full p-5 text-gray-500 bg-white border border-gray-200 rounded-lg cursor-pointer dark:hover:text-gray-300 dark:border-gray-700 dark:peer-checked:text-blue-500 peer-checked:border-blue-600 peer-checked:text-blue-600 hover:text-gray-600 hover:bg-gray-100 dark:text-gray-400 dark:bg-gray-800 dark:hover:bg-gray-700">
            <div class="block">
              <div class="w-full text-lg font-semibold">{{ opt["answer"] }}</div>
              <div class="w-full">{{ opt["comment"] }}</div>
            </div>
            <svg class="w-5 h-5 ms-3 rtl:rotate-180" aria-hidden="true" xmlns="http://www.w3.org/2000/svg" fill="none" viewBox="0 0 14 10">
              <path stroke="currentColor" stroke-linecap="round" stroke-linejoin="round" stroke-width="2" d="M1 5h12m0 0L9 1m4 4L9 9">
            </svg>
          </label>
        </li> } }}
      </ul>
    </div>
    <div class="pt-8 flex justify-center">
      <button type="submit" class="text-white bg-blue-700 hover:bg-blue-800 focus:ring-4 focus:outline-none focus:ring-blue-300 font-medium rounded-lg text-2xl w-full sm:w-auto px-5 py-2.5 text-center dark:bg-blue-600 dark:hover:bg-blue-700 dark:focus:ring-blue-800">Vote</button>
    </div>
  </form>
)
