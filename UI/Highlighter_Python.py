import re
from PyQt6.QtGui import QFont, QColor, QTextCharFormat, QSyntaxHighlighter


class PythonHighlighter(QSyntaxHighlighter):
    def __init__(self, parent=None):
        super().__init__(parent)
        self._rules = []

        # Keywords
        kw_fmt = QTextCharFormat()
        kw_fmt.setForeground(QColor("#f92672"))
        kw_fmt.setFontWeight(QFont.Weight.Bold)
        keywords = [
            "False", "None", "True", "and", "as", "assert", "async", "await",
            "break", "class", "continue", "def", "del", "elif", "else",
            "except", "finally", "for", "from", "global", "if", "import",
            "in", "is", "lambda", "nonlocal", "not", "or", "pass", "raise",
            "return", "try", "while", "with", "yield",
        ]
        for w in keywords:
            self._rules.append((re.compile(rf"\b{w}\b"), kw_fmt))

        # Built-in functions
        builtin_fmt = QTextCharFormat()
        builtin_fmt.setForeground(QColor("#66d9ef"))
        builtins = [
            "print", "range", "len", "int", "float", "str", "list", "dict",
            "set", "tuple", "bool", "type", "isinstance", "hasattr", "getattr",
            "setattr", "open", "super", "enumerate", "zip", "map", "filter",
            "sorted", "reversed", "abs", "max", "min", "sum", "any", "all",
            "input", "format", "repr", "id", "hex", "oct", "bin", "round",
        ]
        for w in builtins:
            self._rules.append((re.compile(rf"\b{w}\b"), builtin_fmt))

        # Decorators
        deco_fmt = QTextCharFormat()
        deco_fmt.setForeground(QColor("#a6e22e"))
        self._rules.append((re.compile(r"@\w+"), deco_fmt))

        # self / cls
        self_fmt = QTextCharFormat()
        self_fmt.setForeground(QColor("#fd971f"))
        self_fmt.setFontItalic(True)
        self._rules.append((re.compile(r"\bself\b"), self_fmt))
        self._rules.append((re.compile(r"\bcls\b"), self_fmt))

        # Numbers
        num_fmt = QTextCharFormat()
        num_fmt.setForeground(QColor("#ae81ff"))
        self._rules.append((re.compile(r"\b\d+(\.\d+)?\b"), num_fmt))

        # Strings (single and double quoted)
        str_fmt = QTextCharFormat()
        str_fmt.setForeground(QColor("#e6db74"))
        self._rules.append((re.compile(r'"[^"\\]*(\\.[^"\\]*)*"'), str_fmt))
        self._rules.append((re.compile(r"'[^'\\]*(\\.[^'\\]*)*'"), str_fmt))

        # f-string prefix
        self._rules.append((re.compile(r'\bf"[^"\\]*(\\.[^"\\]*)*"'), str_fmt))
        self._rules.append((re.compile(r"\bf'[^'\\]*(\\.[^'\\]*)*'"), str_fmt))

        # Comments
        comment_fmt = QTextCharFormat()
        comment_fmt.setForeground(QColor("#75715e"))
        comment_fmt.setFontItalic(True)
        self._rules.append((re.compile(r"#[^\n]*"), comment_fmt))

        # Function / class names after def/class
        func_fmt = QTextCharFormat()
        func_fmt.setForeground(QColor("#a6e22e"))
        self._rules.append((re.compile(r"(?<=\bdef\s)\w+"), func_fmt))
        self._rules.append((re.compile(r"(?<=\bclass\s)\w+"), func_fmt))

    def highlightBlock(self, text):
        for pattern, fmt in self._rules:
            for m in pattern.finditer(text):
                self.setFormat(m.start(), m.end() - m.start(), fmt)
