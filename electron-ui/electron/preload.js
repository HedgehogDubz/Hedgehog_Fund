const { contextBridge, ipcRenderer } = require('electron');

contextBridge.exposeInMainWorld('api', {
  // File system
  readFile: (path) => ipcRenderer.invoke('fs:readFile', path),
  writeFile: (path, content) => ipcRenderer.invoke('fs:writeFile', path, content),
  exists: (path) => ipcRenderer.invoke('fs:exists', path),
  readDir: (path) => ipcRenderer.invoke('fs:readDir', path),
  readDirRecursive: (path) => ipcRenderer.invoke('fs:readDirRecursive', path),
  deleteFile: (path) => ipcRenderer.invoke('fs:deleteFile', path),
  rename: (oldPath, newPath) => ipcRenderer.invoke('fs:rename', oldPath, newPath),
  mkdir: (path) => ipcRenderer.invoke('fs:mkdir', path),
  move: (src, dest) => ipcRenderer.invoke('fs:move', src, dest),
  stat: (path) => ipcRenderer.invoke('fs:stat', path),

  // Paths
  getPaths: () => ipcRenderer.invoke('paths:get'),

  // Data
  listParquetFiles: () => ipcRenderer.invoke('data:listParquetFiles'),
  deleteParquetFile: (name) => ipcRenderer.invoke('data:deleteParquetFile', name),
  renameParquetFile: (oldName, newName) => ipcRenderer.invoke('data:renameParquetFile', oldName, newName),
  readParquet: (name) => ipcRenderer.invoke('data:readParquet', name),
  fetchData: (params) => ipcRenderer.invoke('data:fetchData', params),
  getInstruments: (category) => ipcRenderer.invoke('data:getInstruments', category),
  resolveTicker: (ticker) => ipcRenderer.invoke('data:resolveTicker', ticker),

  // Execution
  exec: (cmd, args, opts) => ipcRenderer.invoke('exec:run', cmd, args, opts),
  hogBuild: () => ipcRenderer.invoke('hog:build'),
  hogRun: (command, filepath) => ipcRenderer.invoke('hog:run', command, filepath),
  hogGetTrades: () => ipcRenderer.invoke('hog:getTrades'),
  hogRunTrade: (tradeName, parquetFilename) => ipcRenderer.invoke('hog:runTrade', tradeName, parquetFilename),
  cppBuildAndRun: (filepath) => ipcRenderer.invoke('cpp:buildAndRun', filepath),
  pythonRun: (filepath) => ipcRenderer.invoke('python:run', filepath),
});
