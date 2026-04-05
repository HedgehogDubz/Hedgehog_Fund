from PyQt6.QtWidgets import (
    QPlainTextEdit, QTextEdit, QWidget, QHBoxLayout, QSizePolicy,
    QListWidget, QListWidgetItem,
)
from PyQt6.QtGui import (
    QFont, QColor, QTextCharFormat, QPainter, QPen,
    QKeySequence, QShortcut,
)
from PyQt6.QtCore import Qt, QRect, QSize, QTimer
from tabdock import Panel
from tabdock.panel_state import PanelStateManager
from UI.Panel_CreateFileList import CreateFileList, CREATIONS_DIR
from UI.Highlighter_Python import PythonHighlighter
from UI.Highlighter_Cpp import CppHighlighter
from UI.Intellisense_Python import get_python_completions
from UI.Intellisense_Cpp import get_cpp_completions


# ── Line-number gutter ────────────────────────────────────────────────

class LineNumberArea(QWidget):
    def __init__(self, editor):
        super().__init__(editor)
        self._editor = editor

    def sizeHint(self):
        return QSize(self._editor.line_number_area_width(), 0)

    def paintEvent(self, event):
        self._editor.line_number_area_paint(event)


class CodeEditor(QPlainTextEdit):
    def __init__(self, bg, fg, gutter_bg, accent, parent=None):
        super().__init__(parent)
        self._bg = bg
        self._fg = fg
        self._gutter_bg = gutter_bg
        self._accent = accent

        font = QFont("Menlo", 13)
        font.setStyleHint(QFont.StyleHint.Monospace)
        self.setFont(font)
        self.setTabStopDistance(self.fontMetrics().horizontalAdvance(" ") * 4)
        self.setLineWrapMode(QPlainTextEdit.LineWrapMode.NoWrap)

        self.setStyleSheet(f"""
            QPlainTextEdit {{
                background-color: {bg};
                color: {fg};
                border: none;
                selection-background-color: #49483e;
                selection-color: {fg};
            }}
            QScrollBar:vertical {{
                background: {bg}; width: 10px;
            }}
            QScrollBar::handle:vertical {{
                background: {gutter_bg}; border-radius: 5px; min-height: 20px;
            }}
            QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical {{
                height: 0px;
            }}
            QScrollBar:horizontal {{
                background: {bg}; height: 10px;
            }}
            QScrollBar::handle:horizontal {{
                background: {gutter_bg}; border-radius: 5px; min-width: 20px;
            }}
            QScrollBar::add-line:horizontal, QScrollBar::sub-line:horizontal {{
                width: 0px;
            }}
        """)

        self._line_number_area = LineNumberArea(self)
        self.blockCountChanged.connect(self._update_line_number_area_width)
        self.updateRequest.connect(self._update_line_number_area)
        self.cursorPositionChanged.connect(self._highlight_current_line)
        self._update_line_number_area_width()
        self._highlight_current_line()

        # Autocomplete popup
        self._completer = QListWidget(self)
        self._completer.setWindowFlags(Qt.WindowType.ToolTip)
        self._completer.setStyleSheet(f"""
            QListWidget {{
                background-color: #272822;
                color: {fg};
                border: 1px solid #49483e;
                font-family: Menlo;
                font-size: 12px;
                padding: 2px;
            }}
            QListWidget::item {{
                padding: 2px 6px;
            }}
            QListWidget::item:selected {{
                background-color: #49483e;
                color: #f8f8f2;
            }}
        """)
        self._completer.setMaximumHeight(200)
        self._completer.setMaximumWidth(300)
        self._completer.setHorizontalScrollBarPolicy(Qt.ScrollBarPolicy.ScrollBarAlwaysOff)
        self._completer.hide()
        self._completer.itemClicked.connect(self._insert_completion)
        self._completion_prefix = ""
        self._lang = "python"

        self._complete_timer = QTimer(self)
        self._complete_timer.setSingleShot(True)
        self._complete_timer.setInterval(150)
        self._complete_timer.timeout.connect(self._update_completions)

    def line_number_area_width(self):
        digits = max(1, len(str(self.blockCount())))
        return 10 + self.fontMetrics().horizontalAdvance("9") * digits + 10

    def _update_line_number_area_width(self):
        self.setViewportMargins(self.line_number_area_width(), 0, 0, 0)

    def _update_line_number_area(self, rect, dy):
        if dy:
            self._line_number_area.scroll(0, dy)
        else:
            self._line_number_area.update(0, rect.y(), self._line_number_area.width(), rect.height())
        if rect.contains(self.viewport().rect()):
            self._update_line_number_area_width()

    def resizeEvent(self, event):
        super().resizeEvent(event)
        cr = self.contentsRect()
        self._line_number_area.setGeometry(
            QRect(cr.left(), cr.top(), self.line_number_area_width(), cr.height())
        )

    def line_number_area_paint(self, event):
        painter = QPainter(self._line_number_area)
        painter.fillRect(event.rect(), QColor(self._gutter_bg))
        pen = QPen(QColor("#75715e"))
        painter.setPen(pen)
        painter.setFont(self.font())

        block = self.firstVisibleBlock()
        block_number = block.blockNumber()
        top = round(self.blockBoundingGeometry(block).translated(self.contentOffset()).top())
        bottom = top + round(self.blockBoundingRect(block).height())

        while block.isValid() and top <= event.rect().bottom():
            if block.isVisible() and bottom >= event.rect().top():
                painter.drawText(
                    0, top,
                    self._line_number_area.width() - 6,
                    self.fontMetrics().height(),
                    Qt.AlignmentFlag.AlignRight,
                    str(block_number + 1),
                )
            block = block.next()
            top = bottom
            bottom = top + round(self.blockBoundingRect(block).height())
            block_number += 1
        painter.end()

    def _highlight_current_line(self):
        selections = []
        if not self.isReadOnly():
            selection = QTextEdit.ExtraSelection()
            selection.format.setBackground(QColor("#3e3d32"))
            selection.format.setProperty(QTextCharFormat.Property.FullWidthSelection, True)
            selection.cursor = self.textCursor()
            selection.cursor.clearSelection()
            selections.append(selection)
        self.setExtraSelections(selections)

    def set_language(self, lang):
        self._lang = lang

    def keyPressEvent(self, event):
        # Handle completer navigation when visible
        if self._completer.isVisible():
            if event.key() == Qt.Key.Key_Return or event.key() == Qt.Key.Key_Tab:
                current = self._completer.currentItem()
                if current:
                    self._insert_completion(current)
                    return
            elif event.key() == Qt.Key.Key_Escape:
                self._completer.hide()
                return
            elif event.key() == Qt.Key.Key_Down:
                row = self._completer.currentRow()
                if row < self._completer.count() - 1:
                    self._completer.setCurrentRow(row + 1)
                return
            elif event.key() == Qt.Key.Key_Up:
                row = self._completer.currentRow()
                if row > 0:
                    self._completer.setCurrentRow(row - 1)
                return

        # Block indent/dedent
        if event.key() == Qt.Key.Key_Tab and self.textCursor().hasSelection():
            self._indent_selection(indent=True)
            return
        if event.key() == Qt.Key.Key_Backtab and self.textCursor().hasSelection():
            self._indent_selection(indent=False)
            return

        super().keyPressEvent(event)

        # Trigger completion after typing
        if event.text().isalnum() or event.text() in ("_", ".", ":"):
            self._complete_timer.start()
        else:
            self._completer.hide()

    def _get_current_prefix(self):
        cursor = self.textCursor()
        block_text = cursor.block().text()
        col = cursor.positionInBlock()
        # Walk backwards to find the start of the current word
        start = col
        while start > 0 and (block_text[start - 1].isalnum() or block_text[start - 1] in "_.:"):
            start -= 1
        return block_text[start:col]

    def _update_completions(self):
        prefix = self._get_current_prefix()
        if len(prefix) < 2:
            self._completer.hide()
            return

        self._completion_prefix = prefix
        if self._lang == "cpp":
            results = get_cpp_completions(prefix)
        elif self._lang == "python":
            results = get_python_completions(prefix)
        else:
            results = []

        if not results:
            self._completer.hide()
            return

        self._completer.clear()
        for display, insertion in results[:15]:
            item = QListWidgetItem(display)
            item.setData(Qt.ItemDataRole.UserRole, insertion)
            self._completer.addItem(item)

        self._completer.setCurrentRow(0)
        self._position_completer()
        self._completer.show()

    def _position_completer(self):
        cursor_rect = self.cursorRect()
        # Map to global coordinates so the popup appears at the right spot
        global_pos = self.mapToGlobal(cursor_rect.bottomLeft())
        self._completer.move(global_pos)
        # Resize width to fit content
        width = min(300, max(150, self._completer.sizeHintForColumn(0) + 20))
        row_count = min(self._completer.count(), 10)
        height = row_count * (self._completer.sizeHintForRow(0) + 2) + 6
        self._completer.setFixedSize(width, height)

    def _insert_completion(self, item):
        insertion = item.data(Qt.ItemDataRole.UserRole)
        prefix = self._completion_prefix
        cursor = self.textCursor()
        # Remove the prefix that was already typed
        for _ in range(len(prefix)):
            cursor.deletePreviousChar()
        cursor.insertText(insertion)
        self.setTextCursor(cursor)
        self._completer.hide()

    def _indent_selection(self, indent=True):
        cursor = self.textCursor()
        start = cursor.selectionStart()
        end = cursor.selectionEnd()

        cursor.setPosition(start)
        cursor.movePosition(cursor.MoveOperation.StartOfBlock)
        cursor.setPosition(end, cursor.MoveMode.KeepAnchor)
        cursor.movePosition(cursor.MoveOperation.EndOfBlock, cursor.MoveMode.KeepAnchor)

        text = cursor.selectedText()
        # QPlainTextEdit uses \u2029 as block separator
        lines = text.split("\u2029")

        if indent:
            new_lines = ["    " + line for line in lines]
        else:
            new_lines = []
            for line in lines:
                if line.startswith("    "):
                    new_lines.append(line[4:])
                elif line.startswith("\t"):
                    new_lines.append(line[1:])
                elif line.startswith(" "):
                    new_lines.append(line.lstrip(" "))
                else:
                    new_lines.append(line)

        cursor.insertText("\u2029".join(new_lines))

        # Re-select the modified block
        cursor.setPosition(cursor.position() - len("\u2029".join(new_lines)))
        cursor.setPosition(cursor.position() + len("\u2029".join(new_lines)), cursor.MoveMode.KeepAnchor)
        self.setTextCursor(cursor)


# IDE Panel

class IDE(Panel):
    def __init__(self, parent, docked, x, y, w, h, **kw):
        super().__init__(parent, docked, x, y, w, h, **kw)

        self._current_file = None
        self._dirty = False

        self._root_layout.setAlignment(Qt.AlignmentFlag(0))
        self._root_layout.setSpacing(0)
        self._root_layout.setContentsMargins(0, 0, 0, 0)

        # Toolbar row
        self._title_label = self.add_label("No file selected")
        self.add_button("Save", callback=self._save)
        self.add_button("Run", callback=self._run)

        # Editor row
        self.next_row()
        self._current_row.setSpacing(0)

        self._editor = CodeEditor(
            bg=self.panel_bg,
            fg=self.text_color,
            gutter_bg=self.widget_bg,
            accent=self.accent_color,
        )
        self._highlighter = None
        self._set_highlighter(".py")
        self._editor.setReadOnly(True)
        self._editor.textChanged.connect(self._on_text_changed)

        wrapper = QWidget()
        layout = QHBoxLayout(wrapper)
        layout.setContentsMargins(0, 0, 0, 0)
        layout.addWidget(self._editor)
        wrapper.setSizePolicy(QSizePolicy.Policy.Expanding, QSizePolicy.Policy.Expanding)
        self._current_row.addWidget(wrapper)

        self._root_layout.setStretchFactor(self._current_row, 1)

        # Ctrl+S shortcut
        save_shortcut = QShortcut(QKeySequence("Ctrl+S"), self)
        save_shortcut.activated.connect(self._save)

        # Subscribe to file selection from CreateFileList
        self._file_state = PanelStateManager.for_class(CreateFileList)
        self._file_state.subscribe("create_selected_file", self._on_selection_changed)

    def _set_highlighter(self, ext):
        if self._highlighter:
            self._highlighter.setDocument(None)
            self._highlighter = None
        if ext == ".cpp":
            self._highlighter = CppHighlighter(self._editor.document())
            self._editor.set_language("cpp")
        elif ext == ".py":
            self._highlighter = PythonHighlighter(self._editor.document())
            self._editor.set_language("python")
        else:
            self._editor.set_language("plain")

    def _on_selection_changed(self, selected):
        if self._dirty and self._current_file:
            self._save()

        if not selected:
            self._current_file = None
            self._editor.setReadOnly(True)
            self._editor.setPlainText("")
            self._title_label.setText("No file selected")
            self._dirty = False
            return

        filename = selected[0] if isinstance(selected, list) else selected
        filepath = CREATIONS_DIR / filename
        if not filepath.is_file():
            return

        try:
            text = filepath.read_text(encoding="utf-8")
        except (UnicodeDecodeError, OSError):
            return

        self._current_file = filename
        self._dirty = False
        self._title_label.setText(filename)
        self._set_highlighter(filepath.suffix)
        self._editor.blockSignals(True)
        self._editor.setPlainText(text)
        self._editor.setReadOnly(False)
        self._editor.blockSignals(False)

    def _on_text_changed(self):
        if self._current_file and not self._editor.isReadOnly():
            self._dirty = True
            if not self._title_label.text().endswith(" *"):
                self._title_label.setText(f"{self._current_file} *")

    def _save(self):
        if not self._current_file:
            return
        filepath = CREATIONS_DIR / self._current_file
        filepath.write_text(self._editor.toPlainText(), encoding="utf-8")
        self._dirty = False
        self._title_label.setText(self._current_file)

    def _run(self):
        if not self._current_file:
            return
        if self._dirty:
            self._save()

        filepath = CREATIONS_DIR / self._current_file
        try:
            from UI.Panel_Console import Console
            console_state = PanelStateManager.for_class(Console)
            import subprocess

            if filepath.suffix == ".cpp":
                out_bin = filepath.with_suffix("")
                comp = subprocess.run(
                    ["g++", "-std=c++17", "-o", str(out_bin), str(filepath)],
                    capture_output=True, text=True, timeout=30,
                    cwd=str(filepath.parent),
                )
                if comp.returncode != 0:
                    output = comp.stderr or "(compilation failed)\n"
                    current = console_state.get("console_text", "")
                    console_state.set("console_text", current + f"▶ g++ {self._current_file}\n{output}\n")
                    return
                result = subprocess.run(
                    [str(out_bin)],
                    capture_output=True, text=True, timeout=30,
                    cwd=str(filepath.parent),
                )
                run_label = f"./{filepath.stem}"
            elif filepath.suffix == ".py":
                result = subprocess.run(
                    ["python3", str(filepath)],
                    capture_output=True, text=True, timeout=30,
                    cwd=str(filepath.parent),
                )
                run_label = f"python3 {self._current_file}"
            else:
                current = console_state.get("console_text", "")
                console_state.set("console_text", current + f"▶ Cannot run {filepath.suffix} files\n")
                return

            output = ""
            if result.stdout:
                output += result.stdout
            if result.stderr:
                output += result.stderr
            if not output:
                output = "(no output)\n"

            current = console_state.get("console_text", "")
            console_state.set("console_text", current + f"▶ {run_label}\n{output}\n")
        except Exception:
            pass
