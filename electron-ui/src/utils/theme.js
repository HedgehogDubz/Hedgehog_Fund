// Original tabdock Monokai palette — same colors apply_theme('monokai')
// hands back in the PyQt build, with a few extra tokens added for the
// React chrome (bevels, hover, status). Active tabs share the panel
// background so they melt into the content beneath them.

const ACTIVE_BG = '#2d2a2e';   // content_bg / panel_bg / active_tab_color
const CHROME    = '#19181a';   // tab_bar_bg / dock_bg / dock_border_color
const ACCENT    = '#b8860b';   // dark gold (chrome accent)

export const theme = {
  // ── Surfaces ────────────────────────────────────────────────────────────
  appBg:           CHROME,
  contentBg:       ACTIVE_BG,
  panelBg:         ACTIVE_BG,
  dockBg:          ACTIVE_BG,
  tabBarBg:        CHROME,
  topBarBg:        CHROME,

  // Inputs/buttons sit on a slightly lighter facet of the panel
  widgetBg:        '#3a3739',
  widgetBgHover:   '#46434a',
  widgetBgActive:  '#221f23',
  inputBg:         '#221f23',

  // ── Borders (subtle pseudo-3D) ─────────────────────────────────────────
  borderDark:        CHROME,
  borderLight:       '#403d42',
  dockBorderColor:   CHROME,
  separator:         '#3a3739',
  borderWidth:       1,

  // ── Text ───────────────────────────────────────────────────────────────
  textColor:         '#fcfcfa',
  textColorMuted:    '#a59fa6',
  textColorDim:      '#6e6a72',
  tabTextColor:      '#a59fa6',
  activeTabColor:    '#fcfcfa',

  // ── Accent (dark gold) ─────────────────────────────────────────────────
  accentColor:       ACCENT,
  accentColorDim:    '#8a6508',
  accentColorSoft:   'rgba(184,134,11,0.18)',

  // ── Status (monokai pro syntax greens/reds) ────────────────────────────
  successColor:      '#a9dc76',
  warningColor:      '#fc9867',
  errorColor:        '#ff6188',

  // ── Layout ─────────────────────────────────────────────────────────────
  dockTabHeight:     28,
  topTabHeight:      32,

  // ── Chart palette (data, not chrome) — Monokai bright ──────────────────
  chartColors: ['#ffd866', '#78dce8', '#ff6188', '#a9dc76', '#fc9867', '#ab9df2', '#a1efe4', '#fcfcfa'],

  // ── Syntax (IDE) — Monokai Pro ─────────────────────────────────────────
  syntax: {
    keyword:  '#ff6188',
    type:     '#78dce8',
    string:   '#ffd866',
    number:   '#ab9df2',
    comment:  '#727072',
    function: '#a9dc76',
    operator: '#ff6188',
    self:     '#fc9867',
  },

  // ── Fonts ──────────────────────────────────────────────────────────────
  fontUI:   '-apple-system, BlinkMacSystemFont, "Segoe UI", "Inter", Roboto, sans-serif',
  fontMono: '"JetBrains Mono", Menlo, Consolas, monospace',
};
