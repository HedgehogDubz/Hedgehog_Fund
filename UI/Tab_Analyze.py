from tabdock.tabs import StandardTab


def AnalyzeTab(TabDock):
    analyze_tab = StandardTab(
        TabDock, "Analyze", 1,
        left_panels=[],
        bottom_panels=[],
        center_panels=[],
        right_panels=[],
        left_ratio=0.25,
        bottom_ratio=0.25,
        right_ratio=0.25
    )
    return analyze_tab