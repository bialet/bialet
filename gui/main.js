const electron = require('electron');
const app = electron.app;
const BrowserWindow = electron.BrowserWindow;
const shell = electron.shell;
const ipcMain = electron.ipcMain;

const bialet = require('./bialet.js'); // Assuming you've converted your MJS to common JS as well.
const start = bialet.start;
const stopAll = bialet.stopAll;

const path = require('path');

const createWindow = () => {
  const win = new BrowserWindow({
    width: 800,
    height: 600,
    autoHideMenuBar: true,
    webPreferences: {
      preload: path.join(__dirname, './preload.js')
    }
  })
  win.loadFile('gui/index.html')
}

app.whenReady().then(() => {
  ipcMain.handle('openExternal', (_, href) => shell.openExternal(href))
  createWindow()
  app.on('activate', () => {
    if (BrowserWindow.getAllWindows().length === 0) {
      createWindow()
    }
  })
  const bialet = start(7000,  'docs/recipes')
  console.log(`Bialet started pid: ${bialet.pid}`)
})
app.on('window-all-closed', () => stopAll())
