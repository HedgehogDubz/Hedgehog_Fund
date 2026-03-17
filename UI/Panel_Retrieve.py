from datetime import datetime, timedelta
from PyQt6.QtWidgets import QWidget, QHBoxLayout, QLineEdit
from PyQt6.QtCore import Qt
from PyQt6.QtGui import QKeyEvent
from tabdock import Panel
from retrieve_data import (
    INTERVALS, OFFER_SIDES, CATEGORIES,
    get_instruments_for_category, resolve_ticker, fetch_data,
)


class TickerInput(QWidget):
    """Segmented 5-character ticker input. Each cell holds one character."""

    def __init__(self, parent_panel, num_chars=5, string_key=None, default=""):
        super().__init__(parent_panel)
        self._panel = parent_panel
        self._num = num_chars
        self._string_key = string_key
        self._syncing = False

        layout = QHBoxLayout(self)
        layout.setContentsMargins(0, 0, 0, 0)
        layout.setSpacing(2)

        bg = parent_panel.widget_bg
        fg = parent_panel.text_color
        accent = parent_panel.accent_color

        self._cells = []
        for i in range(num_chars):
            cell = QLineEdit(self)
            cell.setMaxLength(1)
            cell.setAlignment(Qt.AlignmentFlag.AlignCenter)
            cell.setFixedWidth(28)
            cell.setFixedHeight(28)
            radius_l = "4px" if i == 0 else "0px"
            radius_r = "4px" if i == num_chars - 1 else "0px"
            cell.setStyleSheet(f"""
                QLineEdit {{
                    background-color: {bg};
                    color: {fg};
                    border: 1px solid transparent;
                    border-top-left-radius: {radius_l};
                    border-bottom-left-radius: {radius_l};
                    border-top-right-radius: {radius_r};
                    border-bottom-right-radius: {radius_r};
                    padding: 0px;
                    font-size: 13px;
                    font-weight: bold;
                    font-family: "Menlo";
                }}
                QLineEdit:focus {{
                    border: 1px solid {accent};
                }}
            """)
            cell.setPlaceholderText("-")
            cell.textChanged.connect(lambda text, idx=i: self._on_cell_changed(idx, text))
            cell.installEventFilter(self)
            layout.addWidget(cell)
            self._cells.append(cell)

        layout.addStretch()

        if string_key and default:
            self._set_text(default)
            if not parent_panel.state.has(string_key):
                parent_panel.state.set(string_key, default)

    def _on_cell_changed(self, idx, text):
        if self._syncing:
            return
        if text and text.isalpha():
            self._cells[idx].setText(text.upper())
        if text and idx < self._num - 1:
            self._cells[idx + 1].setFocus()
            self._cells[idx + 1].setCursorPosition(0)
        self._sync_state()

    def _sync_state(self):
        if self._syncing or not self._string_key:
            return
        self._syncing = True
        val = "".join(c.text() for c in self._cells).strip()
        self._panel.state.set(self._string_key, val)
        self._syncing = False

    def _set_text(self, text):
        self._syncing = True
        for i, cell in enumerate(self._cells):
            cell.setText(text[i].upper() if i < len(text) else "")
        self._syncing = False

    def text(self):
        return "".join(c.text() for c in self._cells).strip()

    def eventFilter(self, obj, event):
        if isinstance(event, QKeyEvent) and event.type() == event.Type.KeyPress:
            idx = self._cells.index(obj) if obj in self._cells else -1
            if idx < 0:
                return super().eventFilter(obj, event)
            if event.key() == Qt.Key.Key_Backspace:
                if obj.text():
                    obj.clear()
                    self._sync_state()
                if idx > 0:
                    self._cells[idx - 1].setFocus()
                    self._cells[idx - 1].deselect()
                    self._cells[idx - 1].setCursorPosition(len(self._cells[idx - 1].text()))
                return True
            text = event.text()
            if text and text.isalpha() and obj.text():
                self._syncing = True
                obj.setText(text.upper())
                self._syncing = False
                self._sync_state()
                if idx < self._num - 1:
                    self._cells[idx + 1].setFocus()
                    self._cells[idx + 1].setCursorPosition(0)
                return True
            if event.key() == Qt.Key.Key_Left and obj.cursorPosition() == 0:
                if idx > 0:
                    self._cells[idx - 1].setFocus()
                    self._cells[idx - 1].deselect()
                    self._cells[idx - 1].setCursorPosition(len(self._cells[idx - 1].text()))
                return True
            if event.key() == Qt.Key.Key_Right and obj.cursorPosition() >= len(obj.text()):
                if idx < self._num - 1:
                    self._cells[idx + 1].setFocus()
                    self._cells[idx + 1].setCursorPosition(0)
                return True
        return super().eventFilter(obj, event)


class Retrieve(Panel):
    def __init__(self, parent, docked, x, y, w, h, **kw):
        super().__init__(parent, docked, x, y, w, h, **kw)

        self.add_label("Mode")
        self.next_row()
        self.add_dropdown(
            ["Ticker", "Browse"],
            callback=self._on_mode_changed,
            string_key="retrieve_mode",
            default="Ticker",
        )

        self.next_row()
        self._ticker_label = self.add_label("Ticker")
        self.next_row()
        self._ticker_input = TickerInput(self, num_chars=5, string_key="retrieve_ticker", default="AAPL")
        self._current_row.addWidget(self._ticker_input)

        self.next_row()
        self._cat_label = self.add_label("Category")
        self.next_row()
        self._cat_dropdown = self.add_dropdown(
            list(CATEGORIES.keys()),
            callback=self._on_category_changed,
            string_key="retrieve_category",
            default="FX Majors",
        )
        self.next_row()
        self._inst_label = self.add_label("Instrument")
        self.next_row()
        instruments = get_instruments_for_category("FX Majors")
        self._inst_dropdown = self.add_dropdown(
            instruments,
            string_key="retrieve_instrument",
            default=instruments[0] if instruments else "",
        )

        self._browse_widgets = [
            self._cat_label, self._cat_dropdown,
            self._inst_label, self._inst_dropdown,
        ]
        self._ticker_widgets = [self._ticker_label, self._ticker_input]

        LOOKBACK_OPTIONS = [
            "1 Day", "3 Days", "1 Week", "2 Weeks", "1 Month",
            "3 Months", "6 Months", "1 Year", "2 Years", "5 Years",
        ]

        self.next_row()
        self.add_separator()
        self.add_label("Date Mode")
        self.next_row()
        self.add_dropdown(
            ["Lookback", "Text", "Calendar"],
            callback=self._on_date_mode_changed,
            string_key="retrieve_date_mode",
            default="Lookback",
        )

        self.next_row()
        self._lookback_label = self.add_label("Look into the past")
        self.next_row()
        self._lookback_dropdown = self.add_dropdown(
            LOOKBACK_OPTIONS,
            string_key="retrieve_lookback",
            default="1 Month",
        )

        self.next_row()
        self._text_start_label = self.add_label("Start")
        self._text_start_input = self.add_text_input(
            placeholder="2025-01-01",
            string_key="retrieve_start",
            default="2025-01-01",
        )
        self._text_end_label = self.add_label("End")
        self._text_end_input = self.add_text_input(
            placeholder="2025-02-01",
            string_key="retrieve_end",
            default="2025-02-01",
        )

        self.next_row()
        self._cal_container = QWidget(self)
        cal_layout = QHBoxLayout(self._cal_container)
        cal_layout.setContentsMargins(0, 0, 0, 0)
        cal_layout.setSpacing(8)

        from PyQt6.QtWidgets import QVBoxLayout
        left_col = QVBoxLayout()
        left_col.setSpacing(4)
        self._cal_start_label = self._make_styled_label("Start Date")
        left_col.addWidget(self._cal_start_label)
        self._cal_start = self.add_calendar(string_key="retrieve_cal_start")
        self._root_layout.removeWidget(self._cal_start)
        left_col.addWidget(self._cal_start)

        right_col = QVBoxLayout()
        right_col.setSpacing(4)
        self._cal_end_label = self._make_styled_label("End Date")
        right_col.addWidget(self._cal_end_label)
        self._cal_end = self.add_calendar(string_key="retrieve_cal_end")
        self._root_layout.removeWidget(self._cal_end)
        right_col.addWidget(self._cal_end)

        cal_layout.addLayout(left_col)
        cal_layout.addLayout(right_col)
        self._root_layout.addWidget(self._cal_container)

        self._lookback_widgets = [self._lookback_label, self._lookback_dropdown]
        self._text_date_widgets = [
            self._text_start_label, self._text_start_input,
            self._text_end_label, self._text_end_input,
        ]
        self._cal_widgets = [self._cal_container]
        self._apply_date_mode("Lookback")

        self.next_row()
        self.add_separator()
        self.add_label("Interval")
        self.add_dropdown(
            list(INTERVALS.keys()),
            string_key="retrieve_interval",
            default="1 Hour",
        )

        self.next_row()
        self.add_label("Offer Side")
        self.add_dropdown(
            list(OFFER_SIDES.keys()),
            string_key="retrieve_offer_side",
            default="Bid",
        )

        self.next_row()
        self._status = self.add_label("")
        self.next_row()
        self.add_button("Fetch", callback=self._on_fetch)

        self._apply_mode("Ticker")

    def _make_styled_label(self, text):
        from PyQt6.QtWidgets import QLabel
        w = QLabel(text, self)
        w.setStyleSheet(f"color: {self.text_color}; background: transparent; padding: 2px 0px; font-size: 12px;")
        return w

    def _apply_mode(self, mode):
        for w in self._ticker_widgets:
            w.setVisible(mode == "Ticker")
        for w in self._browse_widgets:
            w.setVisible(mode == "Browse")

    def _on_mode_changed(self, mode):
        self._apply_mode(mode)

    def _apply_date_mode(self, mode):
        for w in self._lookback_widgets:
            w.setVisible(mode == "Lookback")
        for w in self._text_date_widgets:
            w.setVisible(mode == "Text")
        for w in self._cal_widgets:
            w.setVisible(mode == "Calendar")

    def _on_date_mode_changed(self, mode):
        self._apply_date_mode(mode)

    def _resolve_dates(self, date_mode):
        LOOKBACK_DELTAS = {
            "1 Day": timedelta(days=1),
            "3 Days": timedelta(days=3),
            "1 Week": timedelta(weeks=1),
            "2 Weeks": timedelta(weeks=2),
            "1 Month": timedelta(days=30),
            "3 Months": timedelta(days=90),
            "6 Months": timedelta(days=180),
            "1 Year": timedelta(days=365),
            "2 Years": timedelta(days=730),
            "5 Years": timedelta(days=1825),
        }

        if date_mode == "Lookback":
            label = self.state.get("retrieve_lookback", "1 Month")
            delta = LOOKBACK_DELTAS.get(label, timedelta(days=30))
            end = datetime.now()
            start = end - delta
            return start, end

        if date_mode == "Text":
            start_str = self.state.get("retrieve_start", "")
            end_str = self.state.get("retrieve_end", "")
            if not start_str or not end_str:
                raise ValueError("Fill in both dates.")
            try:
                start = datetime.strptime(start_str, "%Y-%m-%d")
                end = datetime.strptime(end_str, "%Y-%m-%d")
            except Exception:
                raise ValueError("Invalid date format (YYYY-MM-DD).")
            return start, end

        start_str = self.state.get("retrieve_cal_start", "")
        end_str = self.state.get("retrieve_cal_end", "")
        if not start_str or not end_str:
            raise ValueError("Select both dates.")
        start = datetime.strptime(start_str, "%Y-%m-%d")
        end = datetime.strptime(end_str, "%Y-%m-%d")
        return start, end

    def _on_category_changed(self, category):
        instruments = get_instruments_for_category(category)
        self._inst_dropdown.clear()
        self._inst_dropdown.addItems(instruments)
        if instruments:
            self.state.set("retrieve_instrument", instruments[0])

    def _on_fetch(self):
        mode = self.state.get("retrieve_mode", "Ticker")

        if mode == "Ticker":
            ticker = self.state.get("retrieve_ticker", "").strip()
            instrument = resolve_ticker(ticker)
            if not instrument:
                self._status.setText(f"Unknown ticker: {ticker}")
                return
        else:
            instrument = self.state.get("retrieve_instrument", "")

        interval = self.state.get("retrieve_interval", "1 Hour")
        offer_side = self.state.get("retrieve_offer_side", "Bid")
        date_mode = self.state.get("retrieve_date_mode", "Lookback")

        if not instrument:
            self._status.setText("Fill in all fields.")
            return

        try:
            start, end = self._resolve_dates(date_mode)
        except ValueError as e:
            self._status.setText(str(e))
            return

        if end <= start:
            self._status.setText("End must be after start.")
            return

        self._status.setText("Fetching...")
        try:
            path = fetch_data(instrument, interval, offer_side, start, end)
            self._status.setText(f"Saved: {path.split('/')[-1]}")
        except Exception as e:
            self._status.setText(f"Error: {e}")
