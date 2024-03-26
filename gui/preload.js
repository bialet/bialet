const { contextBridge, ipcRenderer, shell } = require('electron')

contextBridge.exposeInMainWorld('bialet', {
  start: (port, path) => ipcRenderer.invoke('start', port, path),
  stop: () => ipcRenderer.invoke('stop'),
  openExternal: (href) => ipcRenderer.invoke('openExternal', href),
  selectFolder: () => ipcRenderer.invoke('dialog:openDirectory'),
  onUpdateLog: (callback) => ipcRenderer.on('log', (_event, value) => callback(value)),
  onUpdateStatus: (callback) => ipcRenderer.on('status', (_event, value) => callback(value)),
})
