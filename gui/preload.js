const { contextBridge, ipcRenderer, shell } = require('electron')

contextBridge.exposeInMainWorld('bialet', {
  start: (port, path) => ipcRenderer.invoke('start', port, path),
  stop: () => ipcRenderer.invoke('stop'),
  openExternal: (href) => ipcRenderer.invoke('openExternal', href),
  selectFolder: () => ipcRenderer.invoke('dialog:openDirectory'),
})
