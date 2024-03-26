let rootPath = ''

document.body.addEventListener('click', event => {
  if (event.target.tagName.toLowerCase() === 'a') {
    event.preventDefault()
    bialet.openExternal(event.target.href)
  }
})
document.getElementById('select-folder').addEventListener('click', async (_) => {
  rootPath = await bialet.selectFolder()
  document.getElementById('rootPath').innerText = rootPath
})
const start = () => {
  const port = document.getElementById('port').value
  console.log(port, rootPath)
  bialet.start(port, rootPath)
}
document.getElementById('start').addEventListener('click', async (_) => {
  start();
})
document.getElementById('stop').addEventListener('click', async (_) => {
  bialet.stop()
})
document.getElementById('restart').addEventListener('click', async (_) => {
  bialet.stop()
  start();
})
