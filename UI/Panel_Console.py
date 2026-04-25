from pathlib import Path
from tabdock import Panel
from PyQt6.QtWidgets import QSizePolicy
from PyQt6.QtCore import Qt


CREATIONS_DIR = Path(__file__).resolve().parent.parent / "User_Creations"
HOG_DIR = Path(__file__).resolve().parent.parent / "Hog"
HOG_SOURCES = [
    str(HOG_DIR / "hog.cpp"),
    str(HOG_DIR / "compile.cpp"),
    str(HOG_DIR / "parse.cpp"),
    str(HOG_DIR / "typecheck.cpp"),
    str(HOG_DIR / "interpret.cpp"),
    str(HOG_DIR / "trade.cpp"),
]

class Console(Panel):
    def __init__(self, parent, docked, x, y, w, h, **kw):
        super().__init__(parent, docked, x, y, w, h, **kw)

        # Remove AlignTop so rows can fill vertically, collapse spacing for unified look
        self._root_layout.setAlignment(Qt.AlignmentFlag(0))
        self._root_layout.setSpacing(0)

        self.label_console = self.add_label("", state_key="console_text")
        self.label_console.setStyleSheet(f"background-color: {self.widget_bg};")
        self.label_console.setSizePolicy(QSizePolicy.Policy.Expanding, QSizePolicy.Policy.Expanding)
        self.label_console.setAlignment(Qt.AlignmentFlag.AlignBottom | Qt.AlignmentFlag.AlignLeft)

        # Stretch the console label row to fill all available space above the input
        self._root_layout.setStretchFactor(self._current_row, 1)

        self.next_row()
        self._current_row.setSpacing(0)
        self.label_text_indicator = self.add_label("% ")
        self.label_text_indicator.setStyleSheet(f"background-color: {self.widget_bg};")
        self.line_edit = self.add_text_input()
        self.line_edit.setStyleSheet(f"background-color: {self.widget_bg};")
        self.line_edit.returnPressed.connect(self._on_enter_pressed)

    def _on_enter_pressed(self):
        text = self.line_edit.text()
        self.interpret(text)
        self.line_edit.clear()

    def write(self, text):
        self.label_console.setText(self.label_console.text() + text)

    def clear(self):
        self.label_console.setText("")

    def set_text(self, text):
        self.label_console.setText(text)

    def interpret(self, text):
        self.write(f"▶ {text}\n")
        try:
            if (text.startswith("clear")):
                self.clear()

            elif (text.startswith("run ")):
                self.run(text[4:].strip())

            elif (text.startswith("hog build ")):
                self.run_hog("build", text[10:].strip())

            elif (text.startswith("hog run ")):
                self.run_hog("run", text[8:].strip())

            elif (text.startswith("hog ")):
                self.run_hog("run", text[4:].strip())

        except Exception as e:
            self.write(f"{type(e).__name__}: {e}\n")

    def _build_hog_compiler(self):
        """Compile the hog binary. Returns (success, error_output)."""
        import subprocess
        hog_bin = HOG_DIR / "hog"
        comp = subprocess.run(
            ["g++", "-std=c++17", "-o", str(hog_bin)] + HOG_SOURCES,
            capture_output=True, text=True, timeout=30,
            cwd=str(HOG_DIR),
        )
        if comp.returncode != 0:
            return False, comp.stderr or "(hog compiler build failed)\n"
        return True, ""

    def run_hog(self, command, filename):
        """Run a hog command (build, run, build-run) on a .hog file."""
        filepath = CREATIONS_DIR / filename
        if not filepath.exists():
            self.write(f"File not found: {filename}\n")
            return
        try:
            import subprocess
            ok, err = self._build_hog_compiler()
            if not ok:
                self.write(f"{err}\n")
                return
            hog_bin = HOG_DIR / "hog"
            result = subprocess.run(
                [str(hog_bin), command, str(filepath)],
                capture_output=True, text=True, timeout=30,
                cwd=str(filepath.parent),
            )
            output = ""
            if result.stdout:
                output += result.stdout
            if result.stderr:
                output += result.stderr
            if not output:
                output = "(no output)\n"
            self.write(f"{output}\n")
        except Exception:
            pass

    def run(self, filename):
        filepath = CREATIONS_DIR / filename
        try:
            import subprocess

            if filepath.suffix == ".cpp":
                out_bin = filepath.with_suffix("")
                comp = subprocess.run(
                    ["g++", "-std=c++17", "-o", str(out_bin), str(filepath)],
                    capture_output=True, text=True, timeout=30,
                    cwd=str(filepath.parent),
                )
                if comp.returncode != 0:
                    output = comp.stderr or "(compilation failed)\n"
                    self.write(f"▶ g++ {filename}\n{output}\n")
                    return
                result = subprocess.run(
                    [str(out_bin)],
                    capture_output=True, text=True, timeout=30,
                    cwd=str(filepath.parent),
                )
                run_label = f"./{filepath.stem}"
            elif filepath.suffix == ".py":
                result = subprocess.run(
                    ["python3", str(filepath)],
                    capture_output=True, text=True, timeout=30,
                    cwd=str(filepath.parent),
                )
                run_label = f"python3 {filename}"
            elif filepath.suffix == ".hog":
                self.run_hog("run", filename)
                return
            else:
                return

            output = ""
            if result.stdout:
                output += result.stdout
            if result.stderr:
                output += result.stderr
            if not output:
                output = "(no output)\n"

            self.write(f"▶ {run_label}\n{output}\n")
        except Exception:
            pass
