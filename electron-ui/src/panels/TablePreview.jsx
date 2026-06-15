import React, { useState, useEffect, useMemo } from 'react';
import { PanelContainer } from '../components/widgets';
import { usePanelValue } from '../state/store';
import { theme } from '../utils/theme';

export default function TablePreview() {
  const selectedArr = usePanelValue('preview_selected_file', []);
  const [tableData, setTableData] = useState(null);

  const filename = useMemo(() => {
    if (Array.isArray(selectedArr) && selectedArr.length > 0) return selectedArr[0];
    if (typeof selectedArr === 'string') return selectedArr;
    return null;
  }, [selectedArr]);

  useEffect(() => {
    if (!filename) {
      setTableData(null);
      return;
    }

    let cancelled = false;
    window.api.readParquet(filename).then((data) => {
      if (!cancelled) setTableData(data);
    }).catch(() => {
      if (!cancelled) setTableData(null);
    });

    return () => { cancelled = true; };
  }, [filename]);

  if (!tableData || !tableData.columns || !tableData.data) {
    return (
      <PanelContainer>
        <div style={{ padding: 16, color: theme.tabTextColor, fontSize: 13 }}>
          Select a file to preview
        </div>
      </PanelContainer>
    );
  }

  const { columns, data } = tableData;

  return (
    <PanelContainer style={{ overflow: 'hidden' }}>
      <div style={{ flex: 1, overflow: 'auto' }}>
        <table style={{
          borderCollapse: 'collapse',
          width: '100%',
          fontSize: 11,
          fontFamily: theme.fontMono,
        }}>
          <thead>
            <tr>
              {columns.map((col) => (
                <th key={col} style={{
                  position: 'sticky',
                  top: 0,
                  padding: '6px 12px',
                  textAlign: 'left',
                  fontWeight: 600,
                  fontSize: 10,
                  letterSpacing: '0.06em',
                  textTransform: 'uppercase',
                  color: theme.textColorMuted,
                  background: theme.tabBarBg,
                  borderBottom: `1px solid ${theme.borderDark}`,
                  whiteSpace: 'nowrap',
                }}>
                  {col}
                </th>
              ))}
            </tr>
          </thead>
          <tbody>
            {data.map((row, ri) => (
              <tr key={ri} style={{ background: ri % 2 === 0 ? 'rgba(255,255,255,0.015)' : 'transparent' }}>
                {row.map((cell, ci) => (
                  <td key={ci} style={{
                    padding: '4px 12px',
                    color: theme.textColor,
                    textAlign: typeof cell === 'number' ? 'right' : 'left',
                    whiteSpace: 'nowrap',
                  }}>
                    {cell != null ? String(cell) : ''}
                  </td>
                ))}
              </tr>
            ))}
          </tbody>
        </table>
      </div>
    </PanelContainer>
  );
}
