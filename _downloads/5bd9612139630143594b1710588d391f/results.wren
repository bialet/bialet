import "bialet" for Response
import "_app" for Template, Poll

var poll = Poll.new()
System.print(poll.options)

Response.out(Template.layout('

  <h2 class="mb-5 text-2xl font-medium text-gray-900 dark:text-white">Has web development become overly complex?</h2>
  %( poll.options.map{ |opt| '
    <h3 class="mt-2 mb-5 text-lg font-medium text-gray-900 dark:text-white">%(opt["answer"])</h3>
    <div class="w-full bg-gray-200 h-8 mb-6 rounded-full dark:bg-gray-700">
      <div class="bg-%(opt["color"])-600 bg-600 text-xl h-8 font-medium text-blue-100 text-center p-1 leading-none rounded-full" style="width: %( poll.percentage(opt) )\%"> %( poll.percentage(opt) )\%</div>
    </div>
    <p class="mb-5 text-md font-medium text-gray-700 dark:text-gray-400">Total Votes: %(opt["votes"])</p>
  ' })
'))
