from pathlib import Path
from PyQt6.QtWidgets import QWidget, QHBoxLayout, QVBoxLayout, QPushButton, QSplitter
from PyQt6.QtCore import Qt
from PyQt6.QtGui import QIcon, QPixmap, QColor, QPainter
from tabdock import Panel
from tabdock.panel_state import PanelStateManager
from retrieve_data import read_parquet_preview
from UI.Panel_AnalyzeDataFileList import AnalyzeDataFileList

_ICON_DIR = Path(__file__).parent / "icons"


def _tinted_icon(filename, color):
    """Load an icon file and tint it to the given color."""
    path = str(_ICON_DIR / filename)
    pixmap = QPixmap(path)
    if pixmap.isNull():
        return QIcon()
    painter = QPainter(pixmap)
    painter.setCompositionMode(QPainter.CompositionMode.CompositionMode_SourceIn)
    painter.fillRect(pixmap.rect(), QColor(color))
    painter.end()
    return QIcon(pixmap)


class AnalyzeChart(Panel):
    _COLORS = ["#e6db74", "#66d9ef", "#f92672", "#a6e22e", "#fd971f", "#ae81ff", "#a1efe4", "#f8f8f2"]

    def __init__(self, parent, docked, x, y, w, h, **kw):
        super().__init__(parent, docked, x, y, w, h, **kw)

        self._chart_style_combo = self.add_dropdown(
            ["Line", "Mountain", "Candle", "OHLC", "All Lines"],
            callback=lambda _: self._render_chart(),
            string_key="chart_style",
            default="Line",
        )

        self.next_row()

        import pyqtgraph as pg
        pg.setConfigOptions(background=self.panel_bg, foreground=self.text_color, antialias=True)

        chart_container = QWidget(self)
        chart_layout = QHBoxLayout(chart_container)
        chart_layout.setContentsMargins(0, 0, 0, 0)
        chart_layout.setSpacing(0)

        self._chart_splitter = QSplitter(Qt.Orientation.Vertical)
        self._chart_splitter.setStyleSheet(f"QSplitter::handle {{ background-color: {self.panel_bg}; height: 2px; }}")

        self._chart = pg.PlotWidget()
        self._chart.setStyleSheet(f"background-color: {self.panel_bg}; border: none;")
        self._chart.showGrid(x=True, y=True, alpha=0.15)
        self._chart.setLabel("left", "Price")

        self._volume_chart = pg.PlotWidget()
        self._volume_chart.setStyleSheet(f"background-color: {self.panel_bg}; border: none;")
        self._volume_chart.showGrid(x=True, y=True, alpha=0.15)
        self._volume_chart.setLabel("left", "Volume")
        self._volume_chart.setXLink(self._chart)
        self._volume_chart.setVisible(False)

        self._chart_splitter.addWidget(self._chart)
        self._chart_splitter.addWidget(self._volume_chart)
        self._chart_splitter.setSizes([700, 300])

        btn_style = f"""
            QPushButton {{
                background-color: {self.widget_bg}; color: {self.text_color};
                border: none; padding: 4px 6px; border-radius: 3px;
            }}
            QPushButton:hover {{ background-color: {self.accent_color}; }}
            QPushButton:checked {{ background-color: {self.accent_color}; }}
        """
        toolbar = QWidget()
        toolbar.setFixedWidth(32)
        toolbar.setStyleSheet(f"background-color: {self.panel_bg};")
        tb_layout = QVBoxLayout(toolbar)
        tb_layout.setContentsMargins(2, 4, 2, 4)
        tb_layout.setSpacing(4)

        icon_color = self.text_color

        self._btn_home = QPushButton()
        self._btn_home.setIcon(_tinted_icon("icons8-home.svg", icon_color))
        self._btn_home.setStyleSheet(btn_style)
        self._btn_home.setToolTip("Reset view")
        self._btn_home.clicked.connect(self._chart_home)

        self._btn_pan = QPushButton()
        self._btn_pan.setIcon(_tinted_icon("icons8-hand-30.png", icon_color))
        self._btn_pan.setStyleSheet(btn_style)
        self._btn_pan.setCheckable(True)
        self._btn_pan.setChecked(True)
        self._btn_pan.setToolTip("Pan")
        self._btn_pan.clicked.connect(lambda: self._set_chart_mode("pan"))

        self._btn_zoom = QPushButton()
        self._btn_zoom.setIcon(_tinted_icon("icons8-magnifying-glass.svg", icon_color))
        self._btn_zoom.setStyleSheet(btn_style)
        self._btn_zoom.setCheckable(True)
        self._btn_zoom.setToolTip("Zoom to rect")
        self._btn_zoom.clicked.connect(lambda: self._set_chart_mode("zoom"))

        self._btn_hresize = QPushButton()
        self._btn_hresize.setIcon(_tinted_icon("icons8-resize-horizontal-50.png", icon_color))
        self._btn_hresize.setStyleSheet(btn_style)
        self._btn_hresize.setCheckable(True)
        self._btn_hresize.setToolTip("Resize Horizontally")
        self._btn_hresize.clicked.connect(lambda: self._set_chart_mode("hresize"))

        self._btn_vresize = QPushButton()
        self._btn_vresize.setIcon(_tinted_icon("icons8-resize-vertical-50.png", icon_color))
        self._btn_vresize.setStyleSheet(btn_style)
        self._btn_vresize.setCheckable(True)
        self._btn_vresize.setToolTip("Resize Vertically")
        self._btn_vresize.clicked.connect(lambda: self._set_chart_mode("vresize"))

        for btn in [self._btn_home, self._btn_pan, self._btn_zoom, self._btn_hresize, self._btn_vresize]:
            tb_layout.addWidget(btn)
        tb_layout.addStretch()

        chart_layout.addWidget(self._chart_splitter, 1)
        chart_layout.addWidget(toolbar)
        self._root_layout.addWidget(chart_container, 1)
        self._has_dates = False
        self._legend = None

        self._current_df = None
        self._file_state = PanelStateManager.for_class(AnalyzeDataFileList)
        self._file_state.subscribe("analyze_selected_file", self._on_selection_changed)

    def _chart_home(self):
        self._chart.autoRange()
        if self._volume_chart.isVisible():
            self._volume_chart.autoRange()

    def _set_chart_mode(self, mode):
        for chart in [self._chart, self._volume_chart]:
            vb = chart.getViewBox()
            if mode == "pan":
                vb.setMouseMode(vb.PanMode)
                vb.setMouseEnabled(x=True, y=True)
            elif mode == "zoom":
                vb.setMouseMode(vb.RectMode)
                vb.setMouseEnabled(x=True, y=True)
            elif mode == "hresize":
                vb.setMouseMode(vb.PanMode)
                vb.setMouseEnabled(x=True, y=False)
            elif mode == "vresize":
                vb.setMouseMode(vb.PanMode)
                vb.setMouseEnabled(x=False, y=True)
        self._btn_pan.setChecked(mode == "pan")
        self._btn_zoom.setChecked(mode == "zoom")
        self._btn_hresize.setChecked(mode == "hresize")
        self._btn_vresize.setChecked(mode == "vresize")

    def _on_selection_changed(self, selected):
        if not selected:
            self._current_df = None
            self._chart.clear()
            self._chart.setTitle("")
            self._volume_chart.clear()
            self._volume_chart.setVisible(False)
            return

        df = read_parquet_preview(selected[0])
        self._current_df = df
        self._render_chart()

    def _render_chart(self):
        self._chart.clear()
        self._chart.setTitle("")
        self._volume_chart.clear()
        if self._legend is not None:
            scene = self._legend.scene()
            if scene is not None:
                scene.removeItem(self._legend)
            self._legend = None
        df = self._current_df
        if df is None:
            self._volume_chart.setVisible(False)
            return

        import numpy as np
        import pandas as pd
        import pyqtgraph as pg

        time_col = None
        for col in df.columns:
            if col.lower() in ("timestamp", "time", "date", "datetime"):
                time_col = col
                break

        has_dates = time_col is not None
        if has_dates:
            x = (pd.to_datetime(df[time_col]).astype(np.int64) / 1e9).values
        else:
            x = np.arange(len(df), dtype=float)

        if has_dates != self._has_dates:
            self._has_dates = has_dates
            splitter = self._chart_splitter
            old_sizes = splitter.sizes()

            old_chart = self._chart
            if has_dates:
                self._chart = pg.PlotWidget(axisItems={"bottom": pg.DateAxisItem(orientation="bottom")})
            else:
                self._chart = pg.PlotWidget()
            self._chart.setStyleSheet(f"background-color: {self.panel_bg}; border: none;")
            self._chart.showGrid(x=True, y=True, alpha=0.15)
            splitter.replaceWidget(0, self._chart)
            old_chart.deleteLater()

            old_vol = self._volume_chart
            if has_dates:
                self._volume_chart = pg.PlotWidget(axisItems={"bottom": pg.DateAxisItem(orientation="bottom")})
            else:
                self._volume_chart = pg.PlotWidget()
            self._volume_chart.setStyleSheet(f"background-color: {self.panel_bg}; border: none;")
            self._volume_chart.showGrid(x=True, y=True, alpha=0.15)
            self._volume_chart.setXLink(self._chart)
            splitter.replaceWidget(1, self._volume_chart)
            old_vol.deleteLater()
            splitter.setSizes(old_sizes)

        numeric_cols = [c for c in df.columns if c != time_col and pd.api.types.is_numeric_dtype(df[c])]
        if not numeric_cols:
            self._chart.setTitle("No numeric columns found", color=self.text_color, size="10pt")
            self._volume_chart.setVisible(False)
            return

        volume_cols = [c for c in numeric_cols if c.lower() == "volume"]
        price_cols = [c for c in numeric_cols if c.lower() != "volume"]

        col_lower = {c: c.lower() for c in df.columns}
        close_col = next((c for c in price_cols if col_lower[c] == "close"), None)
        open_col = next((c for c in price_cols if col_lower[c] == "open"), None)
        high_col = next((c for c in price_cols if col_lower[c] == "high"), None)
        low_col = next((c for c in price_cols if col_lower[c] == "low"), None)
        has_ohlc = all(c is not None for c in [open_col, high_col, low_col, close_col])

        style = self._chart_style_combo.currentText()

        if style == "Line":
            target = close_col or (price_cols[0] if price_cols else None)
            if target:
                y = df[target].values.astype(float)
                self._chart.plot(x, y, pen=pg.mkPen(color="#e6db74", width=1.5))
                self._chart.setLabel("left", target.capitalize())
                self._chart.autoRange()
            else:
                self._chart.autoRange()

        elif style == "Mountain":
            target = close_col or (price_cols[0] if price_cols else None)
            if target:
                y = df[target].values.astype(float)
                fill = pg.FillBetweenItem(
                    pg.PlotCurveItem(x, y, pen=pg.mkPen(color="#66d9ef", width=1.5)),
                    pg.PlotCurveItem(x, np.full_like(y, np.nanmin(y)), pen=pg.mkPen(None)),
                    brush=pg.mkBrush("#66d9ef30"),
                )
                self._chart.addItem(fill)
                self._chart.plot(x, y, pen=pg.mkPen(color="#66d9ef", width=1.5))
                self._chart.setLabel("left", target.capitalize())
                self._chart.autoRange()

        elif style == "Candle" and has_ohlc:
            o = df[open_col].values.astype(float)
            h = df[high_col].values.astype(float)
            l = df[low_col].values.astype(float)
            c = df[close_col].values.astype(float)
            bar_w = (x[1] - x[0]) * 0.6 if len(x) > 1 else 1.0

            up = c >= o
            dn = ~up

            if np.any(up):
                self._chart.addItem(pg.BarGraphItem(
                    x=x[up], height=(c[up] - o[up]), y0=o[up], width=bar_w,
                    brush=pg.mkBrush("#00FF00"), pen=pg.mkPen("#00FF00", width=0.5)))
                for i in np.where(up)[0]:
                    self._chart.plot([x[i], x[i]], [l[i], h[i]], pen=pg.mkPen("#00FF00", width=0.5))

            if np.any(dn):
                self._chart.addItem(pg.BarGraphItem(
                    x=x[dn], height=(o[dn] - c[dn]), y0=c[dn], width=bar_w,
                    brush=pg.mkBrush("#FF0000"), pen=pg.mkPen("#FF0000", width=0.5)))
                for i in np.where(dn)[0]:
                    self._chart.plot([x[i], x[i]], [l[i], h[i]], pen=pg.mkPen("#FF0000", width=0.5))

            self._chart.setLabel("left", "Price")
            self._chart.autoRange()

        elif style == "OHLC" and has_ohlc:
            o = df[open_col].values.astype(float)
            h = df[high_col].values.astype(float)
            l = df[low_col].values.astype(float)
            c = df[close_col].values.astype(float)
            tick_w = (x[1] - x[0]) * 0.3 if len(x) > 1 else 0.5

            for i in range(len(x)):
                color = "#a6e22e" if c[i] >= o[i] else "#f92672"
                pen = pg.mkPen(color, width=1)
                self._chart.plot([x[i], x[i]], [l[i], h[i]], pen=pen)
                self._chart.plot([x[i] - tick_w, x[i]], [o[i], o[i]], pen=pen)
                self._chart.plot([x[i], x[i] + tick_w], [c[i], c[i]], pen=pen)

            self._chart.setLabel("left", "Price")
            self._chart.autoRange()

        elif style == "All Lines":
            if price_cols:
                self._legend = self._chart.addLegend(offset=(10, 10))
                for i, col in enumerate(price_cols):
                    color = self._COLORS[i % len(self._COLORS)]
                    y = df[col].values.astype(float)
                    self._chart.plot(x, y, pen=pg.mkPen(color=color, width=1.5), name=col)
                all_vals = np.concatenate([df[c].values.astype(float) for c in price_cols])
                y_min, y_max = np.nanmin(all_vals), np.nanmax(all_vals)
                margin = (y_max - y_min) * 0.05 if y_max > y_min else 1.0
                self._chart.setYRange(y_min - margin, y_max + margin)
                self._chart.setXRange(float(x[0]), float(x[-1]))
            self._chart.setLabel("left", "Value")

        else:
            target = close_col or (price_cols[0] if price_cols else None)
            if target:
                y = df[target].values.astype(float)
                self._chart.plot(x, y, pen=pg.mkPen(color="#e6db74", width=1.5))
                self._chart.setLabel("left", target.capitalize())
            self._chart.autoRange()

        if volume_cols:
            self._volume_chart.setVisible(True)
            for col in volume_cols:
                y = df[col].values.astype(float)
                bar_width = (x[1] - x[0]) * 0.8 if len(x) > 1 else 1.0
                self._volume_chart.addItem(pg.BarGraphItem(
                    x=x, height=y, width=bar_width, brush=pg.mkBrush("#66d9ef80")))
            self._volume_chart.setLabel("left", "Volume")
            self._volume_chart.autoRange()
        else:
            self._volume_chart.setVisible(False)
