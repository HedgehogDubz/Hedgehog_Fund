from tabdock.tabs import StandardTab
from UI.Panel_Console import Console

def CreateTab(TabDock):
    analyze_tab = StandardTab(
        TabDock, "Create", 1,
        left_panels=[],
        center_panels=[],
        bottom_panels=[Console],
        right_panels=[],
        left_ratio=0.25,
        bottom_ratio=0.25,
        right_ratio=0.25
    )
    return analyze_tab