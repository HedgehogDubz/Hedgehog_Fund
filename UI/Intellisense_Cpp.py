CPP_COMPLETIONS = {
    # Keywords
    "keywords": [
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
    ],
    # Standard library types
    "std_types": [
        "std::string", "std::vector", "std::map", "std::set",
        "std::pair", "std::array", "std::deque", "std::list",
        "std::queue", "std::stack", "std::priority_queue",
        "std::unordered_map", "std::unordered_set",
        "std::shared_ptr", "std::unique_ptr", "std::weak_ptr",
        "std::optional", "std::variant", "std::any",
        "std::tuple", "std::function", "std::thread",
        "std::mutex", "std::lock_guard", "std::atomic",
        "std::cout", "std::cin", "std::cerr", "std::endl",
        "std::ifstream", "std::ofstream", "std::stringstream",
        "size_t", "int8_t", "int16_t", "int32_t", "int64_t",
        "uint8_t", "uint16_t", "uint32_t", "uint64_t",
    ],
    # Standard library functions
    "std_functions": [
        "std::sort", "std::find", "std::count", "std::accumulate",
        "std::transform", "std::for_each", "std::copy",
        "std::swap", "std::reverse", "std::min", "std::max",
        "std::min_element", "std::max_element",
        "std::distance", "std::advance", "std::next", "std::prev",
        "std::move", "std::forward", "std::make_shared",
        "std::make_unique", "std::make_pair", "std::make_tuple",
        "std::to_string", "std::stoi", "std::stod", "std::stof",
        "std::getline",
    ],
    # Common headers
    "headers": [
        "iostream", "string", "vector", "map", "set", "array",
        "algorithm", "numeric", "functional", "memory", "utility",
        "cmath", "cstdlib", "cstdio", "cstring", "cassert",
        "fstream", "sstream", "iomanip", "limits", "stdexcept",
        "thread", "mutex", "atomic", "chrono", "random",
        "unordered_map", "unordered_set", "queue", "stack", "deque",
        "list", "tuple", "optional", "variant", "any", "regex",
        "filesystem", "type_traits", "initializer_list",
    ],
    # Common snippets: trigger -> (display, insertion text)
    "snippets": {
        "include": ("#include <...>", '#include <${0:header}>'),
        "main": ("int main() {...}", "int main() {\n    ${0}\n    return 0;\n}"),
        "for": ("for loop", "for (${1:int i = 0}; ${2:i < n}; ${3:i++}) {\n    ${0}\n}"),
        "forr": ("range-based for", "for (${1:auto}& ${2:item} : ${3:container}) {\n    ${0}\n}"),
        "while": ("while loop", "while (${1:condition}) {\n    ${0}\n}"),
        "if": ("if statement", "if (${1:condition}) {\n    ${0}\n}"),
        "ife": ("if/else", "if (${1:condition}) {\n    ${2}\n} else {\n    ${0}\n}"),
        "class": ("class definition", "class ${1:Name} {\npublic:\n    ${1:Name}();\n    ~${1:Name}();\n\nprivate:\n    ${0}\n};"),
        "struct": ("struct definition", "struct ${1:Name} {\n    ${0}\n};"),
        "cout": ("std::cout", "std::cout << ${0} << std::endl;"),
        "cin": ("std::cin", "std::cin >> ${0};"),
        "vector": ("std::vector<T>", "std::vector<${1:int}> ${0:vec};"),
        "map": ("std::map<K,V>", "std::map<${1:std::string}, ${2:int}> ${0:m};"),
        "func": ("function definition", "${1:void} ${2:name}(${3:}) {\n    ${0}\n}"),
        "try": ("try/catch", "try {\n    ${1}\n} catch (${2:const std::exception& e}) {\n    ${0}\n}"),
        "namespace": ("namespace", "namespace ${1:name} {\n    ${0}\n}"),
        "template": ("template<...>", "template <typename ${1:T}>\n${0}"),
        "enum": ("enum class", "enum class ${1:Name} {\n    ${0}\n};"),
        "switch": ("switch statement", "switch (${1:var}) {\n    case ${2:val}:\n        ${0}\n        break;\n    default:\n        break;\n}"),
    },
}


def get_cpp_completions(prefix):
    """Return a list of (display_text, insertion_text) tuples matching prefix."""
    results = []
    prefix_lower = prefix.lower()

    for kw in CPP_COMPLETIONS["keywords"]:
        if kw.lower().startswith(prefix_lower) and kw.lower() != prefix_lower:
            results.append((kw, kw))

    for t in CPP_COMPLETIONS["std_types"]:
        if t.lower().startswith(prefix_lower) and t.lower() != prefix_lower:
            results.append((t, t))
        # Also match without std:: prefix
        short = t.removeprefix("std::")
        if short != t and short.lower().startswith(prefix_lower) and short.lower() != prefix_lower:
            results.append((t, t))

    for f in CPP_COMPLETIONS["std_functions"]:
        if f.lower().startswith(prefix_lower) and f.lower() != prefix_lower:
            results.append((f, f))
        short = f.removeprefix("std::")
        if short != f and short.lower().startswith(prefix_lower) and short.lower() != prefix_lower:
            results.append((f, f))

    # Match headers when typing after #include
    if prefix_lower.startswith("<") or prefix_lower.startswith('"'):
        inner = prefix_lower.lstrip('<"')
        for h in CPP_COMPLETIONS["headers"]:
            if h.startswith(inner) and h != inner:
                results.append((f"<{h}>", h))
    else:
        for h in CPP_COMPLETIONS["headers"]:
            if h.lower().startswith(prefix_lower) and h.lower() != prefix_lower:
                results.append((f"<{h}>", h))

    for trigger, (display, insertion) in CPP_COMPLETIONS["snippets"].items():
        if trigger.lower().startswith(prefix_lower) and trigger.lower() != prefix_lower:
            results.append((display, trigger))

    return results
