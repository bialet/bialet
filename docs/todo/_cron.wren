import "/_domain" for Task

Cron.every(5){ |date| "👋 Hello, from Cron!" }
Cron.at(2, 0){ |date| Task.clearAll() }
