import re
from PyQt6.QtGui import QFont, QColor, QTextCharFormat, QSyntaxHighlighter


class HogHighlighter(QSyntaxHighlighter):
    def __init__(self, parent=None):
        super().__init__(parent)
        self._rules = []

        # Keywords (control flow, modifiers, constructs)
        kw_fmt = QTextCharFormat()
        kw_fmt.setForeground(QColor("#f92672"))
        kw_fmt.setFontWeight(QFont.Weight.Bold)
        keywords = [
            "if", "else", "for", "while", "break", "continue", "return",
            "class", "struct", "enum", "new",
            "import", "from", "export",
            "trade", "metric",
            "public", "private", "protected",
            "const", "auto", "void",
        ]
        for w in keywords:
            self._rules.append((re.compile(rf"\b{w}\b"), kw_fmt))

        # Types
        type_fmt = QTextCharFormat()
        type_fmt.setForeground(QColor("#66d9ef"))
        type_fmt.setFontItalic(True)
        types = [
            "int", "long", "float", "double", "bool", "char", "string",
            "Timestamp", "Tuple", "List", "Array", "Set", "Map",
        ]
        for w in types:
            self._rules.append((re.compile(rf"\b{w}\b"), type_fmt))

        # Built-in functions
        builtin_fmt = QTextCharFormat()
        builtin_fmt.setForeground(QColor("#66d9ef"))
        builtins = [
            "print", "time", "chrono",
            "signal_int", "signal_string", "signal_bool",
        ]
        for w in builtins:
            self._rules.append((re.compile(rf"\b{w}\b"), builtin_fmt))

        # Trading actions
        trade_fmt = QTextCharFormat()
        trade_fmt.setForeground(QColor("#a6e22e"))
        trade_fmt.setFontWeight(QFont.Weight.Bold)
        for w in ["buy", "sell"]:
            self._rules.append((re.compile(rf"\b{w}\b"), trade_fmt))

        # Boolean literals
        bool_fmt = QTextCharFormat()
        bool_fmt.setForeground(QColor("#ae81ff"))
        for w in ["true", "false"]:
            self._rules.append((re.compile(rf"\b{w}\b"), bool_fmt))

        # Numbers
        num_fmt = QTextCharFormat()
        num_fmt.setForeground(QColor("#ae81ff"))
        self._rules.append((re.compile(r"\b\d+(\.\d+)?\b"), num_fmt))

        # Strings
        str_fmt = QTextCharFormat()
        str_fmt.setForeground(QColor("#e6db74"))
        self._rules.append((re.compile(r'"[^"\\]*(\\.[^"\\]*)*"'), str_fmt))

        # Char literals
        self._rules.append((re.compile(r"'[^'\\]*(\\.[^'\\]*)*'"), str_fmt))

        # Function / class names after keyword
        func_fmt = QTextCharFormat()
        func_fmt.setForeground(QColor("#a6e22e"))
        self._rules.append((re.compile(r"(?<=\bclass\s)\w+"), func_fmt))
        self._rules.append((re.compile(r"(?<=\bstruct\s)\w+"), func_fmt))
        self._rules.append((re.compile(r"(?<=\benum\s)\w+"), func_fmt))

        # Function calls (identifier followed by open paren)
        call_fmt = QTextCharFormat()
        call_fmt.setForeground(QColor("#a6e22e"))
        self._rules.append((re.compile(r"\b([a-zA-Z_]\w*)\s*(?=\()"), call_fmt))

        # Comments (single-line)
        comment_fmt = QTextCharFormat()
        comment_fmt.setForeground(QColor("#75715e"))
        comment_fmt.setFontItalic(True)
        self._rules.append((re.compile(r"//[^\n]*"), comment_fmt))

        # Multi-line comments stored for block handling
        self._comment_fmt = comment_fmt
        self._comment_start = re.compile(r"/\*")
        self._comment_end = re.compile(r"\*/")

    def highlightBlock(self, text):
        # Single-line rules
        for pattern, fmt in self._rules:
            for m in pattern.finditer(text):
                self.setFormat(m.start(), m.end() - m.start(), fmt)

        # Multi-line comment handling
        self.setCurrentBlockState(0)
        start = 0
        if self.previousBlockState() != 1:
            match = self._comment_start.search(text)
            start = match.start() if match else -1
        while start >= 0:
            end_match = self._comment_end.search(text, start)
            if end_match:
                length = end_match.end() - start
                self.setFormat(start, length, self._comment_fmt)
                match = self._comment_start.search(text, end_match.end())
                start = match.start() if match else -1
            else:
                self.setCurrentBlockState(1)
                self.setFormat(start, len(text) - start, self._comment_fmt)
                break
