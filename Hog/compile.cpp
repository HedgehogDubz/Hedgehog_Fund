#include <string>
#include <vector>
#include <stdexcept>
#include <unordered_set>

struct Node {
    std::string type;
    std::string value;
    std::vector<Node*> children;

    Node(std::string t, std::string val) : type(t), value(val) {}

    void add_child(Node* child) {
        children.push_back(child);
    }

    ~Node() {
        for (Node* child : children) {
            delete child;
        }
    }
};

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
    // imports
    "import", "from",
    // user-defined types
    "class", "struct", "enum",
    // literals
    "true", "false",
    // interop targets
    "python", "cpp", "hog"
};

static const std::unordered_set<std::string> TWO_CHAR_OPS = {
    "==", "!=", "<=", ">=", "&&", "||"
};

static const std::unordered_set<char> SINGLE_CHAR_OPS = {
    '+', '-', '*', '/', '%', '=', '<', '>', '!'
};

static const std::unordered_set<char> DELIMITERS = {
    '(', ')', '{', '}', '[', ']', ';', ',', '.'
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
            root->add_child(new Node("string_literal", str));
            continue;
        }

        // char literal
        if (c == '\'') {
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
            root->add_child(new Node("char_literal", ch));
            continue;
        }

        // number literal (int or float/double)
        if (is_digit(c)) {
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
            root->add_child(new Node(is_float ? "float_literal" : "int_literal", num));
            continue;
        }

        // identifier or keyword
        if (is_ident_start(c)) {
            std::string word;
            while (i < len && is_ident_char(code[i])) {
                word += code[i];
                i++;
                col++;
            }
            if (KEYWORDS.count(word)) {
                root->add_child(new Node("keyword", word));
            } else {
                root->add_child(new Node("identifier", word));
            }
            continue;
        }

        // two-character operators
        if (i + 1 < len) {
            std::string two_char = std::string(1, c) + code[i + 1];
            if (TWO_CHAR_OPS.count(two_char)) {
                root->add_child(new Node("operator", two_char));
                i += 2;
                col += 2;
                continue;
            }
        }

        // single-character operators
        if (SINGLE_CHAR_OPS.count(c)) {
            root->add_child(new Node("operator", std::string(1, c)));
            i++;
            col++;
            continue;
        }

        // delimiters
        if (DELIMITERS.count(c)) {
            root->add_child(new Node("delimiter", std::string(1, c)));
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

Node* compile(std::string code) {
    Node* tokens = tokenize(code);
    parse(tokens);
    return tokens;
}
