const { spawn } = require('node:child_process');
const readline = require('node:readline');
const kill = require('tree-kill');
const { stdout } = require('node:process');

// Run Bialet through Node
//
const WAITING_TIME = 2000
const SIGINT = 'SIGINT'

const processes = []
const stopAll = (exit = true) => {
  console.log('Ending all Bialet processes...')
  const length = processes.length
  let count = 0
  processes.forEach((bialet) => {
    kill(bialet.pid, SIGINT, () => {
      console.log('Kill bialet process', bialet.pid)
      count++
      if (count >= length && exit) {
        process.exit(0)
      }
    })
  })
  setTimeout(() => {
    if (exit) {
      process.exit(0)
    }
  }, WAITING_TIME)
}

const rl = readline.createInterface({
  input: process.stdin,
  output: process.stdout
})
rl.on('close', stopAll)

const start = (port, path, logCallback, statusChanged) => {
  if (!statusChanged) {
    statusChanged = () => {}
  }
  if (!logCallback) {
    logCallback = () => {}
  }
  console.log(`Starting Bialet on port ${port} and serving ${path}`)
  const bialet = spawn(process.resourcesPath + '/bialet', ['-p', port, path], {
  })
  statusChanged(bialet.pid > 0)
  bialet.stdout.on('data', (data) => {
    logCallback(data.toString())
    stdout.write(data)
  })
  bialet.stderr.on('data', (data) => {
    logCallback(data.toString())
    stdout.write(data)
  })
  bialet.on('close', (code) => {
    statusChanged(false)
  })
  bialet.on('error', (error) => {
    console.error(error)
    statusChanged(false)
  })
  processes.push(bialet)
  return bialet
}

const stop = (bialet) => {
  console.log('Stopping Bialet...')
  kill(bialet.pid, SIGINT)
}

exports.start = start
exports.stop = stop
exports.stopAll = stopAll
