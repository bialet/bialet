// A folder route file beats the folder's own index for /dupe, and still serves
// deeper segments.
return "dupe-route:" + (Request.route(0) || "root")
