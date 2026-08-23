var target = Request.get("target")

// Transport failure: nothing listens on port 1, so this must fail fast with
// a connection error instead of hanging or crashing the request.
var down = Http.get("http://127.0.0.1:1/", {"connectTimeout": 500})
var downIsNull = down == null
var downHasError = Http.error != 0
var downHasMessage = Http.errorMessage != ""

// Non-2xx status from a real server (a separate instance, so this can't
// self-deadlock), just not a 2xx path.
var missing = Http.get(target + "/does-not-exist-xyz")
var missingIsNull = missing == null
var missingStatus = Http.status
var missingHasError = Http.error != 0

return "down:" + downIsNull.toString +
  "|downError:" + downHasError.toString +
  "|downHasMessage:" + downHasMessage.toString +
  "|missing:" + missingIsNull.toString +
  "|missingStatus:" + missingStatus.toString +
  "|missingError:" + missingHasError.toString
