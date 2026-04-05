PYTHON_COMPLETIONS = {
    # Keywords
    "keywords": [
        "False", "None", "True", "and", "as", "assert", "async", "await",
        "break", "class", "continue", "def", "del", "elif", "else",
        "except", "finally", "for", "from", "global", "if", "import",
        "in", "is", "lambda", "nonlocal", "not", "or", "pass", "raise",
        "return", "try", "while", "with", "yield",
    ],
    # Built-in functions
    "builtins": [
        "abs", "all", "any", "bin", "bool", "breakpoint", "bytearray",
        "bytes", "callable", "chr", "classmethod", "compile", "complex",
        "delattr", "dict", "dir", "divmod", "enumerate", "eval", "exec",
        "filter", "float", "format", "frozenset", "getattr", "globals",
        "hasattr", "hash", "help", "hex", "id", "input", "int",
        "isinstance", "issubclass", "iter", "len", "list", "locals",
        "map", "max", "memoryview", "min", "next", "object", "oct",
        "open", "ord", "pow", "print", "property", "range", "repr",
        "reversed", "round", "set", "setattr", "slice", "sorted",
        "staticmethod", "str", "sum", "super", "tuple", "type", "vars",
        "zip",
    ],
    # Common snippets: trigger -> (display, insertion text)
    "snippets": {
        "def": ("def func(...):", "def ${1:name}(${2:args}):\n    ${0:pass}"),
        "class": ("class Name(...):", "class ${1:Name}:\n    def __init__(self${2:}):\n        ${0:pass}"),
        "if": ("if ...:", "if ${1:condition}:\n    ${0:pass}"),
        "elif": ("elif ...:", "elif ${1:condition}:\n    ${0:pass}"),
        "else": ("else:", "else:\n    ${0:pass}"),
        "for": ("for ... in ...:", "for ${1:item} in ${2:iterable}:\n    ${0:pass}"),
        "while": ("while ...:", "while ${1:condition}:\n    ${0:pass}"),
        "try": ("try/except", "try:\n    ${1:pass}\nexcept ${2:Exception} as ${3:e}:\n    ${0:pass}"),
        "with": ("with ... as ...:", "with ${1:expr} as ${2:var}:\n    ${0:pass}"),
        "import": ("import ...", "import ${0:module}"),
        "from": ("from ... import ...", "from ${1:module} import ${0:name}"),
        "lambda": ("lambda ...:", "lambda ${1:args}: ${0:expr}"),
        "print": ("print(...)", "print(${0})"),
        "main": ("if __name__ == '__main__'", 'if __name__ == "__main__":\n    ${0:main()}'),
    },
    # Common module-level completions
    "modules": [
        "os", "sys", "math", "random", "json", "re", "datetime",
        "collections", "itertools", "functools", "pathlib", "typing",
        "subprocess", "threading", "multiprocessing", "socket",
        "http", "urllib", "csv", "io", "time", "logging",
    ],
}


def get_python_completions(prefix):
    """Return a list of (display_text, insertion_text) tuples matching prefix."""
    results = []
    prefix_lower = prefix.lower()

    for kw in PYTHON_COMPLETIONS["keywords"]:
        if kw.lower().startswith(prefix_lower) and kw.lower() != prefix_lower:
            results.append((kw, kw))

    for bi in PYTHON_COMPLETIONS["builtins"]:
        if bi.lower().startswith(prefix_lower) and bi.lower() != prefix_lower:
            results.append((bi, bi))

    for mod in PYTHON_COMPLETIONS["modules"]:
        if mod.lower().startswith(prefix_lower) and mod.lower() != prefix_lower:
            results.append((mod, mod))

    for trigger, (display, insertion) in PYTHON_COMPLETIONS["snippets"].items():
        if trigger.lower().startswith(prefix_lower) and trigger.lower() != prefix_lower:
            results.append((display, trigger))

    return results
