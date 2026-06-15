const { app, BrowserWindow, ipcMain, Menu } = require('electron');
const path = require('path');
const fs = require('fs');
const { execSync, execFile } = require('child_process');

const PROJECT_ROOT = path.resolve(__dirname, '..', '..');
const HOG_DIR = path.join(PROJECT_ROOT, 'Hog');
const CREATIONS_DIR = path.join(PROJECT_ROOT, 'User_Creations');
const DATA_DIR = path.join(PROJECT_ROOT, 'data');

const HOG_SOURCES = [
  'hog.cpp', 'compile.cpp', 'parse.cpp', 'typecheck.cpp', 'interpret.cpp', 'trade.cpp', 'native_loader.cpp'
].map(f => path.join(HOG_DIR, f));

function createWindow() {
  const win = new BrowserWindow({
    width: 1200,
    height: 800,
    title: 'HedgehogFund',
    webPreferences: {
      preload: path.join(__dirname, 'preload.js'),
      contextIsolation: true,
      nodeIntegration: false,
    },
  });

  if (process.env.NODE_ENV === 'development' || !app.isPackaged) {
    win.loadURL('http://localhost:5173');
  } else {
    win.loadFile(path.join(__dirname, '..', 'dist', 'index.html'));
  }
}

function buildMenu() {
  const isMac = process.platform === 'darwin';
  const template = [
    ...(isMac ? [{
      label: app.name,
      submenu: [
        { role: 'about' },
        { type: 'separator' },
        { role: 'services' },
        { type: 'separator' },
        { role: 'hide' },
        { role: 'hideOthers' },
        { role: 'unhide' },
        { type: 'separator' },
        { role: 'quit' },
      ],
    }] : []),
    {
      label: 'Edit',
      submenu: [
        { role: 'undo' },
        { role: 'redo' },
        { type: 'separator' },
        { role: 'cut' },
        { role: 'copy' },
        { role: 'paste' },
        { role: 'pasteAndMatchStyle' },
        { role: 'delete' },
        { role: 'selectAll' },
      ],
    },
    {
      label: 'View',
      submenu: [
        { role: 'reload' },
        { role: 'forceReload' },
        { role: 'toggleDevTools' },
        { type: 'separator' },
        { role: 'resetZoom' },
        { role: 'zoomIn' },
        { role: 'zoomOut' },
        { type: 'separator' },
        { role: 'togglefullscreen' },
      ],
    },
    {
      label: 'Window',
      submenu: [
        { role: 'minimize' },
        { role: 'zoom' },
        ...(isMac ? [
          { type: 'separator' },
          { role: 'front' },
        ] : [
          { role: 'close' },
        ]),
      ],
    },
  ];
  Menu.setApplicationMenu(Menu.buildFromTemplate(template));
}

app.whenReady().then(() => { buildMenu(); createWindow(); });
app.on('window-all-closed', () => { if (process.platform !== 'darwin') app.quit(); });
app.on('activate', () => { if (BrowserWindow.getAllWindows().length === 0) createWindow(); });

// ── File I/O ────────────────────────────────────────────────────────────────

ipcMain.handle('fs:readFile', async (_, filePath) => {
  return fs.readFileSync(filePath, 'utf-8');
});

ipcMain.handle('fs:writeFile', async (_, filePath, content) => {
  fs.writeFileSync(filePath, content, 'utf-8');
  return true;
});

ipcMain.handle('fs:exists', async (_, filePath) => {
  return fs.existsSync(filePath);
});

ipcMain.handle('fs:readDir', async (_, dirPath) => {
  if (!fs.existsSync(dirPath)) return [];
  return fs.readdirSync(dirPath);
});

ipcMain.handle('fs:readDirRecursive', async (_, dirPath) => {
  if (!fs.existsSync(dirPath)) return [];
  const result = [];
  function walk(dir, rel) {
    const entries = fs.readdirSync(dir, { withFileTypes: true });
    for (const entry of entries) {
      const relPath = rel ? `${rel}/${entry.name}` : entry.name;
      if (entry.isDirectory()) {
        result.push({ name: entry.name, path: relPath, isDir: true });
        walk(path.join(dir, entry.name), relPath);
      } else {
        result.push({ name: entry.name, path: relPath, isDir: false });
      }
    }
  }
  walk(dirPath, '');
  return result;
});

ipcMain.handle('fs:deleteFile', async (_, filePath) => {
  if (fs.existsSync(filePath)) {
    const stat = fs.statSync(filePath);
    if (stat.isDirectory()) {
      fs.rmSync(filePath, { recursive: true, force: true });
    } else {
      fs.unlinkSync(filePath);
    }
    return true;
  }
  return false;
});

ipcMain.handle('fs:rename', async (_, oldPath, newPath) => {
  if (fs.existsSync(oldPath)) {
    fs.renameSync(oldPath, newPath);
    return true;
  }
  return false;
});

ipcMain.handle('fs:mkdir', async (_, dirPath) => {
  fs.mkdirSync(dirPath, { recursive: true });
  return true;
});

ipcMain.handle('fs:move', async (_, src, dest) => {
  fs.renameSync(src, dest);
  return true;
});

ipcMain.handle('fs:stat', async (_, filePath) => {
  if (!fs.existsSync(filePath)) return null;
  const stat = fs.statSync(filePath);
  return { isDir: stat.isDirectory(), size: stat.size, mtime: stat.mtimeMs };
});

// ── Paths ───────────────────────────────────────────────────────────────────

ipcMain.handle('paths:get', async () => {
  return { projectRoot: PROJECT_ROOT, hogDir: HOG_DIR, creationsDir: CREATIONS_DIR, dataDir: DATA_DIR };
});

// ── Data files (parquet) ────────────────────────────────────────────────────

ipcMain.handle('data:listParquetFiles', async () => {
  if (!fs.existsSync(DATA_DIR)) return [];
  return fs.readdirSync(DATA_DIR)
    .filter(f => f.endsWith('.parquet'))
    .sort();
});

ipcMain.handle('data:deleteParquetFile', async (_, filename) => {
  const fp = path.join(DATA_DIR, filename);
  if (fs.existsSync(fp)) { fs.unlinkSync(fp); return true; }
  return false;
});

ipcMain.handle('data:renameParquetFile', async (_, oldName, newName) => {
  if (!newName.endsWith('.parquet')) newName += '.parquet';
  const oldPath = path.join(DATA_DIR, oldName);
  const newPath = path.join(DATA_DIR, newName);
  if (fs.existsSync(oldPath)) { fs.renameSync(oldPath, newPath); return true; }
  return false;
});

ipcMain.handle('data:readParquet', async (_, filename) => {
  const fp = path.join(DATA_DIR, filename);
  if (!fs.existsSync(fp)) return null;
  const venvPython = path.join(PROJECT_ROOT, '.venv', 'bin', 'python3');
  const pyBin = fs.existsSync(venvPython) ? venvPython : 'python3';
  const os = require('os');
  const tmpScript = path.join(os.tmpdir(), '_hf_read_parquet.py');
  try {
    fs.writeFileSync(tmpScript, `
import pandas as pd, json, sys
df = pd.read_parquet(sys.argv[1])
if df.index.name:
    df = df.reset_index()
for col in df.columns:
    if pd.api.types.is_datetime64_any_dtype(df[col]):
        df[col] = df[col].dt.strftime("%Y-%m-%dT%H:%M:%S")
cols = list(df.columns)
data = df.values.tolist()
print(json.dumps({"columns": cols, "data": data}))
`);
    const result = execSync(
      `"${pyBin}" "${tmpScript}" "${fp}"`,
      { timeout: 10000, cwd: PROJECT_ROOT, maxBuffer: 50 * 1024 * 1024 }
    );
    return JSON.parse(result.toString());
  } catch (e) {
    console.error('readParquet error:', e.message);
    return null;
  }
});

ipcMain.handle('data:fetchData', async (_, params) => {
  const { instrument, intervalLabel, offerSideLabel, start, end } = params;
  const os = require('os');
  const venvPy = path.join(PROJECT_ROOT, '.venv', 'bin', 'python3');
  const tmpScript = path.join(os.tmpdir(), '_hf_fetch.py');
  try {
    fs.writeFileSync(tmpScript, `
import sys, json
sys.path.insert(0, ${JSON.stringify(PROJECT_ROOT)})
from retrieve_data import fetch_data
from datetime import datetime
result = fetch_data(
    ${JSON.stringify(instrument)},
    ${JSON.stringify(intervalLabel)},
    ${JSON.stringify(offerSideLabel)},
    datetime.fromisoformat(${JSON.stringify(start)}),
    datetime.fromisoformat(${JSON.stringify(end)})
)
print(json.dumps({"filepath": result}))
`);
    const result = execSync(
      `"${venvPy}" "${tmpScript}"`,
      { timeout: 60000, cwd: PROJECT_ROOT }
    );
    return JSON.parse(result.toString());
  } catch (e) {
    return { error: e.stderr?.toString() || e.message };
  }
});

ipcMain.handle('data:getInstruments', async (_, category) => {
  const os = require('os');
  const venvPy = path.join(PROJECT_ROOT, '.venv', 'bin', 'python3');
  const tmpScript = path.join(os.tmpdir(), '_hf_instruments.py');
  try {
    fs.writeFileSync(tmpScript, `
import sys, json
sys.path.insert(0, ${JSON.stringify(PROJECT_ROOT)})
from retrieve_data import get_instruments_for_category
print(json.dumps(get_instruments_for_category(${JSON.stringify(category)})))
`);
    const result = execSync(
      `"${venvPy}" "${tmpScript}"`,
      { timeout: 10000, cwd: PROJECT_ROOT }
    );
    return JSON.parse(result.toString());
  } catch (e) {
    return [];
  }
});

ipcMain.handle('data:resolveTicker', async (_, ticker) => {
  const os = require('os');
  const venvPy = path.join(PROJECT_ROOT, '.venv', 'bin', 'python3');
  const tmpScript = path.join(os.tmpdir(), '_hf_ticker.py');
  try {
    fs.writeFileSync(tmpScript, `
import sys, json
sys.path.insert(0, ${JSON.stringify(PROJECT_ROOT)})
from retrieve_data import resolve_ticker
r = resolve_ticker(${JSON.stringify(ticker)})
print(json.dumps(r))
`);
    const result = execSync(
      `"${venvPy}" "${tmpScript}"`,
      { timeout: 10000, cwd: PROJECT_ROOT }
    );
    return JSON.parse(result.toString());
  } catch (e) {
    return null;
  }
});

// ── Subprocess execution ────────────────────────────────────────────────────

ipcMain.handle('exec:run', async (_, command, args, options) => {
  return new Promise((resolve) => {
    const proc = execFile(command, args, {
      timeout: 30000,
      cwd: options?.cwd || PROJECT_ROOT,
      ...options,
    }, (error, stdout, stderr) => {
      resolve({ stdout: stdout || '', stderr: stderr || '', code: error ? error.code || 1 : 0 });
    });
  });
});

// ── Hog compiler ────────────────────────────────────────────────────────────

ipcMain.handle('hog:build', async () => {
  try {
    execSync(
      `g++ -std=c++17 -o "${path.join(HOG_DIR, 'hog')}" ${HOG_SOURCES.map(s => `"${s}"`).join(' ')}`,
      { timeout: 30000, cwd: HOG_DIR }
    );
    return { success: true };
  } catch (e) {
    return { success: false, error: e.stderr?.toString() || e.message };
  }
});

ipcMain.handle('hog:run', async (_, command, filepath) => {
  const hogBin = path.join(HOG_DIR, 'hog');
  return new Promise((resolve) => {
    execFile(hogBin, [command, filepath], {
      timeout: 30000,
      cwd: path.dirname(filepath),
    }, (error, stdout, stderr) => {
      resolve({ stdout: stdout || '', stderr: stderr || '', code: error ? error.code || 1 : 0 });
    });
  });
});

ipcMain.handle('hog:getTrades', async () => {
  const hogBin = path.join(HOG_DIR, 'hog');
  if (!fs.existsSync(hogBin)) return [];
  return new Promise((resolve) => {
    execFile(hogBin, ['trades'], { timeout: 10000, cwd: PROJECT_ROOT }, (error, stdout) => {
      if (error) { resolve([]); return; }
      resolve(stdout.trim().split('\n').filter(Boolean));
    });
  });
});

// Run a trade block over a parquet file's OHLCV columns and return the buy/sell
// signals emitted by the script. Output lines have the form "SIGNAL <action> <index>".
ipcMain.handle('hog:runTrade', async (_, tradeName, parquetFilename) => {
  const hogBin = path.join(HOG_DIR, 'hog');
  if (!fs.existsSync(hogBin)) return { error: 'hog binary not built' };

  const parquetPath = path.join(DATA_DIR, parquetFilename);
  if (!fs.existsSync(parquetPath)) return { error: 'data file not found' };

  const os = require('os');
  const venvPython = path.join(PROJECT_ROOT, '.venv', 'bin', 'python3');
  const pyBin = fs.existsSync(venvPython) ? venvPython : 'python3';
  const csvPath = path.join(os.tmpdir(), `_hf_trade_${process.pid}_${Date.now()}.csv`);
  const dumpScript = path.join(os.tmpdir(), '_hf_dump_ohlcv.py');

  try {
    fs.writeFileSync(dumpScript, `
import pandas as pd, sys
df = pd.read_parquet(sys.argv[1])
if df.index.name:
    df = df.reset_index()
cols = {c.lower(): c for c in df.columns}
out_cols = {}
for k in ['open','high','low','close','volume']:
    if k in cols:
        out_cols[k] = df[cols[k]]
out = pd.DataFrame(out_cols)
out.to_csv(sys.argv[2], index=False)
`);
    execSync(`"${pyBin}" "${dumpScript}" "${parquetPath}" "${csvPath}"`, {
      timeout: 30000, cwd: PROJECT_ROOT, maxBuffer: 50 * 1024 * 1024,
    });
  } catch (e) {
    return { error: 'failed to extract OHLCV: ' + (e.stderr?.toString() || e.message) };
  }

  return new Promise((resolve) => {
    execFile(hogBin, ['run-trade', tradeName, csvPath], {
      timeout: 30000, cwd: PROJECT_ROOT, maxBuffer: 20 * 1024 * 1024,
    }, (error, stdout, stderr) => {
      try { fs.unlinkSync(csvPath); } catch {}
      const signals = [];
      const lines = {}; // name -> [{x, y}]
      for (const line of (stdout || '').split('\n')) {
        if (line.startsWith('SIGNAL ')) {
          const parts = line.trim().split(/\s+/);
          const action = parts[1];
          const idx = parts.length >= 3 ? parseFloat(parts[2]) : NaN;
          if (action && Number.isFinite(idx)) {
            signals.push({ action, index: Math.round(idx) });
          }
        } else if (line.startsWith('LINE ')) {
          const parts = line.trim().split(/\s+/);
          if (parts.length >= 4) {
            const name = parts[1];
            const x = parseFloat(parts[2]);
            const y = parseFloat(parts[3]);
            if (Number.isFinite(x) && Number.isFinite(y)) {
              if (!lines[name]) lines[name] = [];
              lines[name].push({ x, y });
            }
          }
        }
      }
      resolve({
        signals,
        lines,
        stdout: stdout || '',
        stderr: stderr || '',
        code: error ? error.code || 1 : 0,
      });
    });
  });
});

// ── C++ compilation & execution ─────────────────────────────────────────────

ipcMain.handle('cpp:buildAndRun', async (_, filepath) => {
  const outBin = filepath.replace(/\.cpp$/, '');
  try {
    execSync(`g++ -std=c++17 -o "${outBin}" "${filepath}"`, { timeout: 30000, cwd: path.dirname(filepath) });
  } catch (e) {
    return { stdout: '', stderr: e.stderr?.toString() || 'Compilation failed', code: 1 };
  }
  return new Promise((resolve) => {
    execFile(outBin, [], { timeout: 30000, cwd: path.dirname(filepath) }, (error, stdout, stderr) => {
      resolve({ stdout: stdout || '', stderr: stderr || '', code: error ? error.code || 1 : 0 });
    });
  });
});

// ── Python execution ────────────────────────────────────────────────────────

ipcMain.handle('python:run', async (_, filepath) => {
  return new Promise((resolve) => {
    execFile('python3', [filepath], { timeout: 30000, cwd: path.dirname(filepath) }, (error, stdout, stderr) => {
      resolve({ stdout: stdout || '', stderr: stderr || '', code: error ? error.code || 1 : 0 });
    });
  });
});
