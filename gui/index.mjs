import { spawn } from 'node:child_process'
import readline from 'node:readline'
import kill from 'tree-kill'
import { stdout } from 'node:process'

// Run Bialet through Node

const port = 7001
const path = './docs/recipes'

const bialet = spawn('./build/bialet', ['-p', port, path])

bialet.stdout.on('data', (data) => {
  stdout.write(data)
})
bialet.stderr.on('data', (data) => {
  stdout.write(data)
})
bialet.on('close', (code) => {
  console.log(`Oops! Bialet exited with code ${code}`)
  process.exit(1)
})

const rl = readline.createInterface({
  input: process.stdin,
  output: process.stdout
})

rl.on('close', () => {
  console.log('Ending program...')
  kill(bialet.pid, 'SIGINT', () => {
    console.log('Bialet killed')
    process.exit(0)
  })
})
