HOG_COMPLETIONS = {
    # Keywords
    "keywords": [
        "if", "else", "for", "while", "break", "continue", "return",
        "class", "struct", "enum", "new",
        "import", "from", "export",
        "trade", "metric",
        "public", "private", "protected",
        "const", "auto", "void",
        "true", "false",
        "buy", "sell",
    ],
    # Types
    "types": [
        "int", "long", "float", "double", "bool", "char", "string",
        "Timestamp", "Tuple", "List", "Array", "Set", "Map",
    ],
    # Built-in functions
    "builtins": [
        "print", "time", "chrono",
        "signal_int", "signal_string", "signal_bool",
    ],
    # Container methods
    "methods": [
        "size", "length", "push", "pop", "append", "add",
        "insert", "remove", "contains", "clear", "get",
        "set", "put", "delete", "has", "keys", "values",
        "substr",
    ],
    # Common snippets: trigger -> (display, insertion text)
    "snippets": {
        "class": ("class Name {...}", "class ${1:Name} {\n    ${0}\n}"),
        "struct": ("struct Name {...}", "struct ${1:Name} {\n    ${0}\n}"),
        "enum": ("enum Name {...}", "enum ${1:Name} {\n    ${0}\n}"),
        "if": ("if (...) {...}", "if (${1:condition}) {\n    ${0}\n}"),
        "ife": ("if/else", "if (${1:condition}) {\n    ${2}\n} else {\n    ${0}\n}"),
        "for": ("for loop", "for (${1:int i = 0}; ${2:i < n}; ${3:i++}) {\n    ${0}\n}"),
        "while": ("while (...) {...}", "while (${1:condition}) {\n    ${0}\n}"),
        "func": ("type name(...) {...}", "${1:void} ${2:name}(${3:}) {\n    ${0}\n}"),
        "trade": ("trade Name {...}", "trade ${1:Name} {\n    ${0}\n}"),
        "metric": ("metric Name {...}", "metric ${1:Name} {\n    ${0}\n}"),
        "import": ("import module;", "import ${0:module};"),
        "from": ("from mod import ...;", "from ${1:module} import ${0:name};"),
        "export": ("export ...", "export ${0}"),
        "print": ("print(...)", "print(${0});"),
        "new": ("new ClassName()", "new ${1:ClassName}(${0})"),
    },
}


def get_hog_completions(prefix):
    """Return a list of (display_text, insertion_text) tuples matching prefix."""
    results = []
    prefix_lower = prefix.lower()

    for kw in HOG_COMPLETIONS["keywords"]:
        if kw.lower().startswith(prefix_lower) and kw.lower() != prefix_lower:
            results.append((kw, kw))

    for t in HOG_COMPLETIONS["types"]:
        if t.lower().startswith(prefix_lower) and t.lower() != prefix_lower:
            results.append((t, t))

    for bi in HOG_COMPLETIONS["builtins"]:
        if bi.lower().startswith(prefix_lower) and bi.lower() != prefix_lower:
            results.append((bi, bi))

    # Methods (typically after a dot)
    if "." in prefix:
        method_prefix = prefix.split(".")[-1].lower()
        for m in HOG_COMPLETIONS["methods"]:
            if m.lower().startswith(method_prefix) and m.lower() != method_prefix:
                results.append((m, m))
    else:
        for m in HOG_COMPLETIONS["methods"]:
            if m.lower().startswith(prefix_lower) and m.lower() != prefix_lower:
                results.append((m, m))

    for trigger, (display, insertion) in HOG_COMPLETIONS["snippets"].items():
        if trigger.lower().startswith(prefix_lower) and trigger.lower() != prefix_lower:
            results.append((display, trigger))

    return results
