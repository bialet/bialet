import "bialet" for Cron
import "/_domain" for Task

Cron.every(2){ |date| "Hello, from Cron!" }
Cron.at(2, 0){ |date| Task.clearAll() }
