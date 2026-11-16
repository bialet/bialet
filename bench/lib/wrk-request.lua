-- wrk request script for the Bialet benchmark harness.
--
-- The harness passes everything through the environment so one static script
-- works for GET, form POST and multi-path runs (the "mixed" workload and the
-- multi-site scenario). wrk invokes request() once per request.
--
--   BENCH_METHOD   GET (default), POST, ...
--   BENCH_PATHS    comma-separated absolute paths, e.g. "/,/api/items"
--   BENCH_BODY     request body, empty for none
--   BENCH_CTYPE    Content-Type header, optional
--
-- wrk closes every connection after each response by design, which matches
-- Bialet: the server has no keep-alive.

local method = os.getenv("BENCH_METHOD") or "GET"
local body = os.getenv("BENCH_BODY") or ""
local ctype = os.getenv("BENCH_CTYPE") or ""
local raw_paths = os.getenv("BENCH_PATHS") or "/"

local paths = {}
for path in string.gmatch(raw_paths, "[^,]+") do
  table.insert(paths, path)
end
if #paths == 0 then
  paths[1] = "/"
end

local headers = {}
if ctype ~= "" then
  headers["Content-Type"] = ctype
end
if #body > 0 then
  headers["Content-Length"] = tostring(#body)
end

local index = 0
request = function()
  -- Deterministic round-robin rather than math.random: every wrk thread then
  -- hits every path, which keeps the mix balanced even at low request counts.
  index = (index % #paths) + 1
  return wrk.format(method, paths[index], headers, body)
end
