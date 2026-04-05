import shutil
from pathlib import Path
from PyQt6.QtWidgets import (
    QInputDialog, QTreeWidget, QTreeWidgetItem, QTreeWidgetItemIterator,
    QAbstractItemView, QSizePolicy, QMenu,
)
from PyQt6.QtCore import QFileSystemWatcher, Qt, QTimer, QPoint
from PyQt6.QtGui import QFont, QPainter, QColor, QPen, QAction
from tabdock import Panel
from tabdock._style_guide import lighten

CREATIONS_DIR = Path(__file__).resolve().parent.parent / "User_Creations"


class FileTree(QTreeWidget):
    """Tree widget with custom drag indicator and forced move semantics."""

    def __init__(self, parent=None):
        super().__init__(parent)
        self._drop_indicator_rect = None

    def dragEnterEvent(self, event):
        if event.source() is self:
            event.setDropAction(Qt.DropAction.MoveAction)
            event.accept()
        else:
            event.ignore()

    def dragMoveEvent(self, event):
        if event.source() is not self:
            event.ignore()
            return

        event.setDropAction(Qt.DropAction.MoveAction)

        # Let Qt compute the indicator position, then paint our own
        pos = event.position().toPoint()
        item = self.itemAt(pos)

        if item is None:
            # Over empty space — drop to root
            self._drop_indicator_rect = ("line", self.viewport().height() - 1, 0, self.viewport().width())
        else:
            rect = self.visualItemRect(item)
            margin = 4
            item_type = item.data(0, Qt.ItemDataRole.UserRole + 1)

            if item_type == "folder" and rect.top() + margin < pos.y() < rect.bottom() - margin:
                # Over the middle of a folder — highlight the whole row
                self._drop_indicator_rect = ("rect", rect)
            elif pos.y() < rect.center().y():
                # Above item — line above
                indent = self._get_item_indent(item)
                self._drop_indicator_rect = ("line", rect.top(), indent, self.viewport().width())
            else:
                # Below item — line below
                indent = self._get_item_indent(item)
                self._drop_indicator_rect = ("line", rect.bottom(), indent, self.viewport().width())

        self.viewport().update()
        event.accept()

    def _get_item_indent(self, item):
        depth = 0
        parent = item.parent()
        while parent is not None:
            depth += 1
            parent = parent.parent()
        return depth * self.indentation() + 4

    def dragLeaveEvent(self, event):
        self._drop_indicator_rect = None
        self.viewport().update()
        super().dragLeaveEvent(event)

    def dropEvent(self, event):
        self._drop_indicator_rect = None
        self.viewport().update()
        # The actual drop logic is handled by CreateFileList._on_drop
        # which is monkey-patched onto this method after construction.
        # This base version just clears the indicator.
        event.ignore()

    def paintEvent(self, event):
        super().paintEvent(event)
        if self._drop_indicator_rect is None:
            return

        painter = QPainter(self.viewport())
        painter.setRenderHint(QPainter.RenderHint.Antialiasing)

        accent = self.palette().highlight().color()

        kind = self._drop_indicator_rect[0]
        if kind == "line":
            _, y, x_start, x_end = self._drop_indicator_rect
            pen = QPen(accent, 2)
            painter.setPen(pen)
            painter.drawLine(x_start, y, x_end, y)
            # Small circle on the left end
            painter.setBrush(accent)
            painter.drawEllipse(QPoint(x_start + 3, y), 3, 3)
        elif kind == "rect":
            _, rect = self._drop_indicator_rect
            pen = QPen(accent, 1)
            painter.setPen(pen)
            fill = QColor(accent)
            fill.setAlpha(30)
            painter.setBrush(fill)
            painter.drawRoundedRect(rect, 3, 3)

        painter.end()


class CreateFileList(Panel):
    def __init__(self, parent, docked, x, y, w, h, **kw):
        super().__init__(parent, docked, x, y, w, h, **kw)

        CREATIONS_DIR.mkdir(parents=True, exist_ok=True)

        self._list_key = "create_selected_file"
        self._expanded = set()
        self._selected_rel = None
        self._suppress_watcher = False

        self._root_layout.setContentsMargins(4, 4, 4, 4)

        # Row 1: file type buttons
        self.add_button("+ .py", callback=self._on_new_py)
        self.add_button("+ .cpp", callback=self._on_new_cpp)
        self.add_button("+ .hog", callback=self._on_new_hog)

        self.next_row()
        # Row 2: generic + folder + actions
        self.add_button("+ File", callback=self._on_new_file)
        self.add_button("+ Folder", callback=self._on_new_folder)

        self.next_row()

        # Tree
        self._tree = FileTree(self)
        self._tree.setHeaderHidden(True)
        self._tree.setColumnCount(1)
        self._tree.setIndentation(16)
        self._tree.setRootIsDecorated(False)
        self._tree.setAnimated(False)
        self._tree.setFont(QFont("Menlo", 12))
        self._tree.setSizePolicy(QSizePolicy.Policy.Expanding, QSizePolicy.Policy.Expanding)

        # Drag and drop
        self._tree.setDragEnabled(True)
        self._tree.setAcceptDrops(True)
        self._tree.setDragDropMode(QAbstractItemView.DragDropMode.DragDrop)
        self._tree.setDefaultDropAction(Qt.DropAction.MoveAction)
        self._tree.setDropIndicatorShown(False)  # We draw our own

        # Wire up the real drop handler
        self._tree.dropEvent = self._on_drop

        # Click = select, double-click = toggle folder, right-click = context menu
        self._tree.setSelectionMode(QAbstractItemView.SelectionMode.SingleSelection)
        self._tree.itemClicked.connect(self._on_item_clicked)
        self._tree.itemDoubleClicked.connect(self._on_item_double_clicked)
        self._tree.setContextMenuPolicy(Qt.ContextMenuPolicy.CustomContextMenu)
        self._tree.customContextMenuRequested.connect(self._on_context_menu)

        _h = lighten(self.panel_bg, 0.25)
        _hh = lighten(self.panel_bg, 0.35)
        self._tree.setStyleSheet(f"""
            QTreeWidget {{
                background-color: {self.widget_bg};
                color: {self.text_color};
                border: none;
                border-radius: 4px;
                padding: 2px 0px;
                font-size: 12px;
                outline: none;
            }}
            QTreeWidget::item {{
                padding: 2px 6px;
                border: none;
                border-radius: 0px;
            }}
            QTreeWidget::item:selected {{
                background-color: {lighten(self.widget_bg, 0.15)};
                color: {self.text_color};
            }}
            QTreeWidget::item:hover:!selected {{
                background-color: {lighten(self.widget_bg, 0.08)};
            }}
            QTreeWidget::branch {{
                background: transparent;
                border: none;
                image: none;
            }}
            QScrollBar:vertical {{
                background: transparent;
                width: 8px;
                margin: 0;
            }}
            QScrollBar::handle:vertical {{
                background: {_h};
                border-radius: 4px;
                min-height: 20px;
            }}
            QScrollBar::handle:vertical:hover {{
                background: {_hh};
            }}
            QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical {{
                height: 0px;
            }}
            QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical {{
                background: none;
            }}
        """)

        self._root_layout.addWidget(self._tree)
        self._current_row = self._make_row()

        # Watcher
        self._watcher = QFileSystemWatcher()
        self._watch_recursive(CREATIONS_DIR)
        self._watcher.directoryChanged.connect(self._on_dir_changed)

        self._refresh_timer = QTimer(self)
        self._refresh_timer.setSingleShot(True)
        self._refresh_timer.setInterval(100)
        self._refresh_timer.timeout.connect(self._rebuild_tree)

        self._rebuild_tree()

    # ── Filesystem watcher ───────────────────────────────────────────

    def _watch_recursive(self, path):
        self._watcher.addPath(str(path))
        for p in path.rglob("*"):
            if p.is_dir():
                self._watcher.addPath(str(p))

    def _on_dir_changed(self, _path):
        if self._suppress_watcher:
            return
        self._refresh_timer.start()

    # ── Tree building ────────────────────────────────────────────────

    def _rebuild_tree(self):
        self._tree.blockSignals(True)
        self._tree.clear()
        self._build_children(self._tree, CREATIONS_DIR, Path(""))
        self._tree.blockSignals(False)
        self._watch_recursive(CREATIONS_DIR)

        if self._selected_rel is not None:
            self._select_by_rel(self._selected_rel)

    def _build_children(self, parent_widget, abs_path, rel_path):
        try:
            entries = sorted(abs_path.iterdir(), key=lambda p: p.name.lower())
        except OSError:
            return

        folders = [p for p in entries if p.is_dir()]
        files = [p for p in entries if p.is_file()]

        for folder in folders:
            folder_rel = str(rel_path / folder.name)
            is_expanded = folder_rel in self._expanded
            arrow = "\u25BC" if is_expanded else "\u25B6"
            item = QTreeWidgetItem(parent_widget, [f"{arrow}  {folder.name}"])
            item.setData(0, Qt.ItemDataRole.UserRole, folder_rel)
            item.setData(0, Qt.ItemDataRole.UserRole + 1, "folder")
            item.setFlags(
                Qt.ItemFlag.ItemIsEnabled
                | Qt.ItemFlag.ItemIsSelectable
                | Qt.ItemFlag.ItemIsDragEnabled
                | Qt.ItemFlag.ItemIsDropEnabled
            )
            if is_expanded:
                self._build_children(item, folder, rel_path / folder.name)
                item.setExpanded(True)

        for f in files:
            file_rel = str(rel_path / f.name)
            item = QTreeWidgetItem(parent_widget, [f.name])
            item.setData(0, Qt.ItemDataRole.UserRole, file_rel)
            item.setData(0, Qt.ItemDataRole.UserRole + 1, "file")
            item.setFlags(
                Qt.ItemFlag.ItemIsEnabled
                | Qt.ItemFlag.ItemIsSelectable
                | Qt.ItemFlag.ItemIsDragEnabled
            )

    def _select_by_rel(self, rel_path):
        it = QTreeWidgetItemIterator(self._tree)
        while it.value():
            if it.value().data(0, Qt.ItemDataRole.UserRole) == rel_path:
                self._tree.setCurrentItem(it.value())
                return
            it += 1

    # ── Click handlers ───────────────────────────────────────────────

    def _on_item_clicked(self, item, _col):
        item_type = item.data(0, Qt.ItemDataRole.UserRole + 1)
        rel_path = item.data(0, Qt.ItemDataRole.UserRole)
        self._selected_rel = rel_path

        if item_type == "file":
            self.state.set(self._list_key, [rel_path])
        elif item_type == "folder":
            # Single click on folder: toggle expand
            if rel_path in self._expanded:
                self._expanded.discard(rel_path)
            else:
                self._expanded.add(rel_path)
            self._rebuild_tree()

    def _on_item_double_clicked(self, item, _col):
        item_type = item.data(0, Qt.ItemDataRole.UserRole + 1)
        if item_type == "file":
            rel_path = item.data(0, Qt.ItemDataRole.UserRole)
            self.state.set(self._list_key, [rel_path])

    def _on_context_menu(self, pos):
        item = self._tree.itemAt(pos)
        if item is None:
            # Right-click on empty space
            menu = QMenu(self)
            menu.setStyleSheet(self._menu_stylesheet())
            menu.addAction("New .py", self._on_new_py)
            menu.addAction("New .cpp", self._on_new_cpp)
            menu.addAction("New .hog", self._on_new_hog)
            menu.addAction("New File", self._on_new_file)
            menu.addSeparator()
            menu.addAction("New Folder", self._on_new_folder)
            menu.exec(self._tree.viewport().mapToGlobal(pos))
            return

        # Select the right-clicked item
        self._tree.setCurrentItem(item)
        rel_path = item.data(0, Qt.ItemDataRole.UserRole)
        item_type = item.data(0, Qt.ItemDataRole.UserRole + 1)
        self._selected_rel = rel_path

        menu = QMenu(self)
        menu.setStyleSheet(self._menu_stylesheet())

        if item_type == "folder":
            menu.addAction("New .py", self._on_new_py)
            menu.addAction("New .cpp", self._on_new_cpp)
            menu.addAction("New .hog", self._on_new_hog)
            menu.addAction("New File", self._on_new_file)
            menu.addAction("New Folder", self._on_new_folder)
            menu.addSeparator()

        menu.addAction("Rename", self._on_rename)

        delete_action = QAction("Delete", self)
        delete_action.triggered.connect(self._on_delete)
        menu.addAction(delete_action)

        menu.exec(self._tree.viewport().mapToGlobal(pos))

    def _menu_stylesheet(self):
        return f"""
            QMenu {{
                background-color: {self.widget_bg};
                color: {self.text_color};
                border: 1px solid {lighten(self.widget_bg, 0.2)};
                border-radius: 4px;
                padding: 4px 0px;
                font-size: 12px;
            }}
            QMenu::item {{
                padding: 5px 20px;
            }}
            QMenu::item:selected {{
                background-color: {lighten(self.widget_bg, 0.15)};
            }}
            QMenu::separator {{
                height: 1px;
                background: {lighten(self.widget_bg, 0.2)};
                margin: 4px 8px;
            }}
        """

    # ── Drag and drop ────────────────────────────────────────────────

    def _on_drop(self, event):
        self._tree._drop_indicator_rect = None
        self._tree.viewport().update()

        source_item = self._tree.currentItem()
        if not source_item:
            event.ignore()
            return

        source_rel = source_item.data(0, Qt.ItemDataRole.UserRole)
        source_type = source_item.data(0, Qt.ItemDataRole.UserRole + 1)
        source_path = CREATIONS_DIR / source_rel

        if not source_path.exists():
            event.ignore()
            return

        # Resolve destination directory
        pos = event.position().toPoint()
        target_item = self._tree.itemAt(pos)

        if target_item is None:
            dest_dir = CREATIONS_DIR
        else:
            target_type = target_item.data(0, Qt.ItemDataRole.UserRole + 1)
            target_rel = target_item.data(0, Qt.ItemDataRole.UserRole)
            rect = self._tree.visualItemRect(target_item)
            margin = 4

            if target_type == "folder" and rect.top() + margin < pos.y() < rect.bottom() - margin:
                # Dropped ON a folder — move inside it
                dest_dir = CREATIONS_DIR / target_rel
            else:
                # Dropped between items — place in the target's parent
                parent_item = target_item.parent()
                if parent_item is not None:
                    dest_dir = CREATIONS_DIR / parent_item.data(0, Qt.ItemDataRole.UserRole)
                else:
                    dest_dir = CREATIONS_DIR

        # Guard: don't drop a folder into itself or its children
        if source_type == "folder":
            try:
                dest_rel = str(dest_dir.relative_to(CREATIONS_DIR))
            except ValueError:
                event.ignore()
                return
            if dest_rel == source_rel or dest_rel.startswith(source_rel + "/"):
                event.ignore()
                return

        dest_path = dest_dir / source_path.name

        if dest_path == source_path or dest_path.exists():
            event.ignore()
            return

        # Do the move — suppress watcher to avoid double rebuild
        self._suppress_watcher = True
        source_path.rename(dest_path)
        self._suppress_watcher = False
        new_rel = str(dest_path.relative_to(CREATIONS_DIR))

        # Update bookkeeping
        if source_type == "folder":
            updated = set()
            for e in self._expanded:
                if e == source_rel:
                    updated.add(new_rel)
                elif e.startswith(source_rel + "/"):
                    updated.add(new_rel + e[len(source_rel):])
                else:
                    updated.add(e)
            self._expanded = updated
            self._selected_rel = new_rel
            self.state.set(self._list_key, [])
        else:
            self._selected_rel = new_rel
            self.state.set(self._list_key, [new_rel])

        # Expand the target folder so the moved item is visible
        if target_item is not None:
            target_type = target_item.data(0, Qt.ItemDataRole.UserRole + 1)
            if target_type == "folder":
                rect = self._tree.visualItemRect(target_item)
                if rect.top() + 4 < pos.y() < rect.bottom() - 4:
                    self._expanded.add(target_item.data(0, Qt.ItemDataRole.UserRole))

        event.setDropAction(Qt.DropAction.MoveAction)
        event.accept()
        self._rebuild_tree()

    # ── Helpers ──────────────────────────────────────────────────────

    def _get_selected(self):
        items = self._tree.selectedItems()
        if not items:
            return None
        item = items[0]
        rel_path = item.data(0, Qt.ItemDataRole.UserRole)
        item_type = item.data(0, Qt.ItemDataRole.UserRole + 1)
        return rel_path, item_type, (CREATIONS_DIR / rel_path).parent

    def _get_context_dir(self):
        sel = self._get_selected()
        if sel is None:
            return CREATIONS_DIR
        rel_path, item_type, parent_dir = sel
        if item_type == "folder":
            self._expanded.add(rel_path)
            return CREATIONS_DIR / rel_path
        return parent_dir

    def _create_file(self, name):
        target_dir = self._get_context_dir()
        filepath = target_dir / name
        if not filepath.exists():
            filepath.write_text("", encoding="utf-8")
        rel = str(filepath.relative_to(CREATIONS_DIR))
        self._selected_rel = rel
        self.state.set(self._list_key, [rel])
        self._rebuild_tree()

    # ── Actions ──────────────────────────────────────────────────────

    def _on_new_py(self):
        name, ok = QInputDialog.getText(self, "New Python File", "File name:")
        if not ok or not name:
            return
        if not name.endswith(".py"):
            name += ".py"
        self._create_file(name)

    def _on_new_cpp(self):
        name, ok = QInputDialog.getText(self, "New C++ File", "File name:")
        if not ok or not name:
            return
        if not name.endswith(".cpp"):
            name += ".cpp"
        self._create_file(name)

    def _on_new_hog(self):
        name, ok = QInputDialog.getText(self, "New Hog File", "File name:")
        if not ok or not name:
            return
        if not name.endswith(".hog"):
            name += ".hog"
        self._create_file(name)

    def _on_new_file(self):
        name, ok = QInputDialog.getText(self, "New File", "File name (with extension):")
        if not ok or not name:
            return
        self._create_file(name)

    def _on_new_folder(self):
        name, ok = QInputDialog.getText(self, "New Folder", "Folder name:")
        if not ok or not name:
            return
        target_dir = self._get_context_dir()
        folder_path = target_dir / name
        folder_path.mkdir(parents=True, exist_ok=True)
        rel = str(folder_path.relative_to(CREATIONS_DIR))
        self._expanded.add(rel)
        self._selected_rel = rel
        self._rebuild_tree()

    def _on_rename(self):
        sel = self._get_selected()
        if sel is None:
            return
        rel_path, item_type, parent_dir = sel
        abs_path = CREATIONS_DIR / rel_path
        old_name = abs_path.name

        new_name, ok = QInputDialog.getText(self, "Rename", "New name:", text=old_name)
        if not ok or not new_name or new_name == old_name:
            return

        new_path = parent_dir / new_name
        if abs_path.exists() and not new_path.exists():
            self._suppress_watcher = True
            abs_path.rename(new_path)
            self._suppress_watcher = False
            new_rel = str(new_path.relative_to(CREATIONS_DIR))

            if item_type == "folder":
                updated = set()
                for e in self._expanded:
                    if e == rel_path:
                        updated.add(new_rel)
                    elif e.startswith(rel_path + "/"):
                        updated.add(new_rel + e[len(rel_path):])
                    else:
                        updated.add(e)
                self._expanded = updated
                self._selected_rel = new_rel
                self.state.set(self._list_key, [])
            else:
                self._selected_rel = new_rel
                self.state.set(self._list_key, [new_rel])
            self._rebuild_tree()

    def _on_delete(self):
        sel = self._get_selected()
        if sel is None:
            return
        rel_path, item_type, _parent = sel
        target = CREATIONS_DIR / rel_path

        self._suppress_watcher = True
        if target.is_dir():
            shutil.rmtree(target)
            self._expanded = {
                e for e in self._expanded
                if e != rel_path and not e.startswith(rel_path + "/")
            }
        elif target.is_file():
            target.unlink()
        self._suppress_watcher = False

        self._selected_rel = None
        self.state.set(self._list_key, [])
        self._rebuild_tree()
