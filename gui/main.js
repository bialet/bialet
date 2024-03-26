const electron = require('electron')
const app = electron.app
const BrowserWindow = electron.BrowserWindow
const shell = electron.shell
const ipcMain = electron.ipcMain
const dialog = electron.dialog

const bialet = require('./bialet.js') // Assuming you've converted your MJS to common JS as well.
const start = bialet.start
const stopAll = bialet.stopAll

const path = require('path')

const createWindow = () => {
  const win = new BrowserWindow({
    width: 800,
    height: 600,
    autoHideMenuBar: true,
    webPreferences: {
      preload: path.join(__dirname, './preload.js'),
      enableRemoteModule: true,
    }
  })
  win.loadFile('gui/index.html')
  ipcMain.handle('dialog:openDirectory', async () => {
    const { canceled, filePaths } = await dialog.showOpenDialog(win, {
      properties: ['openDirectory']
    })
    return (canceled) ? null : filePaths[0]
  })
}

app.whenReady().then(() => {
  ipcMain.handle('openExternal', (_, href) => shell.openExternal(href))
  createWindow()
  app.on('activate', () => {
    if (BrowserWindow.getAllWindows().length === 0) {
      createWindow()
    }
  })
  ipcMain.handle('start', (_, port, path) => {
    const bialet = start(port,  path)
    console.log(`Bialet started pid: ${bialet.pid}`)
  })
  ipcMain.handle('stop', (_) => stopAll(false))
})
app.on('window-all-closed', () => stopAll(true))
