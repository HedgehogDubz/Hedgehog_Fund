import React, { useState } from 'react';
import { theme } from '../utils/theme';

// Tiny hedgehog mark — a single quill-fan glyph. Used once in the top bar
// as a subtle brand cue. Hand-tuned, not a stock icon.
function HedgehogMark({ size = 14 }) {
  return (
    <svg width={size} height={size} viewBox="0 0 24 24" style={{ display: 'block' }} aria-hidden>
      {/* body */}
      <path
        d="M5 16 C5 11, 9 8, 13 8 C17 8, 20 10.5, 20 14 C20 17, 17.5 19, 14 19 L7 19 C5.8 19, 5 18.2, 5 16 Z"
        fill={theme.accentColor}
        opacity="0.85"
      />
      {/* nose */}
      <circle cx="20.4" cy="13.6" r="0.9" fill={theme.textColor} />
      {/* eye */}
      <circle cx="17.5" cy="12.4" r="0.7" fill="#1a1a1a" />
      {/* quills */}
      <g stroke={theme.textColor} strokeWidth="0.9" strokeLinecap="round" opacity="0.6">
        <line x1="6.5" y1="14" x2="5" y2="11" />
        <line x1="8.5" y1="11.5" x2="7.5" y2="8" />
        <line x1="11" y1="10" x2="11" y2="6.5" />
        <line x1="13.5" y1="10" x2="14.5" y2="6.5" />
      </g>
    </svg>
  );
}

export default function TabDock({ tabs }) {
  const [activeTab, setActiveTab] = useState(0);

  return (
    <div style={{
      display: 'flex',
      flexDirection: 'column',
      height: '100vh',
      background: theme.appBg,
    }}>
      {/* ── Top chrome strip ─────────────────────────────────────────── */}
      {/* No bottom border — the color contrast between the chrome and the
          panel content beneath does the dividing, and the active tab uses
          the panel color so it merges seamlessly into the body below. */}
      <div style={{
        display: 'flex',
        alignItems: 'stretch',
        background: theme.topBarBg,
        height: theme.topTabHeight,
        flexShrink: 0,
        userSelect: 'none',
      }}>
        {/* Brand mark */}
        <div style={{
          display: 'flex',
          alignItems: 'center',
          gap: 8,
          padding: '0 14px 0 12px',
          borderRight: `1px solid ${theme.borderDark}`,
          color: theme.textColorMuted,
          fontSize: 11,
          fontWeight: 600,
          letterSpacing: '0.08em',
          textTransform: 'uppercase',
        }}>
          <HedgehogMark size={15} />
          <span>Hedgehog</span>
        </div>

        {/* Tabs */}
        <div style={{ display: 'flex', alignItems: 'stretch', paddingLeft: 4 }}>
          {tabs.map((tab, i) => {
            const isActive = activeTab === i;
            return (
              <button
                key={tab.name}
                onClick={() => setActiveTab(i)}
                style={{
                  position: 'relative',
                  padding: '0 18px',
                  marginTop: 4,
                  marginRight: 1,
                  border: 'none',
                  borderTopLeftRadius: 4,
                  borderTopRightRadius: 4,
                  cursor: 'pointer',
                  fontSize: 12,
                  fontWeight: isActive ? 600 : 500,
                  letterSpacing: '0.02em',
                  color: isActive ? theme.activeTabColor : theme.tabTextColor,
                  background: isActive ? theme.panelBg : 'transparent',
                  transition: 'color 0.12s, background 0.12s',
                  fontFamily: theme.fontUI,
                }}
                onMouseEnter={(e) => { if (!isActive) e.currentTarget.style.color = theme.activeTabColor; }}
                onMouseLeave={(e) => { if (!isActive) e.currentTarget.style.color = theme.tabTextColor; }}
              >
                {/* amber accent strip across the active tab */}
                {isActive && (
                  <span style={{
                    position: 'absolute',
                    left: 8, right: 8, top: 0,
                    height: 2,
                    background: theme.accentColor,
                    borderRadius: '0 0 1px 1px',
                  }} />
                )}
                {tab.name}
              </button>
            );
          })}
        </div>

        {/* Spacer pushes anything we add later (status, etc.) right */}
        <div style={{ flex: 1 }} />
      </div>

      {/* ── Tab content ─────────────────────────────────────────────── */}
      <div style={{
        flex: 1,
        overflow: 'hidden',
        position: 'relative',
        background: theme.contentBg,
        padding: 4,
      }}>
        {tabs.map((tab, i) => (
          <div
            key={tab.name}
            style={{
              display: activeTab === i ? 'flex' : 'none',
              width: '100%',
              height: '100%',
            }}
          >
            {tab.content}
          </div>
        ))}
      </div>

      {/* ── Status footer ───────────────────────────────────────────── */}
      <div style={{
        height: 22,
        flexShrink: 0,
        background: theme.topBarBg,
        borderTop: `1px solid ${theme.borderDark}`,
        boxShadow: 'inset 0 1px 0 rgba(255,255,255,0.03)',
        display: 'flex',
        alignItems: 'center',
        padding: '0 12px',
        gap: 12,
        fontSize: 10.5,
        color: theme.textColorDim,
        letterSpacing: '0.04em',
        fontFamily: theme.fontUI,
        userSelect: 'none',
      }}>
        <div style={{
          display: 'flex',
          alignItems: 'center',
          gap: 6,
        }}>
          <span style={{
            width: 6, height: 6, borderRadius: 3,
            background: theme.successColor,
            boxShadow: `0 0 4px ${theme.successColor}88`,
          }} />
          <span>Ready</span>
        </div>
        <div style={{ width: 1, height: 12, background: theme.borderDark }} />
        <div>{tabs[activeTab]?.name}</div>
        <div style={{ flex: 1 }} />
        <div style={{ opacity: 0.7 }}>HedgehogFund</div>
      </div>
    </div>
  );
}
