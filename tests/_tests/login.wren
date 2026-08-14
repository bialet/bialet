Test.get("/login-check")
  .status(401)

Test.get("/login-check")
  .setHeader("Authorization", "Basic YWRtaW46d3Jvbmc=")
  .status(401)

Test.get("/login-check")
  .setHeader("Authorization", "Basic YWRtaW46c2VjcmV0")
  .status(200)
  .equals("authenticated")

Test.get("/login-check")
  .setHeader("Authorization", "Basic bm90YmFzaWM=")
  .status(401)
