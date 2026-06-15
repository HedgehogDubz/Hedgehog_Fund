import React, { useState, useEffect } from 'react';
import { PanelContainer, Row, Button, ListWidget } from '../components/widgets';
import { usePanelValue, useSetPanelValue } from '../state/store';
import { theme } from '../utils/theme';

export default function AlgorithmTradeList() {
  const [trades, setTrades] = useState([]);
  const [status, setStatus] = useState('');
  const selected = usePanelValue('algorithm_trade_selected', []);
  const analyzeFile = usePanelValue('analyze_selected_file', []);
  const setValue = useSetPanelValue();

  const refreshTrades = async () => {
    try {
      const result = await window.api.hogGetTrades();
      setTrades(result || []);
    } catch {
      setTrades([]);
    }
  };

  useEffect(() => {
    refreshTrades();
    const interval = setInterval(refreshTrades, 3000);
    return () => clearInterval(interval);
  }, []);

  const runSelected = async (tradeName, parquetFile) => {
    if (!window.api?.hogRunTrade) {
      setStatus('Error: hogRunTrade IPC not registered (restart Electron)');
      return;
    }
    if (!tradeName) { setStatus('Pick a trade'); return; }
    if (!parquetFile) { setStatus('Pick a data file'); return; }
    setStatus(`Running ${tradeName}…`);
    try {
      const res = await window.api.hogRunTrade(tradeName, parquetFile);
      if (res?.error) {
        setStatus(`Error: ${res.error}`);
        setValue('analyze_signals', null);
        setValue('analyze_lines', null);
        return;
      }
      const sigs = res?.signals || [];
      const lns = res?.lines || {};
      setValue('analyze_signals', sigs);
      setValue('analyze_lines', lns);
      const lineCount = Object.keys(lns).length;
      const parts = [`${sigs.length} signal${sigs.length === 1 ? '' : 's'}`];
      if (lineCount) parts.push(`${lineCount} line${lineCount === 1 ? '' : 's'}`);
      setStatus(parts.join(', ') + (res.stderr ? ' (stderr present)' : ''));
    } catch (e) {
      setStatus(`Error: ${e.message || e}`);
      setValue('analyze_signals', null);
      setValue('analyze_lines', null);
    }
  };

  const onSelect = (sel) => {
    setValue('algorithm_trade_selected', sel);
    const tradeName = Array.isArray(sel) ? sel[0] : sel;
    const file = Array.isArray(analyzeFile) ? analyzeFile[0] : analyzeFile;
    runSelected(tradeName, file);
  };

  const onRunClick = () => {
    const tradeName = Array.isArray(selected) ? selected[0] : selected;
    const file = Array.isArray(analyzeFile) ? analyzeFile[0] : analyzeFile;
    runSelected(tradeName, file);
  };

  // Re-run when the analyze file changes
  useEffect(() => {
    const tradeName = Array.isArray(selected) ? selected[0] : selected;
    const file = Array.isArray(analyzeFile) ? analyzeFile[0] : analyzeFile;
    if (tradeName && file) runSelected(tradeName, file);
  }, [analyzeFile]);

  return (
    <PanelContainer>
      <Row>
        <Button text="Run" onClick={onRunClick} primary />
        <Button text="Rename" onClick={() => {}} />
        <Button text="Delete" onClick={() => {}} />
      </Row>
      {status && (
        <Row>
          <span style={{
            color: status.startsWith('Error') ? theme.errorColor : theme.textColor,
            fontSize: 11, padding: '0 8px',
          }}>{status}</span>
        </Row>
      )}
      <div style={{ flex: 1, display: 'flex', flexDirection: 'column', padding: '0 8px 8px' }}>
        <ListWidget
          items={trades}
          selected={selected}
          onSelect={onSelect}
        />
      </div>
    </PanelContainer>
  );
}
