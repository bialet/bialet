# Project Structure and Routing

**The best way to think a project with Bialet is do it first like static old school HTML,
then replace the logic and template duplication with Wren code.**

The scripts will be load as if they were HTML files, so the file `contact-us.wren` can be open with the URL [127.0.0.1:7001/contact-us.wren](http://127.0.0.1:7001/contact-us.wren) or even without the wren suffix [127.0.0.1:7001/contact-us](http://127.0.0.1:7001/contact-us).

This will work with each folder, for example the URL [127.0.0.1:7001/landing/newsletter/cool-campaign](http://127.0.0.1:7001/landing/newsletter/cool-campaign) will run the script `landing/newsletter/cool-campaign.wren`. Like if there was an HMTL file.

That mean that each file is public and will be executed **except** when it start with a `_` or `.`. The `_app.wren` file won't be load with the URL [127.0.0.1:7001/_app](http://127.0.0.1:7001/_app) or with any other URL. Any file or any file under a folder that start with `_` or `.` won't be open.

For dynamic routing, add a `_route.wren` file in the folder and use the `Request.route(N)` function to get the value. **N** is the position of the route parameter. For the file `api/_route.wren` when called from [127.0.0.1:7001/api/users/1?fields=name,email&sortType=ASC](http://127.0.0.1:7001/api/users/1?fields=name,email&sortType=ASC) those will be the values:

```wren

Request.route(0) // users
Request.route(1) // 1
Request.param("fields") // name,email
Request.param("sortType") // ASC
```

> **Note:** There is no need to add a single `_route.wren` file to handle all the routing.


