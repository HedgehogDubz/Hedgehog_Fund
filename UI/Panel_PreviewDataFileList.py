from UI.Panel_DataFileList import DataFileList


class PreviewDataFileList(DataFileList):
    def __init__(self, parent, docked, x, y, w, h, **kw):
        super().__init__("preview_selected_file", parent, docked, x, y, w, h, **kw)