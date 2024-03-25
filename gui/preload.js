const { contextBridge, ipcRenderer, shell } = require('electron')

contextBridge.exposeInMainWorld('bialet', {
  start: (port, path, logCallback, statusChanged) => ipcRenderer.send('start', port, path, logCallback, statusChanged),
  stop: (bialet) => ipcRenderer.send('stop', bialet),
  openExternal: (href) => ipcRenderer.invoke('openExternal', href),
})
