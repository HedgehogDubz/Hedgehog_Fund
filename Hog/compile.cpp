#include "node.h"
#include <stdexcept>
#include <unordered_set>

static const std::unordered_set<std::string> KEYWORDS = {
    // primitive types
    "int", "long", "float", "double", "bool", "char", "string", "void",
    "Timestamp",
    // container types
    "Tuple", "List", "Array", "Set", "Map",
    // modifiers
    "auto", "const",
    // control flow
    "if", "else", "for", "while", "break", "continue", "return",
    // special constructs
    "trade", "metric",
    // trading actions
    "buy", "sell",
    // signals
    "signal_int", "signal_string", "signal_bool",
    // imports / exports
    "import", "from", "export",
    // user-defined types
    "class", "struct", "enum", "new",
    // access modifiers
    "public", "private", "protected", "static",
    // literals
    "true", "false",
    // interop targets
    "python", "cpp", "hog"
    
};

static const std::unordered_set<std::string> TWO_CHAR_OPS = {
    "==", "!=", "<=", ">=", "&&", "||", "++", "--", "+=", "-=", "*=", "/=", "%="
};

static const std::unordered_set<char> SINGLE_CHAR_OPS = {
    '+', '-', '*', '/', '%', '=', '<', '>', '!'
};

static const std::unordered_set<char> DELIMITERS = {
    '(', ')', '{', '}', '[', ']', ';', ',', '.', ':'
};

static bool is_ident_start(char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_';
}

static bool is_ident_char(char c) {
    return is_ident_start(c) || (c >= '0' && c <= '9');
}

static bool is_digit(char c) {
    return c >= '0' && c <= '9';
}

Node* tokenize(std::string code) {
    Node* root = new Node("root", "");
    int i = 0;
    int len = code.size();
    int line = 1;
    int col = 1;

    while (i < len) {
        char c = code[i];

        // track line/col for error reporting
        if (c == '\n') {
            line++;
            col = 1;
            i++;
            continue;
        }

        // skip whitespace
        if (c == ' ' || c == '\t' || c == '\r') {
            i++;
            col++;
            continue;
        }

        // single-line comment
        if (c == '/' && i + 1 < len && code[i + 1] == '/') {
            i += 2;
            while (i < len && code[i] != '\n') {
                i++;
            }
            continue;
        }

        // multi-line comment
        if (c == '/' && i + 1 < len && code[i + 1] == '*') {
            i += 2;
            while (i + 1 < len && !(code[i] == '*' && code[i + 1] == '/')) {
                if (code[i] == '\n') {
                    line++;
                    col = 1;
                }
                i++;
            }
            if (i + 1 >= len) {
                throw std::runtime_error(
                    "Unterminated block comment at line " + std::to_string(line));
            }
            i += 2; // skip */
            continue;
        }

        // string literal
        if (c == '"') {
            int start_line = line, start_col = col;
            std::string str;
            i++;
            col++;
            while (i < len && code[i] != '"') {
                if (code[i] == '\\' && i + 1 < len) {
                    char next = code[i + 1];
                    switch (next) {
                        case 'n':  str += '\n'; break;
                        case 't':  str += '\t'; break;
                        case '\\': str += '\\'; break;
                        case '"':  str += '"';  break;
                        default:   str += next; break;
                    }
                    i += 2;
                    col += 2;
                } else if (code[i] == '\n') {
                    throw std::runtime_error(
                        "Unterminated string literal at line " + std::to_string(line));
                } else {
                    str += code[i];
                    i++;
                    col++;
                }
            }
            if (i >= len) {
                throw std::runtime_error(
                    "Unterminated string literal at line " + std::to_string(line));
            }
            i++;
            col++;
            root->add_child(new Node("string_literal", str, start_line, start_col));
            continue;
        }

        // char literal
        if (c == '\'') {
            int start_line = line, start_col = col;
            std::string ch;
            i++;
            col++;
            if (i < len && code[i] == '\\' && i + 1 < len) {
                char next = code[i + 1];
                switch (next) {
                    case 'n':  ch = "\n"; break;
                    case 't':  ch = "\t"; break;
                    case '\\': ch = "\\"; break;
                    case '\'': ch = "'";  break;
                    default:   ch = std::string(1, next); break;
                }
                i += 2;
                col += 2;
            } else if (i < len) {
                ch = std::string(1, code[i]);
                i++;
                col++;
            }
            if (i >= len || code[i] != '\'') {
                throw std::runtime_error(
                    "Unterminated char literal at line " + std::to_string(line));
            }
            i++;
            col++;
            root->add_child(new Node("char_literal", ch, start_line, start_col));
            continue;
        }

        // number literal (int or float/double)
        if (is_digit(c)) {
            int start_line = line, start_col = col;
            std::string num;
            bool is_float = false;
            while (i < len && is_digit(code[i])) {
                num += code[i];
                i++;
                col++;
            }
            if (i < len && code[i] == '.' && i + 1 < len && is_digit(code[i + 1])) {
                is_float = true;
                num += code[i]; // the dot
                i++;
                col++;
                while (i < len && is_digit(code[i])) {
                    num += code[i];
                    i++;
                    col++;
                }
            }
            root->add_child(new Node(is_float ? "float_literal" : "int_literal", num, start_line, start_col));
            continue;
        }

        // identifier or keyword
        if (is_ident_start(c)) {
            int start_line = line, start_col = col;
            std::string word;
            while (i < len && is_ident_char(code[i])) {
                word += code[i];
                i++;
                col++;
            }
            if (KEYWORDS.count(word)) {
                root->add_child(new Node("keyword", word, start_line, start_col));
            } else {
                root->add_child(new Node("identifier", word, start_line, start_col));
            }
            continue;
        }

        // two-character operators
        if (i + 1 < len) {
            std::string two_char = std::string(1, c) + code[i + 1];
            if (TWO_CHAR_OPS.count(two_char)) {
                root->add_child(new Node("operator", two_char, line, col));
                i += 2;
                col += 2;
                continue;
            }
        }

        // single-character operators
        if (SINGLE_CHAR_OPS.count(c)) {
            root->add_child(new Node("operator", std::string(1, c), line, col));
            i++;
            col++;
            continue;
        }

        // delimiters
        if (DELIMITERS.count(c)) {
            root->add_child(new Node("delimiter", std::string(1, c), line, col));
            i++;
            col++;
            continue;
        }

        throw std::runtime_error(
            "Unexpected character '" + std::string(1, c) +
            "' at line " + std::to_string(line) +
            ", col " + std::to_string(col));
    }

    return root;
}

// Defined in parse.cpp
Node* parse(Node* token_root);
// Defined in typecheck.cpp
std::vector<std::string> typecheck(Node* ast, const std::string& source_dir);

// Compile without type-checking (used by interpreter for imported modules)
Node* compile(std::string code) {
    Node* tokens = tokenize(code);
    parse(tokens);
    return tokens;
}

// Compile with type-checking (used by main entry point)
Node* compile(std::string code, const std::string& source_dir) {
    Node* tokens = tokenize(code);
    parse(tokens);

    std::vector<std::string> type_errors = typecheck(tokens, source_dir);
    if (!type_errors.empty()) {
        std::string msg;
        for (const std::string& e : type_errors) {
            msg += e + "\n";
        }
        throw std::runtime_error(msg);
    }

    return tokens;
}
