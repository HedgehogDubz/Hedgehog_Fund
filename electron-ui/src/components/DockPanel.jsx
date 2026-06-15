import React, { useState, useCallback } from 'react';
import { theme } from '../utils/theme';

let dragData = null;

export default function DockPanel({ panels: initialPanels, style, dockId }) {
  const [panels, setPanels] = useState(initialPanels);
  const [activePanel, setActivePanel] = useState(0);
  const [dropHighlight, setDropHighlight] = useState(false);
  const [hoverIdx, setHoverIdx] = useState(-1);

  const handleDragStart = useCallback((e, index) => {
    dragData = {
      sourceDockId: dockId,
      panelIndex: index,
      panel: panels[index],
      removeSelf: () => {
        setPanels((prev) => prev.filter((_, i) => i !== index));
        setActivePanel(0);
      },
    };
    e.dataTransfer.effectAllowed = 'move';
    e.dataTransfer.setData('text/plain', panels[index].name);
  }, [panels, dockId]);

  const handleDragOver = useCallback((e) => {
    e.preventDefault();
    e.dataTransfer.dropEffect = 'move';
    setDropHighlight(true);
  }, []);

  const handleDragLeave = useCallback(() => setDropHighlight(false), []);

  const handleDrop = useCallback((e) => {
    e.preventDefault();
    setDropHighlight(false);
    if (!dragData || dragData.sourceDockId === dockId) { dragData = null; return; }
    const panel = dragData.panel;
    dragData.removeSelf();
    setPanels((prev) => [...prev, panel]);
    dragData = null;
  }, [dockId]);

  return (
    <div style={{
      display: 'flex', flexDirection: 'column',
      background: theme.tabBarBg,
      border: `1px solid ${dropHighlight ? theme.accentColor : theme.borderDark}`,
      overflow: 'hidden',
      ...style,
    }}>
      {/* Dock tab strip — no bottom border so the active tab merges with
          the panel body. Inactive tabs sit on the darker chrome (tabBarBg). */}
      <div
        onDragOver={handleDragOver}
        onDragLeave={handleDragLeave}
        onDrop={handleDrop}
        style={{
          display: 'flex',
          gap: 0,
          padding: 0,
          background: theme.tabBarBg,
          minHeight: theme.dockTabHeight,
          alignItems: 'stretch',
          flexShrink: 0,
        }}
      >
        {panels.map((p, i) => {
          const isActive = activePanel === i;
          const isHover = hoverIdx === i && !isActive;
          return (
            <button
              key={p.name}
              draggable
              onDragStart={(e) => handleDragStart(e, i)}
              onClick={() => setActivePanel(i)}
              onMouseEnter={() => setHoverIdx(i)}
              onMouseLeave={() => setHoverIdx(-1)}
              style={{
                position: 'relative',
                padding: '0 14px',
                border: 'none',
                cursor: 'grab',
                fontSize: 11,
                fontWeight: isActive ? 600 : 500,
                letterSpacing: '0.02em',
                color: isActive ? theme.activeTabColor : (isHover ? theme.textColor : theme.tabTextColor),
                background: isActive ? theme.panelBg : (isHover ? 'rgba(255,255,255,0.04)' : 'transparent'),
                lineHeight: `${theme.dockTabHeight}px`,
                fontFamily: theme.fontUI,
                transition: 'color 0.1s, background 0.1s',
              }}
            >
              {/* amber accent bar above the active tab */}
              {isActive && (
                <span style={{
                  position: 'absolute',
                  left: 0, right: 0, top: 0,
                  height: 2,
                  background: theme.accentColor,
                }} />
              )}
              {p.name}
            </button>
          );
        })}
        {/* drop zone fills remaining space */}
        <div style={{ flex: 1 }} />
      </div>

      {/* Panel content */}
      <div style={{ flex: 1, overflow: 'hidden', background: theme.panelBg }}>
        {panels.map((p, i) => (
          <div
            key={p.name}
            style={{
              display: activePanel === i ? 'flex' : 'none',
              flexDirection: 'column',
              width: '100%', height: '100%', overflow: 'auto',
            }}
          >
            {p.component}
          </div>
        ))}
        {panels.length === 0 && (
          <div
            onDragOver={handleDragOver}
            onDragLeave={handleDragLeave}
            onDrop={handleDrop}
            style={{
              flex: 1, display: 'flex', alignItems: 'center', justifyContent: 'center',
              color: theme.textColorDim, fontSize: 12,
              fontStyle: 'italic',
            }}
          >
            Drop a panel here
          </div>
        )}
      </div>
    </div>
  );
}
