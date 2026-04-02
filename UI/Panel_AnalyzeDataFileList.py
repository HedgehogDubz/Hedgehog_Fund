from UI.Panel_DataFileList import DataFileList


class AnalyzeDataFileList(DataFileList):
    def __init__(self, parent, docked, x, y, w, h, **kw):
        super().__init__("analyze_selected_file", parent, docked, x, y, w, h, **kw)