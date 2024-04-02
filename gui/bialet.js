const { spawn } = require('node:child_process')
const kill = require('tree-kill')
const path = require('path')

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

const start = (port, rootPath, logCallback, statusChanged) => {
  if (!statusChanged) {
    statusChanged = () => {}
  }
  if (!logCallback) {
    logCallback = () => {}
  }
  console.log(`Starting Bialet on port ${port} and serving ${rootPath}`)
  const binaryFileName = process.platform == 'win32' ? 'bialet.exe' : 'bialet'
  const bialet = spawn(path.join(process.resourcesPath, binaryFileName), ['-p', port, rootPath])
  console.log(bialet)
  statusChanged(bialet.pid > 0)
  if (!bialet.pid) {
    logCallback('Failed to start Bialet, please reinstall the Desktop application.\n')
    return null
  }
  bialet.stdout.on('data', (data) => {
    logCallback(data.toString())
  })
  bialet.stderr.on('data', (data) => {
    logCallback(data.toString())
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
