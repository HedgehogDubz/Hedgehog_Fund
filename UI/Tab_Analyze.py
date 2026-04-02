from tabdock import Tab, Dock, HConnector
from UI.Panel_AnalyzeChart import AnalyzeChart
from UI.Panel_AnalyzeDataFileList import AnalyzeDataFileList

def AnalyzeTab(TabDock):
    analyze_tab = Tab_Analyze(TabDock, "Analyze", 2)
    return analyze_tab

class Tab_Analyze(Tab):
    def initUI(self):
        self.left  = Dock(self, [AnalyzeDataFileList], 0,   0, 0.3, 1)
        self.right = Dock(self, [AnalyzeChart], 0.3, 0, 0.7, 1)

        self.add_dock(self.left)
        self.add_dock(self.right)

        self.add_connector(HConnector(self, 0.3, [self.left], [self.right]))

        self.left.raise_()
        self.right.raise_()