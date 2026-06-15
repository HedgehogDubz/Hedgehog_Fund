import React, { useState, useEffect, useRef, useCallback, useMemo } from 'react';
import Editor from '@monaco-editor/react';
import { PanelContainer, Row, Button, Label } from '../components/widgets';
import { usePanelValue, usePanelState } from '../state/store';
import { theme } from '../utils/theme';

// Map file extension to Monaco language id
function detectLanguage(filename) {
  if (!filename) return 'plaintext';
  const ext = filename.slice(filename.lastIndexOf('.')).toLowerCase();
  switch (ext) {
    case '.py': return 'python';
    case '.cpp': return 'cpp';
    case '.hog': return 'cpp'; // closest built-in
    default: return 'plaintext';
  }
}

function getExtension(filename) {
  if (!filename) return '';
  const dot = filename.lastIndexOf('.');
  return dot >= 0 ? filename.slice(dot).toLowerCase() : '';
}

// Define monokai-hog theme once Monaco is available
function defineMonokaiHogTheme(monaco) {
  monaco.editor.defineTheme('monokai-hog', {
    base: 'vs-dark',
    inherit: true,
    rules: [
      { token: 'keyword', foreground: 'ff6188' },
      { token: 'string', foreground: 'ffd866' },
      { token: 'string.escape', foreground: 'ab9df2' },
      { token: 'number', foreground: 'ab9df2' },
      { token: 'comment', foreground: '727072' },
      { token: 'type', foreground: '78dce8' },
      { token: 'type.identifier', foreground: '78dce8' },
      { token: 'identifier', foreground: 'fcfcfa' },
      { token: 'delimiter', foreground: 'fcfcfa' },
      { token: 'operator', foreground: 'ff6188' },
      { token: 'tag', foreground: 'ff6188' },
      { token: 'attribute.name', foreground: 'a9dc76' },
      { token: 'attribute.value', foreground: 'ffd866' },
      { token: 'variable', foreground: 'fcfcfa' },
      { token: 'predefined', foreground: '78dce8' },
    ],
    colors: {
      'editor.background': '#2d2a2e',
      'editor.foreground': '#fcfcfa',
      'editor.selectionBackground': '#403e41',
      'editor.lineHighlightBackground': '#34313538',
      'editor.lineHighlightBorder': '#00000000',
      'editorCursor.foreground': '#ffd866',
      'editorWhitespace.foreground': '#3a3739',
      'editorLineNumber.foreground': '#5b595c',
      'editorLineNumber.activeForeground': '#a59fa6',
      'editorIndentGuide.background': '#3a3739',
      'editorIndentGuide.activeBackground': '#4a474c',
      'editorGutter.background': '#2d2a2e',
    },
  });
}

export default function IDE() {
  const selectedFileRaw = usePanelValue('create_selected_file', []);
  const selectedFile = useMemo(() => {
    if (Array.isArray(selectedFileRaw) && selectedFileRaw.length > 0) return selectedFileRaw[0];
    if (typeof selectedFileRaw === 'string') return selectedFileRaw;
    return null;
  }, [selectedFileRaw]);

  const [filename, setFilename] = useState(null);
  const [content, setContent] = useState('');
  const [language, setLanguage] = useState('plaintext');
  const [dirty, setDirty] = useState(false);
  const [paths, setPaths] = useState(null);

  const editorRef = useRef(null);
  const monacoRef = useRef(null);
  const filenameRef = useRef(null);
  const dirtyRef = useRef(false);
  const contentRef = useRef('');

  // Keep refs in sync
  filenameRef.current = filename;
  dirtyRef.current = dirty;
  contentRef.current = content;

  // Fetch paths once on mount
  useEffect(() => {
    if (window.api?.getPaths) {
      window.api.getPaths().then(setPaths).catch(() => {});
    }
  }, []);

  // Helper that reads current console_text and appends
  const appendToConsole = useCallback((text) => {
    const { get: getVal, set: setVal } = usePanelState.getState();
    const current = getVal('console_text', '');
    setVal('console_text', current + text);
  }, []);

  // Save current file
  const save = useCallback(async () => {
    const fn = filenameRef.current;
    if (!fn) return;
    const text = contentRef.current;
    if (window.api?.writeFile) {
      try {
        await window.api.writeFile(fn, text);
        setDirty(false);
      } catch (err) {
        appendToConsole(`Save error: ${err.message || err}\n`);
      }
    }
  }, [appendToConsole]);

  // Build current file
  const build = useCallback(async () => {
    const fn = filenameRef.current;
    if (!fn) return;
    if (dirtyRef.current) await save();

    const ext = getExtension(fn);

    if (ext === '.py') {
      appendToConsole('Build Complete\n');
      return;
    }

    if (ext === '.cpp') {
      appendToConsole('Build Complete\n');
      return;
    }

    if (ext === '.hog') {
      try {
        if (window.api?.hogBuild) {
          const buildResult = await window.api.hogBuild();
          if (buildResult && buildResult.error) {
            appendToConsole(`\u25b6 hog build\n${buildResult.error}\n`);
            return;
          }
        }
        if (window.api?.hogRun) {
          const result = await window.api.hogRun('build', fn);
          const output = (result?.stdout || '') + (result?.stderr || '') || '(no output)\n';
          appendToConsole(`\u25b6 hog build ${fn}\n${output}\n`);
        }
      } catch (err) {
        appendToConsole(`\u25b6 hog build error\n${err.message || err}\n`);
      }
      return;
    }

    appendToConsole(`\u25b6 Cannot build ${ext} files\n`);
  }, [save, appendToConsole]);

  // Run current file
  const run = useCallback(async () => {
    const fn = filenameRef.current;
    if (!fn) return;
    if (dirtyRef.current) await save();

    const ext = getExtension(fn);

    try {
      if (ext === '.cpp') {
        if (window.api?.cppBuildAndRun) {
          const result = await window.api.cppBuildAndRun(fn);
          const output = (result?.stdout || '') + (result?.stderr || '') || '(no output)\n';
          appendToConsole(`\u25b6 g++ && ./${fn.replace(/\.cpp$/, '')}\n${output}\n`);
        }
        return;
      }

      if (ext === '.py') {
        if (window.api?.pythonRun) {
          const result = await window.api.pythonRun(fn);
          const output = (result?.stdout || '') + (result?.stderr || '') || '(no output)\n';
          appendToConsole(`\u25b6 python3 ${fn}\n${output}\n`);
        }
        return;
      }

      if (ext === '.hog') {
        if (window.api?.hogBuild) {
          const buildResult = await window.api.hogBuild();
          if (buildResult && buildResult.error) {
            appendToConsole(`\u25b6 hog build\n${buildResult.error}\n`);
            return;
          }
        }
        if (window.api?.hogRun) {
          const result = await window.api.hogRun('run', fn);
          const output = (result?.stdout || '') + (result?.stderr || '') || '(no output)\n';
          appendToConsole(`\u25b6 hog run ${fn}\n${output}\n`);
        }
        return;
      }

      appendToConsole(`\u25b6 Cannot run ${ext} files\n`);
    } catch (err) {
      appendToConsole(`\u25b6 Error: ${err.message || err}\n`);
    }
  }, [save, appendToConsole]);

  // Build & Run
  const buildAndRun = useCallback(async () => {
    const fn = filenameRef.current;
    if (!fn) return;
    if (dirtyRef.current) await save();

    const ext = getExtension(fn);

    if (ext === '.hog') {
      // Build then run
      try {
        if (window.api?.hogBuild) {
          const buildResult = await window.api.hogBuild();
          if (buildResult && buildResult.error) {
            appendToConsole(`\u25b6 hog build\n${buildResult.error}\n`);
            return;
          }
        }
        if (window.api?.hogRun) {
          const result = await window.api.hogRun('build-run', fn);
          const output = (result?.stdout || '') + (result?.stderr || '') || '(no output)\n';
          appendToConsole(`\u25b6 hog build-run ${fn}\n${output}\n`);
        }
      } catch (err) {
        appendToConsole(`\u25b6 Error: ${err.message || err}\n`);
      }
      return;
    }

    // For other types, just run (which includes build for cpp)
    await run();
  }, [save, run, appendToConsole]);

  // Handle file selection changes
  useEffect(() => {
    const loadFile = async () => {
      if (!selectedFile) {
        if (filenameRef.current) {
          if (dirtyRef.current) await save();
          setFilename(null);
          setContent('');
          setLanguage('plaintext');
          setDirty(false);
          if (editorRef.current) editorRef.current.setValue('');
        }
        return;
      }

      const fname = selectedFile;
      if (!fname) return;

      // Skip if same file already loaded
      if (fname === filenameRef.current) return;

      // Auto-save previous file if dirty
      if (dirtyRef.current && filenameRef.current) {
        await save();
      }

      try {
        let text = '';
        if (window.api?.readFile) {
          text = await window.api.readFile(fname);
        }
        const lang = detectLanguage(fname);

        setFilename(fname);
        setContent(text || '');
        setLanguage(lang);
        setDirty(false);

        if (editorRef.current) {
          editorRef.current.setValue(text || '');
        }
      } catch (err) {
        // Failed to load file
      }
    };

    loadFile();
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, [selectedFile]);

  // Editor mount handler
  const handleEditorDidMount = useCallback((editor, monaco) => {
    editorRef.current = editor;
    monacoRef.current = monaco;

    // Ctrl+S / Cmd+S to save
    editor.addCommand(monaco.KeyMod.CtrlCmd | monaco.KeyCode.KeyS, () => {
      save();
    });
  }, [save]);

  // Before mount: define the custom theme
  const handleBeforeMount = useCallback((monaco) => {
    defineMonokaiHogTheme(monaco);
  }, []);

  // Track edits for dirty state
  const handleEditorChange = useCallback((value) => {
    contentRef.current = value || '';
    setContent(value || '');
    if (filenameRef.current) {
      setDirty(true);
    }
  }, []);

  const displayName = filename
    ? (dirty ? `${filename} *` : filename)
    : 'No file selected';

  return (
    <PanelContainer style={{ overflow: 'hidden' }}>
      {/* Toolbar */}
      <Row>
        <Label text={displayName} style={{ flex: 1, minWidth: 0, overflow: 'hidden', textOverflow: 'ellipsis', color: dirty ? theme.warningColor : theme.textColorMuted }} />
        <Button text="Save" onClick={save} />
        <Button text="Build" onClick={build} />
        <Button text="Run" onClick={run} primary />
        <Button text="Build & Run" onClick={buildAndRun} />
      </Row>

      {/* Editor */}
      <div style={{ flex: 1, minHeight: 0 }}>
        <Editor
          theme="monokai-hog"
          language={language}
          value={content}
          onChange={handleEditorChange}
          beforeMount={handleBeforeMount}
          onMount={handleEditorDidMount}
          options={{
            readOnly: !filename,
            fontSize: 13,
            fontFamily: 'Menlo, monospace',
            tabSize: 4,
            minimap: { enabled: false },
            scrollBeyondLastLine: false,
            wordWrap: 'off',
            lineNumbers: 'on',
            renderLineHighlight: 'line',
            selectionHighlight: true,
            automaticLayout: true,
            padding: { top: 4, bottom: 4 },
          }}
        />
      </div>
    </PanelContainer>
  );
}
