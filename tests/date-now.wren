var now = Date.now
var checks = []
checks.add(now.year >= 2000)
checks.add(now.month >= 1 && now.month <= 12)
checks.add(now.day >= 1 && now.day <= 31)
checks.add(now.hours >= 0 && now.hours <= 23)
checks.add(now.minutes >= 0 && now.minutes <= 59)
checks.add(now.seconds >= 0 && now.seconds <= 59)
checks.add(now.unix > 0)

var allOk = true
for (c in checks) {
  if (!c) allOk = false
}
return allOk.toString
