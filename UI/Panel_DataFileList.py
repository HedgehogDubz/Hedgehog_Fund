from PyQt6.QtWidgets import QInputDialog
from PyQt6.QtCore import QFileSystemWatcher
from tabdock import Panel
from retrieve_data import DATA_DIR, list_parquet_files, delete_parquet_file, rename_parquet_file


class DataFileList(Panel):
    def __init__(self, list_key, parent, docked, x, y, w, h, **kw):
        super().__init__(parent, docked, x, y, w, h, **kw)

        self.add_button("Rename", callback=self._on_rename)
        self.add_button("Delete", callback=self._on_delete)

        self.next_row()
        self._file_list = self.add_list(
            list_parquet_files(),
            list_key=list_key,
            callback=self._on_file_selected,
        )

        self._watcher = QFileSystemWatcher()
        DATA_DIR.mkdir(parents=True, exist_ok=True)
        self._watcher.addPath(str(DATA_DIR))
        self._watcher.directoryChanged.connect(self._refresh_files)

    def _refresh_files(self):
        files = list_parquet_files()
        current_sel = self.state.get(self.list_key, [])
        self._file_list.clear()
        self._file_list.addItems(files)
        for i in range(self._file_list.count()):
            if self._file_list.item(i).text() in current_sel:
                self._file_list.item(i).setSelected(True)

    def _on_file_selected(self, selected):
        pass

    def _on_delete(self):
        selected = self.state.get(self._file_list.list_key, [])
        if not selected:
            return
        for f in selected:
            delete_parquet_file(f)
        self.state.set(self._file_list.list_key, [])
        self._refresh_files()

    def _on_rename(self):
        selected = self.state.get(self._file_list.list_key, [])
        if not selected:
            return
        old_name = selected[0]
        new_name, ok = QInputDialog.getText(self, "Rename", "New name:", text=old_name)
        if ok and new_name and new_name != old_name:
            if rename_parquet_file(old_name, new_name):
                if not new_name.endswith(".parquet"):
                    new_name += ".parquet"
                self.state.set(self._file_list.list_key, [new_name])
                self._refresh_files()
