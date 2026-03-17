from PyQt6.QtWidgets import QTableWidget, QTableWidgetItem
from PyQt6.QtCore import Qt
from tabdock import Panel
from tabdock.panel_state import PanelStateManager
from retrieve_data import read_parquet_preview
from UI.Panel_DataFileList import DataFileList


class TablePreview(Panel):
    def __init__(self, parent, docked, x, y, w, h, **kw):
        super().__init__(parent, docked, x, y, w, h, **kw)

        self._table = QTableWidget(self)
        self._table.setStyleSheet(f"""
            QTableWidget {{
                background-color: {self.widget_bg};
                color: {self.text_color};
                border: none;
                gridline-color: {self.panel_bg};
                font-size: 11px;
            }}
            QHeaderView::section {{
                background-color: {self.panel_bg};
                color: {self.text_color};
                border: none;
                padding: 4px;
                font-size: 11px;
                font-weight: bold;
            }}
            QTableWidget::item {{
                padding: 2px 6px;
            }}
            QScrollBar:vertical {{
                background: {self.panel_bg};
                width: 8px;
            }}
            QScrollBar::handle:vertical {{
                background: {self.widget_bg};
                border-radius: 4px;
                min-height: 20px;
            }}
            QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical {{
                height: 0px;
            }}
            QScrollBar:horizontal {{
                background: {self.panel_bg};
                height: 8px;
            }}
            QScrollBar::handle:horizontal {{
                background: {self.widget_bg};
                border-radius: 4px;
                min-width: 20px;
            }}
            QScrollBar::add-line:horizontal, QScrollBar::sub-line:horizontal {{
                width: 0px;
            }}
        """)
        self._table.horizontalHeader().setStretchLastSection(True)
        self._table.verticalHeader().setVisible(False)
        self._table.setEditTriggers(QTableWidget.EditTrigger.NoEditTriggers)
        self._root_layout.addWidget(self._table, 1)

        self._current_df = None
        self._file_state = PanelStateManager.for_class(DataFileList)
        self._file_state.subscribe("preview_selected_file", self._on_selection_changed)

    def _on_selection_changed(self, selected):
        if not selected:
            self._current_df = None
            self._table.setRowCount(0)
            self._table.setColumnCount(0)
            return

        df = read_parquet_preview(selected[0])
        self._current_df = df
        self._render_table()

    def _render_table(self):
        df = self._current_df
        if df is None:
            self._table.setRowCount(0)
            self._table.setColumnCount(0)
            return

        self._table.setRowCount(len(df))
        self._table.setColumnCount(len(df.columns))
        self._table.setHorizontalHeaderLabels(list(df.columns))

        for r, (_, row) in enumerate(df.iterrows()):
            for c, val in enumerate(row):
                item = QTableWidgetItem(str(val))
                item.setTextAlignment(Qt.AlignmentFlag.AlignRight | Qt.AlignmentFlag.AlignVCenter)
                self._table.setItem(r, c, item)

        self._table.resizeColumnsToContents()
