import "bialet" for Response
import "_app" for Template

Response.out(Template.layout('

  <h2 class="mb-5 text-2xl font-medium text-gray-900 dark:text-white">Has web development become overly complex?</h2>

  <!-- Yes option -->
  <h3 class="mt-2 mb-5 text-lg font-medium text-gray-900 dark:text-white">Yes</h3>
  <div class="w-full bg-gray-200 h-8 mb-6 rounded-full dark:bg-gray-700">
    <div class="bg-blue-600 text-xl h-8 font-medium text-blue-100 text-center p-1 leading-none rounded-full" style="width: 45\%"> 45\%</div>
  </div>
  <p class="mb-5 text-md font-medium text-gray-700 dark:text-gray-400">Total Votes: 90</p>

  <!-- No option -->
  <h3 class="mt-2 mb-5 text-lg font-medium text-gray-900 dark:text-white">No</h3>
  <div class="w-full bg-gray-200 h-8 mb-6 rounded-full dark:bg-gray-700">
    <div class="bg-red-600 text-xl h-8 font-medium text-blue-100 text-center p-1 leading-none rounded-full" style="width: 55\%"> 55\%</div>
  </div>
  <p class="mb-5 text-md font-medium text-gray-700 dark:text-gray-400">Total Votes: 110</p>

'))
