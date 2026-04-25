from tabdock import Tab, Dock, HConnector,VConnector
from UI.Panel_AnalyzeChart import AnalyzeChart
from UI.Panel_AnalyzeDataFileList import AnalyzeDataFileList
from UI.Panel_AlgorithmTradeList import AlgorithmTradeList

def AnalyzeTab(TabDock):
    analyze_tab = Tab_Analyze(TabDock, "Analyze", 2)
    return analyze_tab

class Tab_Analyze(Tab):
    def initUI(self):
        self.left1  = Dock(self, [AnalyzeDataFileList], 0,   0, 0.3, 0.5)
        self.left2  = Dock(self, [AlgorithmTradeList], 0,   0.5, 0.3, 0.5)
        self.right = Dock(self, [AnalyzeChart], 0.3, 0, 0.7, 1)

        self.add_dock(self.left1)
        self.add_dock(self.left2)
        self.add_dock(self.right)

        self.add_connector(VConnector(self, 0.5, [self.left1], [self.left2]))
        self.add_connector(HConnector(self, 0.3, [self.left1, self.left2], [self.right]))

        self.left1.raise_()
        self.left2.raise_()
        self.right.raise_()