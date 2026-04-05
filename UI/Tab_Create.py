from tabdock import Tab, Dock, HConnector, VConnector
from UI.Panel_CreateFileList import CreateFileList
from UI.Panel_IDE import IDE
from UI.Panel_Console import Console


def CreateTab(TabDock):
    return Tab_Create(TabDock, "Create", 1)


class Tab_Create(Tab):
    def initUI(self):
        self.left = Dock(self, [CreateFileList], 0, 0, 0.2, 1)
        self.top_right = Dock(self, [IDE], 0.2, 0, 0.8, 0.75)
        self.bottom_right = Dock(self, [Console], 0.2, 0.75, 0.8, 0.25)

        self.add_dock(self.left)
        self.add_dock(self.top_right)
        self.add_dock(self.bottom_right)

        self.add_connector(HConnector(self, 0.2, [self.left], [self.top_right, self.bottom_right]))
        self.add_connector(VConnector(self, 0.75, [self.top_right], [self.bottom_right]))

        self.left.raise_()
        self.top_right.raise_()
        self.bottom_right.raise_()
