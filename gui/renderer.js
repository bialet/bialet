const host = 'localhost'
let rootPath = ''
let outputLog = ''

const url = (host, post) => `http://${host}:${post}`
const d = document

d.body.addEventListener('click', event => {
  if (event.target.tagName.toLowerCase() === 'a') {
    event.preventDefault()
    bialet.openExternal(event.target.href)
  }
})
d.querySelector('#select-folder').addEventListener('click', async (_) => {
  rootPath = await bialet.selectFolder()
  d.querySelector('#rootPath').innerText = rootPath
})
d.querySelector('#start').addEventListener('click', async (_) => {
  const port = d.querySelector('#port').value
  bialet.start(port, rootPath)
  d.querySelector('#open-bialet').href = url(host, port)
})
d.querySelector('#stop').addEventListener('click', async (_) => bialet.stop())
d.querySelector('#open').addEventListener('click', async (_) => bialet.openExternal(url(host, d.querySelector('#port').value)))

bialet.onUpdateLog(log => {
  outputLog += log
  const logElement = d.querySelector('#log')
  logElement.scrollTop = logElement.scrollHeight
  logElement.value += log
})
bialet.onUpdateStatus(status => {
  d.querySelector('body').className = status ? 'running' : 'stopped'
  d.querySelector('#start').disabled = status
  d.querySelector('#stop').disabled = !status
  d.querySelector('#select-folder').disabled = status
  d.querySelector('#port').disabled = status
  d.querySelector('#open').disabled = !status
})
