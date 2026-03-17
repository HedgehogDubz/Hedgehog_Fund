from tabdock.tabs import EditorTab
from UI.Panel_Retrieve import Retrieve
from UI.Panel_DataFileList import DataFileList
from UI.Panel_TablePreview import TablePreview
from UI.Panel_ChartPreview import ChartPreview


def RetrieveTab(TabDock):
    retrieve_tab = EditorTab(
            TabDock, "Retrieve", 0,
            left_panels=[Retrieve],
            main_panels=[DataFileList],
            bottom_panels=[ChartPreview, TablePreview],
            left_ratio=0.25,
            bottom_ratio=0.70,
        )
    return retrieve_tab