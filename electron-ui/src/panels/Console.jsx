import React, { useState, useRef, useEffect } from 'react';
import { PanelContainer } from '../components/widgets';
import { usePanelValue, useSetPanelValue } from '../state/store';
import { theme } from '../utils/theme';

export default function Console() {
  const consoleText = usePanelValue('console_text', '');
  const setValue = useSetPanelValue();
  const [input, setInput] = useState('');
  const [creationsDir, setCreationsDir] = useState('');
  const outputRef = useRef(null);

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

  // Auto-scroll to bottom when new text appears
  useEffect(() => {
    if (outputRef.current) {
      outputRef.current.scrollTop = outputRef.current.scrollHeight;
    }
  }, [consoleText]);

  const appendText = (text) => {
    setValue('console_text', (consoleText || '') + text);
  };

  const getExtension = (filename) => {
    const idx = filename.lastIndexOf('.');
    return idx >= 0 ? filename.slice(idx) : '';
  };

  const handleCommand = async (cmd) => {
    const trimmed = cmd.trim();
    if (!trimmed) return;

    appendText(`\u25b6 ${trimmed}\n`);

    if (trimmed === 'clear') {
      setValue('console_text', '');
      return;
    }

    try {
      if (trimmed.startsWith('run ')) {
        const filename = trimmed.slice(4).trim();
        const filepath = creationsDir + '/' + filename;
        const ext = getExtension(filename);
        let result;

        if (ext === '.cpp') {
          result = await window.api.cppBuildAndRun(filepath);
        } else if (ext === '.py') {
          result = await window.api.pythonRun(filepath);
        } else if (ext === '.hog') {
          await window.api.hogBuild();
          result = await window.api.hogRun('run', filepath);
        } else {
          appendText(`Unknown file type: ${ext}\n`);
          return;
        }

        if (result) {
          if (result.stdout) appendText(result.stdout);
          if (result.stderr) appendText(result.stderr);
        }
      } else if (trimmed.startsWith('hog build ')) {
        const filename = trimmed.slice(10).trim();
        const filepath = creationsDir + '/' + filename;
        await window.api.hogBuild();
        const result = await window.api.hogRun('build', filepath);
        if (result) {
          if (result.stdout) appendText(result.stdout);
          if (result.stderr) appendText(result.stderr);
        }
      } else if (trimmed.startsWith('hog run ')) {
        const filename = trimmed.slice(8).trim();
        const filepath = creationsDir + '/' + filename;
        await window.api.hogBuild();
        const result = await window.api.hogRun('run', filepath);
        if (result) {
          if (result.stdout) appendText(result.stdout);
          if (result.stderr) appendText(result.stderr);
        }
      } else if (trimmed.startsWith('hog ')) {
        const filename = trimmed.slice(4).trim();
        const filepath = creationsDir + '/' + filename;
        await window.api.hogBuild();
        const result = await window.api.hogRun('run', filepath);
        if (result) {
          if (result.stdout) appendText(result.stdout);
          if (result.stderr) appendText(result.stderr);
        }
      } else {
        appendText(`Unknown command: ${trimmed}\n`);
      }
    } catch (e) {
      appendText(`Error: ${e.message || e}\n`);
    }
  };

  const handleKeyDown = (e) => {
    if (e.key === 'Enter') {
      e.preventDefault();
      handleCommand(input);
      setInput('');
    }
  };

  return (
    <PanelContainer style={{ overflow: 'hidden' }}>
      <div
        ref={outputRef}
        style={{
          flex: 1,
          overflow: 'auto',
          background: theme.inputBg,
          color: theme.textColor,
          fontFamily: theme.fontMono,
          fontSize: 12,
          lineHeight: 1.5,
          whiteSpace: 'pre-wrap',
          padding: '8px 10px',
          display: 'flex',
          flexDirection: 'column',
          justifyContent: 'flex-end',
        }}
      >
        <div>{consoleText}</div>
      </div>
      <div
        style={{
          display: 'flex',
          alignItems: 'center',
          padding: '5px 10px',
          gap: 6,
          background: theme.inputBg,
          borderTop: `1px solid ${theme.borderDark}`,
        }}
      >
        <span
          style={{
            color: theme.accentColor,
            fontFamily: theme.fontMono,
            fontSize: 12,
            fontWeight: 600,
            whiteSpace: 'nowrap',
          }}
        >
          %
        </span>
        <input
          type="text"
          value={input}
          onChange={(e) => setInput(e.target.value)}
          onKeyDown={handleKeyDown}
          placeholder="run filename, hog filename, clear…"
          style={{
            flex: 1,
            background: 'transparent',
            border: 'none',
            outline: 'none',
            color: theme.textColor,
            fontFamily: theme.fontMono,
            fontSize: 12,
          }}
        />
      </div>
    </PanelContainer>
  );
}
