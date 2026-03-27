from PyQt6.QtWidgets import QApplication, QMainWindow
from tabdock import TabDock, apply_theme
from UI.Panel_Retrieve import Retrieve
from UI.Panel_DataFileList import DataFileList
from UI.Panel_TablePreview import TablePreview
from UI.Panel_ChartPreview import ChartPreview
from UI.Panel_Simulate import Simulate
from UI.Panel_Console import Console
from UI.Tab_Analyze import AnalyzeTab
from UI.Tab_Create import CreateTab
from UI.Tab_Retrieve import RetrieveTab

class MainUI(QMainWindow):
    def __init__(self):
        super().__init__()
        self.setWindowTitle("HedgehogFund")
        self.resize(1200, 800)

        theme_kwargs = apply_theme("monokai")
        all_panels = [Retrieve, DataFileList, TablePreview, ChartPreview, Simulate, Console]
        self.tab_dock = TabDock(available_panels=all_panels, **theme_kwargs)
        self.setCentralWidget(self.tab_dock)

        retrieve_tab = RetrieveTab(self.tab_dock)
        create_tab = CreateTab(self.tab_dock)
        analyze_tab = AnalyzeTab(self.tab_dock)

        self.tab_dock.add_tab(retrieve_tab)
        self.tab_dock.add_tab(create_tab)
        self.tab_dock.add_tab(analyze_tab)

        self.tab_dock.connector_manager._enable_tracking_on_children()

        self.show()


def create_main_UI():
    app = QApplication([])
    window = MainUI()
    app.exec()
