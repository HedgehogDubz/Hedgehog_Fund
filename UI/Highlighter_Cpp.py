import re
from PyQt6.QtGui import QFont, QColor, QTextCharFormat, QSyntaxHighlighter


class CppHighlighter(QSyntaxHighlighter):
    def __init__(self, parent=None):
        super().__init__(parent)
        self._rules = []

        # Keywords
        kw_fmt = QTextCharFormat()
        kw_fmt.setForeground(QColor("#f92672"))
        kw_fmt.setFontWeight(QFont.Weight.Bold)
        keywords = [
            "alignas", "alignof", "auto", "bool", "break", "case", "catch",
            "char", "class", "const", "constexpr", "continue", "decltype",
            "default", "delete", "do", "double", "else", "enum", "explicit",
            "export", "extern", "false", "float", "for", "friend", "goto",
            "if", "inline", "int", "long", "mutable", "namespace", "new",
            "noexcept", "nullptr", "operator", "private", "protected",
            "public", "register", "return", "short", "signed", "sizeof",
            "static", "static_assert", "static_cast", "struct", "switch",
            "template", "this", "throw", "true", "try", "typedef", "typeid",
            "typename", "union", "unsigned", "using", "virtual", "void",
            "volatile", "while", "override", "final",
        ]
        for w in keywords:
            self._rules.append((re.compile(rf"\b{w}\b"), kw_fmt))

        # Preprocessor directives
        prep_fmt = QTextCharFormat()
        prep_fmt.setForeground(QColor("#a6e22e"))
        self._rules.append((re.compile(r"^\s*#\s*\w+"), prep_fmt))

        # Standard library types / common identifiers
        type_fmt = QTextCharFormat()
        type_fmt.setForeground(QColor("#66d9ef"))
        type_fmt.setFontItalic(True)
        std_types = [
            "string", "vector", "map", "set", "pair", "array", "deque",
            "list", "queue", "stack", "unordered_map", "unordered_set",
            "shared_ptr", "unique_ptr", "weak_ptr", "optional",
            "cout", "cin", "cerr", "endl", "std",
            "size_t", "int8_t", "int16_t", "int32_t", "int64_t",
            "uint8_t", "uint16_t", "uint32_t", "uint64_t",
        ]
        for w in std_types:
            self._rules.append((re.compile(rf"\b{w}\b"), type_fmt))

        # Numbers
        num_fmt = QTextCharFormat()
        num_fmt.setForeground(QColor("#ae81ff"))
        self._rules.append((re.compile(r"\b\d+(\.\d+)?[fFlLuU]?\b"), num_fmt))
        self._rules.append((re.compile(r"\b0[xX][0-9a-fA-F]+\b"), num_fmt))

        # Strings
        str_fmt = QTextCharFormat()
        str_fmt.setForeground(QColor("#e6db74"))
        self._rules.append((re.compile(r'"[^"\\]*(\\.[^"\\]*)*"'), str_fmt))
        self._rules.append((re.compile(r"'[^'\\]*(\\.[^'\\]*)*'"), str_fmt))

        # Single-line comments
        comment_fmt = QTextCharFormat()
        comment_fmt.setForeground(QColor("#75715e"))
        comment_fmt.setFontItalic(True)
        self._rules.append((re.compile(r"//[^\n]*"), comment_fmt))

        # Function names (word followed by '(')
        func_fmt = QTextCharFormat()
        func_fmt.setForeground(QColor("#a6e22e"))
        self._rules.append((re.compile(r"\b\w+(?=\s*\()"), func_fmt))

    def highlightBlock(self, text):
        for pattern, fmt in self._rules:
            for m in pattern.finditer(text):
                self.setFormat(m.start(), m.end() - m.start(), fmt)
