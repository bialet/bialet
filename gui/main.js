import {app, BrowserWindow, shell, ipcMain} from 'electron'
import {start, stopAll} from './bialet.mjs'
import path from 'path'
import { URL } from 'url'

const __dirname = new URL('.', import.meta.url).pathname

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
