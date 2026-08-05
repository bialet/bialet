Cron.every(1){ |date|
  `DELETE FROM counter`.query
  `DELETE FROM items`.query
   return "Empty tables"
}
