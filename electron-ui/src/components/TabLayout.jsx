import React, { useState, useRef, useCallback } from 'react';
import { theme } from '../utils/theme';

// ── Connector handle thickness ──────────────────────────────────────────────
const HANDLE_SIZE = 6;
const MIN_DOCK_RATIO = 0.05;

// Three little dots in the middle = "grippable", a friendly cue that
// doesn't shout.
function DotGrip({ vertical }) {
  const dot = (
    <div style={{
      width: 2, height: 2, borderRadius: 1,
      background: theme.textColorDim,
    }} />
  );
  return (
    <div style={{
      display: 'flex',
      flexDirection: vertical ? 'row' : 'column',
      gap: 3,
      opacity: 0.8,
    }}>
      {dot}{dot}{dot}
    </div>
  );
}

// ── HConnector: vertical divider line (draggable left/right) ────────────────
function HConnector({ xRatio, onDrag, top, height }) {
  const dragging = useRef(false);
  const containerRef = useRef(null);
  const [hover, setHover] = useState(false);
  const [active, setActive] = useState(false);

  const onMouseDown = useCallback((e) => {
    e.preventDefault();
    dragging.current = true;
    setActive(true);
    const onMouseMove = (ev) => {
      if (!dragging.current) return;
      const parent = containerRef.current?.parentElement;
      if (!parent) return;
      const rect = parent.getBoundingClientRect();
      const newRatio = (ev.clientX - rect.left) / rect.width;
      onDrag(Math.max(MIN_DOCK_RATIO, Math.min(1 - MIN_DOCK_RATIO, newRatio)));
    };
    const onMouseUp = () => {
      dragging.current = false;
      setActive(false);
      document.removeEventListener('mousemove', onMouseMove);
      document.removeEventListener('mouseup', onMouseUp);
      document.body.style.cursor = '';
      document.body.style.userSelect = '';
    };
    document.addEventListener('mousemove', onMouseMove);
    document.addEventListener('mouseup', onMouseUp);
    document.body.style.cursor = 'col-resize';
    document.body.style.userSelect = 'none';
  }, [onDrag]);

  const showAccent = hover || active;

  return (
    <div
      ref={containerRef}
      onMouseDown={onMouseDown}
      onMouseEnter={() => setHover(true)}
      onMouseLeave={() => setHover(false)}
      style={{
        position: 'absolute',
        left: `calc(${xRatio * 100}% - ${HANDLE_SIZE / 2}px)`,
        top: top != null ? `${top * 100}%` : 0,
        width: HANDLE_SIZE,
        height: height != null ? `${height * 100}%` : '100%',
        cursor: 'col-resize',
        zIndex: 10,
        background: 'transparent',
        display: 'flex',
        alignItems: 'center',
        justifyContent: 'center',
      }}
    >
      <div style={{
        position: 'absolute',
        left: Math.floor(HANDLE_SIZE / 2),
        top: 0,
        width: showAccent ? 2 : 1,
        height: '100%',
        background: showAccent ? theme.accentColor : theme.borderDark,
        transition: 'background 0.12s, width 0.12s',
        marginLeft: showAccent ? -1 : 0,
      }} />
      {hover && <div style={{ position: 'relative', zIndex: 1 }}><DotGrip /></div>}
    </div>
  );
}

// ── VConnector: horizontal divider line (draggable up/down) ─────────────────
function VConnector({ yRatio, onDrag, left, width }) {
  const dragging = useRef(false);
  const containerRef = useRef(null);
  const [hover, setHover] = useState(false);
  const [active, setActive] = useState(false);

  const onMouseDown = useCallback((e) => {
    e.preventDefault();
    dragging.current = true;
    setActive(true);
    const onMouseMove = (ev) => {
      if (!dragging.current) return;
      const parent = containerRef.current?.parentElement;
      if (!parent) return;
      const rect = parent.getBoundingClientRect();
      const newRatio = (ev.clientY - rect.top) / rect.height;
      onDrag(Math.max(MIN_DOCK_RATIO, Math.min(1 - MIN_DOCK_RATIO, newRatio)));
    };
    const onMouseUp = () => {
      dragging.current = false;
      setActive(false);
      document.removeEventListener('mousemove', onMouseMove);
      document.removeEventListener('mouseup', onMouseUp);
      document.body.style.cursor = '';
      document.body.style.userSelect = '';
    };
    document.addEventListener('mousemove', onMouseMove);
    document.addEventListener('mouseup', onMouseUp);
    document.body.style.cursor = 'row-resize';
    document.body.style.userSelect = 'none';
  }, [onDrag]);

  const showAccent = hover || active;

  return (
    <div
      ref={containerRef}
      onMouseDown={onMouseDown}
      onMouseEnter={() => setHover(true)}
      onMouseLeave={() => setHover(false)}
      style={{
        position: 'absolute',
        top: `calc(${yRatio * 100}% - ${HANDLE_SIZE / 2}px)`,
        left: left != null ? `${left * 100}%` : 0,
        height: HANDLE_SIZE,
        width: width != null ? `${width * 100}%` : '100%',
        cursor: 'row-resize',
        zIndex: 10,
        background: 'transparent',
        display: 'flex',
        alignItems: 'center',
        justifyContent: 'center',
      }}
    >
      <div style={{
        position: 'absolute',
        top: Math.floor(HANDLE_SIZE / 2),
        left: 0,
        height: showAccent ? 2 : 1,
        width: '100%',
        background: showAccent ? theme.accentColor : theme.borderDark,
        transition: 'background 0.12s, height 0.12s',
        marginTop: showAccent ? -1 : 0,
      }} />
      {hover && <div style={{ position: 'relative', zIndex: 1 }}><DotGrip vertical /></div>}
    </div>
  );
}

// ── Dock: absolutely positioned panel container ─────────────────────────────
function Dock({ x, y, w, h, children }) {
  return (
    <div style={{
      position: 'absolute',
      left: `${x * 100}%`,
      top: `${y * 100}%`,
      width: `${w * 100}%`,
      height: `${h * 100}%`,
      overflow: 'hidden',
      display: 'flex',
      flexDirection: 'column',
      boxSizing: 'border-box',
      padding: 2,
    }}>
      {children}
    </div>
  );
}

export { HConnector, VConnector, Dock };
