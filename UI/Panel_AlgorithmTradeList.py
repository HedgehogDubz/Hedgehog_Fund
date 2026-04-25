from pathlib import Path
from PyQt6.QtCore import QFileSystemWatcher
from tabdock import Panel

HOG_DIR = Path(__file__).resolve().parent.parent / "Hog"
CREATIONS_DIR = Path(__file__).resolve().parent.parent / "User_Creations"


class AlgorithmTradeList(Panel):
    def __init__(self, parent, docked, x, y, w, h, **kw):
        super().__init__(parent, docked, x, y, w, h, **kw)

        self.add_button("Rename", callback=self._on_rename)
        self.add_button("Delete", callback=self._on_delete)

        self.next_row()
        self._file_list = self.add_list(
            self._get_trades(),
            list_key="algorithm_trade_selected",
            callback=self._on_file_selected,
        )

        self._watcher = QFileSystemWatcher()
        CREATIONS_DIR.mkdir(parents=True, exist_ok=True)
        self._watcher.addPath(str(CREATIONS_DIR))
        self._watcher.directoryChanged.connect(self._refresh_trades)

    def _get_trades(self):
        """Call the hog binary to list all trade block names."""
        import subprocess
        hog_bin = HOG_DIR / "hog"
        if not hog_bin.exists():
            return []
        try:
            result = subprocess.run(
                [str(hog_bin), "trades"],
                capture_output=True, text=True, timeout=10,
                cwd=str(HOG_DIR.parent),
            )
            if result.returncode != 0:
                return []
            return [name for name in result.stdout.strip().splitlines() if name]
        except Exception:
            return []

    def _refresh_trades(self):
        trades = self._get_trades()
        self._file_list.clear()
        self._file_list.addItems(trades)

    def _on_file_selected(self, selected):
        pass

    def _on_rename(self):
        pass

    def _on_delete(self):
        pass
