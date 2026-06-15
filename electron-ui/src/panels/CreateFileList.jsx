import React, { useState, useEffect, useCallback, useRef } from 'react';
import { PanelContainer, Row, Button } from '../components/widgets';
import { useSetPanelValue } from '../state/store';
import { theme } from '../utils/theme';

// Electron's renderer disables window.prompt(), so we ship a small modal that
// resolves a promise with the entered string (or null on cancel).
function PromptModal({ title, defaultValue, onSubmit, onCancel }) {
  const [value, setValue] = useState(defaultValue || '');
  const inputRef = useRef(null);

  useEffect(() => {
    if (inputRef.current) {
      inputRef.current.focus();
      inputRef.current.select();
    }
  }, []);

  const handleKey = (e) => {
    if (e.key === 'Enter') { e.preventDefault(); onSubmit(value); }
    else if (e.key === 'Escape') { e.preventDefault(); onCancel(); }
  };

  return (
    <div
      onClick={onCancel}
      style={{
        position: 'fixed', inset: 0,
        background: 'rgba(0,0,0,0.45)',
        display: 'flex', alignItems: 'center', justifyContent: 'center',
        zIndex: 10000,
      }}
    >
      <div
        onClick={(e) => e.stopPropagation()}
        style={{
          background: theme.panelBg,
          border: `1px solid ${theme.borderDark}`,
          borderRadius: 6,
          padding: 16,
          minWidth: 320,
          boxShadow: '0 8px 32px rgba(0,0,0,0.6)',
        }}
      >
        <div style={{
          color: theme.textColor, fontSize: 12, marginBottom: 10,
          fontFamily: theme.fontUI, fontWeight: 500,
        }}>
          {title}
        </div>
        <input
          ref={inputRef}
          type="text"
          value={value}
          onChange={(e) => setValue(e.target.value)}
          onKeyDown={handleKey}
          style={{
            background: theme.inputBg,
            color: theme.textColor,
            border: `1px solid ${theme.borderDark}`,
            borderRadius: 3,
            padding: '6px 8px',
            fontSize: 12,
            fontFamily: theme.fontUI,
            outline: 'none',
            width: '100%',
            boxSizing: 'border-box',
            boxShadow: 'inset 0 1px 0 rgba(0,0,0,0.25)',
          }}
        />
        <div style={{ display: 'flex', justifyContent: 'flex-end', gap: 8, marginTop: 12 }}>
          <Button text="Cancel" onClick={onCancel} />
          <Button text="OK" onClick={() => onSubmit(value)} primary />
        </div>
      </div>
    </div>
  );
}

function ConfirmModal({ message, onConfirm, onCancel }) {
  return (
    <div
      onClick={onCancel}
      style={{
        position: 'fixed', inset: 0,
        background: 'rgba(0,0,0,0.45)',
        display: 'flex', alignItems: 'center', justifyContent: 'center',
        zIndex: 10000,
      }}
    >
      <div
        onClick={(e) => e.stopPropagation()}
        style={{
          background: theme.panelBg,
          border: `1px solid ${theme.borderDark}`,
          borderRadius: 6,
          padding: 16,
          minWidth: 320,
          boxShadow: '0 8px 32px rgba(0,0,0,0.6)',
        }}
      >
        <div style={{
          color: theme.textColor, fontSize: 12, marginBottom: 12,
          fontFamily: theme.fontUI,
        }}>
          {message}
        </div>
        <div style={{ display: 'flex', justifyContent: 'flex-end', gap: 8 }}>
          <Button text="Cancel" onClick={onCancel} />
          <Button text="Delete" onClick={onConfirm} primary />
        </div>
      </div>
    </div>
  );
}

function buildTree(entries, baseDir) {
  const root = { name: '', children: {}, files: [], fullPath: baseDir };

  for (const entry of entries) {
    // entry shape from fs:readDirRecursive is { name, path, isDir }; path is
    // the relative path from baseDir. We also accept a bare string for
    // robustness (a trailing "/" marks a directory).
    const relPath = typeof entry === 'string' ? entry : entry.path;
    if (!relPath) continue;
    const isDir = typeof entry === 'object' ? !!entry.isDir : relPath.endsWith('/');
    const parts = relPath.replace(/\/$/, '').split('/').filter(Boolean);
    if (parts.length === 0) continue;

    let node = root;
    for (let i = 0; i < parts.length; i++) {
      const part = parts[i];
      const partialPath = parts.slice(0, i + 1).join('/');
      const fullPath = baseDir + '/' + partialPath;

      if (i === parts.length - 1 && !isDir) {
        node.files.push({ name: part, fullPath });
      } else {
        if (!node.children[part]) {
          node.children[part] = {
            name: part,
            children: {},
            files: [],
            fullPath,
            relPath: partialPath,
          };
        }
        node = node.children[part];
      }
    }
  }

  return root;
}

function TreeItem({ name, fullPath, isFolder, depth, expanded, onToggle, selected, onSelect, onContextMenu, onDragStart, onDragOver, onDrop, children }) {
  const isSelected = selected === fullPath;
  const indent = depth * 16;

  const handleDragStart = (e) => {
    e.dataTransfer.setData('text/plain', fullPath);
    if (onDragStart) onDragStart(fullPath);
  };

  const handleDragOver = (e) => {
    if (isFolder) {
      e.preventDefault();
      e.stopPropagation();
      e.currentTarget.style.background = theme.accentColorSoft;
    }
  };

  const handleDragLeave = (e) => {
    e.currentTarget.style.background = 'transparent';
  };

  const handleDrop = (e) => {
    e.preventDefault();
    e.stopPropagation();
    e.currentTarget.style.background = 'transparent';
    const srcPath = e.dataTransfer.getData('text/plain');
    if (onDrop && isFolder) onDrop(srcPath, fullPath);
  };

  return (
    <>
      <div
        draggable
        onDragStart={handleDragStart}
        onDragOver={handleDragOver}
        onDragLeave={handleDragLeave}
        onDrop={handleDrop}
        onClick={() => {
          if (isFolder) {
            onToggle();
            // Track the folder for context (where new files go), but
            // don't push it to the IDE — it's not a file to open.
            onSelect(fullPath, /*isFolder*/ true);
          } else {
            onSelect(fullPath, /*isFolder*/ false);
          }
        }}
        onContextMenu={(e) => onContextMenu(e, fullPath, isFolder)}
        style={{
          paddingLeft: indent + 10,
          paddingRight: 8,
          paddingTop: 3,
          paddingBottom: 3,
          cursor: 'pointer',
          fontFamily: theme.fontUI,
          fontSize: 12,
          fontWeight: isFolder ? 600 : 400,
          color: isSelected ? '#ffffff' : theme.textColor,
          background: isSelected ? theme.accentColor : 'transparent',
          userSelect: 'none',
          whiteSpace: 'nowrap',
          overflow: 'hidden',
          textOverflow: 'ellipsis',
        }}
        onMouseEnter={(e) => { if (!isSelected) e.currentTarget.style.background = 'rgba(255,255,255,0.04)'; }}
        onMouseLeave={(e) => { if (!isSelected) e.currentTarget.style.background = 'transparent'; }}
      >
        <span style={{ color: isSelected ? '#ffffff' : theme.textColorMuted, marginRight: 4, fontSize: 9 }}>
          {isFolder ? (expanded ? '\u25be' : '\u25b8') : '\u00b7'}
        </span>
        {name}
      </div>
      {isFolder && expanded && children}
    </>
  );
}

function TreeView({ node, depth, expanded, onToggle, selected, onSelect, onContextMenu, onDrop }) {
  const folderNames = Object.keys(node.children).sort();
  const fileNames = [...node.files].sort((a, b) => a.name.localeCompare(b.name));

  return (
    <>
      {folderNames.map((name) => {
        const child = node.children[name];
        const isExpanded = expanded.has(child.relPath);
        return (
          <TreeItem
            key={child.fullPath}
            name={name}
            fullPath={child.fullPath}
            isFolder
            depth={depth}
            expanded={isExpanded}
            onToggle={() => onToggle(child.relPath)}
            selected={selected}
            onSelect={onSelect}
            onContextMenu={onContextMenu}
            onDrop={onDrop}
          >
            <TreeView
              node={child}
              depth={depth + 1}
              expanded={expanded}
              onToggle={onToggle}
              selected={selected}
              onSelect={onSelect}
              onContextMenu={onContextMenu}
              onDrop={onDrop}
            />
          </TreeItem>
        );
      })}
      {fileNames.map((file) => (
        <TreeItem
          key={file.fullPath}
          name={file.name}
          fullPath={file.fullPath}
          isFolder={false}
          depth={depth}
          selected={selected}
          onSelect={onSelect}
          onContextMenu={onContextMenu}
          onDrop={onDrop}
        />
      ))}
    </>
  );
}

export default function CreateFileList() {
  const setValue = useSetPanelValue();
  const [creationsDir, setCreationsDir] = useState('');
  const [tree, setTree] = useState(null);
  const [expandedFolders, setExpandedFolders] = useState(new Set());
  const [selected, setSelected] = useState(null);
  const [contextMenu, setContextMenu] = useState(null);
  // { title, defaultValue, resolve } when a prompt is open
  const [prompt, setPrompt] = useState(null);
  const [confirmState, setConfirmState] = useState(null);

  const askPrompt = (title, defaultValue = '') =>
    new Promise((resolve) => setPrompt({ title, defaultValue, resolve }));

  const askConfirm = (message) =>
    new Promise((resolve) => setConfirmState({ message, resolve }));

  const refreshTree = useCallback(async () => {
    if (!creationsDir) return;
    try {
      const entries = await window.api.readDirRecursive(creationsDir);
      setTree(buildTree(entries, creationsDir));
    } catch (e) {
      console.error('Failed to read directory:', e);
    }
  }, [creationsDir]);

  useEffect(() => {
    (async () => {
      try {
        const paths = await window.api.getPaths();
        setCreationsDir(paths.creationsDir || paths.CREATIONS_DIR || '');
      } catch (e) {
        console.error('Failed to get paths:', e);
      }
    })();
  }, []);

  useEffect(() => {
    if (!creationsDir) return;
    refreshTree();
    const interval = setInterval(refreshTree, 2000);
    return () => clearInterval(interval);
  }, [creationsDir, refreshTree]);

  const handleToggle = (relPath) => {
    setExpandedFolders((prev) => {
      const next = new Set(prev);
      if (next.has(relPath)) {
        next.delete(relPath);
      } else {
        next.add(relPath);
      }
      return next;
    });
  };

  const handleSelect = (fullPath, isFolder = false) => {
    setSelected(fullPath);
    // Only push files to the IDE — folders are context for the
    // create-here / new-folder buttons.
    if (!isFolder) {
      setValue('create_selected_file', [fullPath]);
    }
  };

  const getSelectedFolder = () => {
    if (!selected) return creationsDir;
    // Check if selected path is a directory by looking in the tree
    // Simple heuristic: if it has no extension, treat as folder
    const lastPart = selected.split('/').pop();
    if (lastPart.includes('.')) {
      // It's a file, use its parent directory
      return selected.substring(0, selected.lastIndexOf('/'));
    }
    return selected;
  };

  const handleCreateFile = async (ext) => {
    const name = await askPrompt(`Enter file name${ext ? '' : ' (with extension)'}:`);
    if (!name) return;
    const filename = ext ? (name.endsWith(ext) ? name : name + ext) : name;
    const targetDir = getSelectedFolder();
    const filepath = targetDir + '/' + filename;
    try {
      await window.api.writeFile(filepath, '');
      await refreshTree();
      handleSelect(filepath);
    } catch (e) {
      console.error('Failed to create file:', e);
    }
  };

  const handleCreateFolder = async () => {
    const name = await askPrompt('Enter folder name:');
    if (!name) return;
    const targetDir = getSelectedFolder();
    const folderPath = targetDir + '/' + name;
    try {
      await window.api.mkdir(folderPath);
      await refreshTree();
    } catch (e) {
      console.error('Failed to create folder:', e);
    }
  };

  const handleContextMenu = (e, fullPath, isFolder) => {
    e.preventDefault();
    setContextMenu({ x: e.clientX, y: e.clientY, fullPath, isFolder });
  };

  const closeContextMenu = () => setContextMenu(null);

  const handleRename = async () => {
    if (!contextMenu) return;
    const oldPath = contextMenu.fullPath;
    const oldName = oldPath.split('/').pop();
    closeContextMenu();
    const newName = await askPrompt('Enter new name:', oldName);
    if (!newName || newName === oldName) return;
    const parentDir = oldPath.substring(0, oldPath.lastIndexOf('/'));
    const newPath = parentDir + '/' + newName;
    try {
      await window.api.rename(oldPath, newPath);
      await refreshTree();
    } catch (e) {
      console.error('Failed to rename:', e);
    }
  };

  const handleDelete = async () => {
    if (!contextMenu) return;
    const path = contextMenu.fullPath;
    const name = path.split('/').pop();
    closeContextMenu();
    const ok = await askConfirm(`Delete "${name}"?`);
    if (!ok) return;
    try {
      await window.api.deleteFile(path);
      if (selected === path) {
        setSelected(null);
        setValue('create_selected_file', []);
      }
      await refreshTree();
    } catch (e) {
      console.error('Failed to delete:', e);
    }
  };

  const handleNewFileInContext = async () => {
    if (!contextMenu) return;
    const targetDir = contextMenu.isFolder
      ? contextMenu.fullPath
      : contextMenu.fullPath.substring(0, contextMenu.fullPath.lastIndexOf('/'));
    closeContextMenu();
    const name = await askPrompt('Enter file name (with extension):');
    if (!name) return;
    const filepath = targetDir + '/' + name;
    try {
      await window.api.writeFile(filepath, '');
      await refreshTree();
      handleSelect(filepath);
    } catch (e) {
      console.error('Failed to create file:', e);
    }
  };

  const handleDrop = async (srcPath, destDir) => {
    if (!srcPath || !destDir || srcPath === destDir) return;
    const filename = srcPath.split('/').pop();
    const destPath = destDir + '/' + filename;
    if (srcPath === destPath) return;
    try {
      await window.api.move(srcPath, destPath);
      await refreshTree();
    } catch (e) {
      console.error('Failed to move file:', e);
    }
  };

  // Close context menu on any click
  useEffect(() => {
    if (!contextMenu) return;
    const handler = () => closeContextMenu();
    window.addEventListener('click', handler);
    return () => window.removeEventListener('click', handler);
  }, [contextMenu]);

  return (
    <PanelContainer>
      <Row>
        <Button text="+ .py" onClick={() => handleCreateFile('.py')} />
        <Button text="+ .cpp" onClick={() => handleCreateFile('.cpp')} />
        <Button text="+ .hog" onClick={() => handleCreateFile('.hog')} />
      </Row>
      <Row>
        <Button text="+ File" onClick={() => handleCreateFile('')} />
        <Button text="+ Folder" onClick={handleCreateFolder} />
      </Row>
      <div
        style={{
          flex: 1,
          overflow: 'auto',
          background: theme.inputBg,
          border: `1px solid ${theme.borderDark}`,
          borderRadius: 3,
          margin: '4px 10px 10px 10px',
          boxShadow: 'inset 0 1px 0 rgba(0,0,0,0.25)',
          paddingTop: 4,
          paddingBottom: 4,
        }}
      >
        {tree && (Object.keys(tree.children).length > 0 || tree.files.length > 0) ? (
          <TreeView
            node={tree}
            depth={0}
            expanded={expandedFolders}
            onToggle={handleToggle}
            selected={selected}
            onSelect={handleSelect}
            onContextMenu={handleContextMenu}
            onDrop={handleDrop}
          />
        ) : (
          <div style={{
            padding: '14px 12px',
            color: theme.textColorDim,
            fontSize: 11,
            fontStyle: 'italic',
            lineHeight: 1.5,
          }}>
            {tree
              ? 'No files yet — use the + buttons above to create one.'
              : 'Loading…'}
          </div>
        )}
      </div>

      {prompt && (
        <PromptModal
          title={prompt.title}
          defaultValue={prompt.defaultValue}
          onSubmit={(v) => { const r = prompt.resolve; setPrompt(null); r(v); }}
          onCancel={() => { const r = prompt.resolve; setPrompt(null); r(null); }}
        />
      )}
      {confirmState && (
        <ConfirmModal
          message={confirmState.message}
          onConfirm={() => { const r = confirmState.resolve; setConfirmState(null); r(true); }}
          onCancel={() => { const r = confirmState.resolve; setConfirmState(null); r(false); }}
        />
      )}

      {/* Context menu */}
      {contextMenu && (
        <div
          style={{
            position: 'fixed',
            left: contextMenu.x,
            top: contextMenu.y,
            background: theme.panelBg,
            border: `1px solid ${theme.borderDark}`,
            borderRadius: 4,
            zIndex: 9999,
            boxShadow: '0 4px 16px rgba(0,0,0,0.6), 0 0 0 1px rgba(255,255,255,0.04) inset',
            minWidth: 140,
            padding: 3,
          }}
        >
          {[
            { label: 'Rename', action: handleRename },
            { label: 'Delete', action: handleDelete },
            { label: 'New File', action: handleNewFileInContext },
          ].map((item) => (
            <div
              key={item.label}
              onClick={(e) => {
                e.stopPropagation();
                item.action();
              }}
              style={{
                padding: '6px 14px',
                cursor: 'pointer',
                color: theme.textColor,
                fontSize: 12,
                fontFamily: theme.fontUI,
                borderRadius: 2,
              }}
              onMouseEnter={(e) => { e.currentTarget.style.background = theme.accentColor; e.currentTarget.style.color = '#fff'; }}
              onMouseLeave={(e) => { e.currentTarget.style.background = 'transparent'; e.currentTarget.style.color = theme.textColor; }}
            >
              {item.label}
            </div>
          ))}
        </div>
      )}
    </PanelContainer>
  );
}
