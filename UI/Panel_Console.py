from tabdock import Panel
from PyQt6.QtWidgets import QSizePolicy
from PyQt6.QtCore import Qt


class Console(Panel):
    def __init__(self, parent, docked, x, y, w, h, **kw):
        super().__init__(parent, docked, x, y, w, h, **kw)

        # Remove AlignTop so rows can fill vertically, collapse spacing for unified look
        self._root_layout.setAlignment(Qt.AlignmentFlag(0))
        self._root_layout.setSpacing(0)

        self.label_console = self.add_label("test", state_key="console_text")
        self.label_console.setStyleSheet(f"background-color: {self.widget_bg};")
        self.label_console.setSizePolicy(QSizePolicy.Policy.Expanding, QSizePolicy.Policy.Expanding)
        self.label_console.setAlignment(Qt.AlignmentFlag.AlignBottom | Qt.AlignmentFlag.AlignLeft)

        # Stretch the console label row to fill all available space above the input
        self._root_layout.setStretchFactor(self._current_row, 1)

        self.next_row()
        self._current_row.setSpacing(0)
        self.label_text_indicator = self.add_label("% ")
        self.label_text_indicator.setStyleSheet(f"background-color: {self.widget_bg};")
        self.line_edit = self.add_text_input()
        self.line_edit.setStyleSheet(f"background-color: {self.widget_bg};")
        
        