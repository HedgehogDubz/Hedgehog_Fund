from tabdock.tabs import QuadTab
from UI.Panel_DataFileList import DataFileList
from UI.Panel_ChartPreview import ChartPreview


def AnalyzeTab(TabDock):
    analyze_tab = QuadTab(
        TabDock, "Analyze", 2,
        top_left_panels=[DataFileList],
        top_right_panels=[],
        bottom_left_panels=[],
        bottom_right_panels=[],
        h_split=0.5, v_split=0.5
    )
    return analyze_tab