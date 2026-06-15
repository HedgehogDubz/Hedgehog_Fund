import React, { useState, useRef, useEffect } from 'react';
import { theme } from '../utils/theme';
import { usePanelValue, useSetPanelValue } from '../state/store';

// ── Shared input style ─────────────────────────────────────────────────────
// Slightly recessed (inset highlight + subtle inner shadow) so inputs
// feel sunken into the surface, like Unity's inspector.
const baseInput = {
  background: theme.inputBg,
  color: theme.textColor,
  border: `1px solid ${theme.borderDark}`,
  borderRadius: 3,
  padding: '5px 8px',
  fontSize: 12,
  fontFamily: theme.fontUI,
  outline: 'none',
  width: '100%',
  boxSizing: 'border-box',
  boxShadow: 'inset 0 1px 0 rgba(0,0,0,0.25)',
  transition: 'border-color 0.12s, box-shadow 0.12s',
};

export function Row({ children, style }) {
  return (
    <div style={{ display: 'flex', gap: 8, alignItems: 'center', padding: '4px 10px', ...style }}>
      {children}
    </div>
  );
}

export function Label({ text, style }) {
  return (
    <span style={{
      color: theme.textColorMuted,
      fontSize: 11,
      fontWeight: 500,
      letterSpacing: '0.02em',
      whiteSpace: 'nowrap',
      ...style,
    }}>
      {text}
    </span>
  );
}

export function SectionLabel({ text }) {
  return (
    <span style={{
      color: theme.textColor,
      fontSize: 11,
      fontWeight: 700,
      textTransform: 'uppercase',
      letterSpacing: '0.08em',
      padding: '6px 0 2px',
    }}>
      {text}
    </span>
  );
}

export function Separator() {
  return <div style={{ height: 1, background: theme.separator, margin: '6px 10px', opacity: 0.6 }} />;
}

// ── Button ────────────────────────────────────────────────────────────────
// Subtle bevel: light top edge + dark bottom edge → reads as raised.
// `primary` makes it an amber accent button (use sparingly).
export function Button({ text, onClick, style, primary, disabled }) {
  const [hover, setHover] = useState(false);
  const [press, setPress]  = useState(false);

  const bg = primary
    ? (press ? theme.accentColorDim : (hover ? '#d59f1a' : theme.accentColor))
    : (press ? theme.widgetBgActive : (hover ? theme.widgetBgHover : theme.widgetBg));

  const color = primary ? '#fcfcfa' : theme.textColor;

  return (
    <button
      onClick={disabled ? undefined : onClick}
      onMouseEnter={() => setHover(true)}
      onMouseLeave={() => { setHover(false); setPress(false); }}
      onMouseDown={() => setPress(true)}
      onMouseUp={() => setPress(false)}
      disabled={disabled}
      style={{
        background: bg,
        color,
        border: `1px solid ${theme.borderDark}`,
        borderRadius: 3,
        padding: '4px 12px',
        fontSize: 11,
        fontWeight: primary ? 600 : 500,
        letterSpacing: '0.02em',
        cursor: disabled ? 'default' : 'pointer',
        opacity: disabled ? 0.5 : 1,
        whiteSpace: 'nowrap',
        fontFamily: theme.fontUI,
        boxShadow: press
          ? 'inset 0 1px 2px rgba(0,0,0,0.4)'
          : `inset 0 1px 0 ${primary ? 'rgba(255,255,255,0.18)' : 'rgba(255,255,255,0.06)'}, 0 1px 0 rgba(0,0,0,0.4)`,
        transition: 'background 0.1s, box-shadow 0.08s',
        ...style,
      }}
    >
      {text}
    </button>
  );
}

// ── Dropdown ──────────────────────────────────────────────────────────────
// Wrapped <select> styled to feel like a sibling of Button (not Input), so
// it's visually distinct from text fields. The chevron points LEFT when
// closed and rotates 90° to point DOWN when the menu is open.
//
// Open/closed state is tracked manually because there's no DOM event for
// "select is open." We toggle on mousedown, close on change/blur/escape,
// AND listen for any document click outside the wrapper as a safety net so
// the chevron always returns to its closed pose.
export function Dropdown({ options, value, onChange, stateKey, defaultValue, style }) {
  const stateVal = usePanelValue(stateKey, defaultValue);
  const setValue = useSetPanelValue();
  const current = stateKey ? stateVal : value;
  const [open, setOpen] = useState(false);
  const [hover, setHover] = useState(false);
  const wrapperRef = useRef(null);

  const handleChange = (e) => {
    const v = e.target.value;
    if (stateKey) setValue(stateKey, v);
    if (onChange) onChange(v);
    setOpen(false);
  };

  // Safety net: any mousedown outside this wrapper closes the dropdown so
  // the chevron rotates back to its left-pointing pose.
  useEffect(() => {
    if (!open) return;
    const onDocDown = (e) => {
      if (wrapperRef.current && !wrapperRef.current.contains(e.target)) {
        setOpen(false);
      }
    };
    // Use capture so we see the click before it's consumed elsewhere.
    document.addEventListener('mousedown', onDocDown, true);
    window.addEventListener('blur', () => setOpen(false));
    return () => {
      document.removeEventListener('mousedown', onDocDown, true);
    };
  }, [open]);

  // Strip width/flex from style so the wrapper handles layout, and the
  // <select> always fills it.
  const { width, flex, ...selectStyle } = style || {};

  const bg = open ? theme.widgetBgActive : (hover ? theme.widgetBgHover : theme.widgetBg);

  return (
    <div
      ref={wrapperRef}
      style={{
        position: 'relative',
        display: 'inline-flex',
        alignItems: 'center',
        width: width ?? (flex ? '100%' : undefined),
        flex,
        verticalAlign: 'middle',
      }}
    >
      <select
        value={current || ''}
        onChange={handleChange}
        onMouseDown={() => setOpen((o) => !o)}
        onBlur={() => setOpen(false)}
        onMouseEnter={() => setHover(true)}
        onMouseLeave={() => setHover(false)}
        onKeyDown={(e) => {
          if (e.key === 'Enter' || e.key === ' ') setOpen((o) => !o);
          if (e.key === 'Escape') setOpen(false);
        }}
        style={{
          background: bg,
          color: theme.textColor,
          border: `1px solid ${theme.borderDark}`,
          borderRadius: 3,
          padding: '5px 26px 5px 10px',
          fontSize: 12,
          fontFamily: theme.fontUI,
          outline: 'none',
          cursor: 'pointer',
          width: '100%',
          boxSizing: 'border-box',
          // Button-like bevel (raised), not input-like (sunken).
          boxShadow: open
            ? 'inset 0 1px 2px rgba(0,0,0,0.4)'
            : 'inset 0 1px 0 rgba(255,255,255,0.06), 0 1px 0 rgba(0,0,0,0.4)',
          transition: 'background 0.1s, box-shadow 0.1s',
          ...selectStyle,
        }}
      >
        {options.map((o) => (
          <option key={o} value={o}>{o}</option>
        ))}
      </select>
      {/* Crisp SVG chevron — left when closed, down when open. */}
      <span
        aria-hidden
        style={{
          position: 'absolute',
          right: 9,
          top: '50%',
          width: 10,
          height: 10,
          marginTop: -5,
          pointerEvents: 'none',
          color: theme.accentColor,
          transform: open ? 'rotate(-90deg)' : 'rotate(0deg)',
          transformOrigin: '50% 50%',
          transition: 'transform 0.18s ease',
          display: 'flex',
          alignItems: 'center',
          justifyContent: 'center',
        }}
      >
        <svg width="10" height="10" viewBox="0 0 10 10">
          <path d="M 7.5 1.5 L 2.5 5 L 7.5 8.5 Z" fill="currentColor" />
        </svg>
      </span>
    </div>
  );
}

// ── Text Input ────────────────────────────────────────────────────────────
export function TextInput({ placeholder, value, onChange, stateKey, defaultValue, style }) {
  const stateVal = usePanelValue(stateKey, defaultValue || '');
  const setValue = useSetPanelValue();
  const current = stateKey ? stateVal : (value || '');

  const handleChange = (e) => {
    const v = e.target.value;
    if (stateKey) setValue(stateKey, v);
    if (onChange) onChange(v);
  };

  return (
    <input
      type="text"
      placeholder={placeholder}
      value={current}
      onChange={handleChange}
      style={{ ...baseInput, ...style }}
    />
  );
}

// ── Number Input ──────────────────────────────────────────────────────────
export function NumberInput({ placeholder, value, onChange, stateKey, defaultValue, min, max, integersOnly, style }) {
  const stateVal = usePanelValue(stateKey, defaultValue);
  const setValue = useSetPanelValue();
  const current = stateKey ? (stateVal ?? '') : (value ?? '');

  const handleChange = (e) => {
    let v = e.target.value;
    if (integersOnly) v = v.replace(/[^0-9-]/g, '');
    if (stateKey) setValue(stateKey, v === '' ? null : Number(v));
    if (onChange) onChange(v === '' ? null : Number(v));
  };

  return (
    <input
      type="text"
      placeholder={placeholder}
      value={current}
      onChange={handleChange}
      style={{ ...baseInput, ...style }}
    />
  );
}

// ── List Widget ───────────────────────────────────────────────────────────
export function ListWidget({ items, selected, onSelect, multiSelect, style }) {
  const [hoverItem, setHoverItem] = useState(null);
  const handleClick = (item) => {
    if (multiSelect) {
      const sel = selected || [];
      if (sel.includes(item)) {
        onSelect(sel.filter((s) => s !== item));
      } else {
        onSelect([...sel, item]);
      }
    } else {
      if (selected && selected.length === 1 && selected[0] === item) return;
      onSelect([item]);
    }
  };

  return (
    <div style={{
      background: theme.inputBg,
      border: `1px solid ${theme.borderDark}`,
      borderRadius: 3,
      flex: 1,
      overflow: 'auto',
      minHeight: 80,
      boxShadow: 'inset 0 1px 0 rgba(0,0,0,0.25)',
      ...style,
    }}>
      {items.map((item) => {
        const isSelected = (selected || []).includes(item);
        const isHover = hoverItem === item && !isSelected;
        return (
          <div
            key={item}
            onClick={() => handleClick(item)}
            onMouseEnter={() => setHoverItem(item)}
            onMouseLeave={() => setHoverItem(null)}
            style={{
              padding: '4px 10px',
              fontSize: 12,
              color: isSelected ? '#ffffff' : theme.textColor,
              cursor: 'pointer',
              background: isSelected
                ? theme.accentColor
                : (isHover ? 'rgba(255,255,255,0.04)' : 'transparent'),
              borderLeft: isSelected
                ? `2px solid ${theme.accentColorDim}`
                : '2px solid transparent',
              transition: 'background 0.08s',
            }}
          >
            {item}
          </div>
        );
      })}
    </div>
  );
}

// ── Panel Container ───────────────────────────────────────────────────────
export function PanelContainer({ children, style }) {
  return (
    <div style={{
      display: 'flex',
      flexDirection: 'column',
      height: '100%',
      background: theme.panelBg,
      color: theme.textColor,
      fontSize: 12,
      fontFamily: theme.fontUI,
      overflow: 'auto',
      ...style,
    }}>
      {children}
    </div>
  );
}
