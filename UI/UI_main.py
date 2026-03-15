from PyQt6.QtWidgets import QApplication, QMainWindow
from tabdock import TabDock, apply_theme
from tabdock.tabs import StandardTab
from UI.UI_retrieve import RetrievePanel, FileListPanel, PreviewPanel


class MainUI(QMainWindow):
    def __init__(self):
        super().__init__()
        self.setWindowTitle("HedgehogFund")
        self.resize(1200, 800)

        theme_kwargs = apply_theme("monokai")
        all_panels = [RetrievePanel, FileListPanel, PreviewPanel]
        self.tab_dock = TabDock(available_panels=all_panels, **theme_kwargs)
        self.setCentralWidget(self.tab_dock)

        retrieve_tab = StandardTab(
            self.tab_dock, "Retrieve", 0,
            left_panels=[RetrievePanel],
            center_panels=[FileListPanel],
            right_panels=[],
            bottom_panels=[PreviewPanel],
            left_ratio=0.25,
            right_ratio=0.0,
            bottom_ratio=0.55,
        )
        self.tab_dock.add_tab(retrieve_tab)
        self.tab_dock.connector_manager._enable_tracking_on_children()

        self.show()


def create_main_UI():
    app = QApplication([])
    window = MainUI()
    app.exec()
