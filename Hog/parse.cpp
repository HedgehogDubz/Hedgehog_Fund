#include <string>
#include <vector>
#include <stdexcept>

// Forward declaration — Node is defined in compile.cpp
struct Node;

// ─── Parser ─────────────────────────────────────────────────────────────────
// Transforms a flat token list (children of a "root" Node from tokenize())
// into a nested AST.  Grammar (informal):
//
//   program        → statement*
//   statement      → import_stmt | variable_decl | func_decl | class_decl
//                  | struct_decl | enum_decl | trade_block | metric_block
//                  | if_stmt | while_stmt | for_stmt | return_stmt
//                  | break_stmt | continue_stmt | expr_stmt
//   import_stmt    → "from" IDENT "import" IDENT ("," IDENT)* ";"
//                  | "import" IDENT ("." IDENT)* ";"
//   variable_decl  → type_spec IDENT ("=" expr)? ";"
//   func_decl      → type_spec IDENT "(" params? ")" block
//   class_decl     → "class" IDENT "{" member* "}"
//   struct_decl    → "struct" IDENT "{" member* "}"
//   enum_decl      → "enum" IDENT "{" IDENT ("," IDENT)* "}"
//   trade_block    → "trade" IDENT block
//   metric_block   → "metric" IDENT block
//   if_stmt        → "if" "(" expr ")" block ("else" (if_stmt | block))?
//   while_stmt     → "while" "(" expr ")" block
//   for_stmt       → "for" "(" (variable_decl | expr_stmt) expr ";" expr ")" block
//   return_stmt    → "return" expr? ";"
//   break_stmt     → "break" ";"
//   continue_stmt  → "continue" ";"
//   expr_stmt      → expr ";"
//   block          → "{" statement* "}"
//   expr           → assignment
//   assignment     → logical_or ("=" assignment)?
//   logical_or     → logical_and ("||" logical_and)*
//   logical_and    → equality ("&&" equality)*
//   equality       → comparison (("==" | "!=") comparison)*
//   comparison     → addition (("<" | ">" | "<=" | ">=") addition)*
//   addition       → multiplication (("+" | "-") multiplication)*
//   multiplication → unary (("*" | "/" | "%") unary)*
//   unary          → ("!" | "-") unary | call
//   call           → primary ( "(" args? ")" | "[" expr "]" | "." IDENT )*
//   primary        → NUMBER | STRING | CHAR | "true" | "false" | IDENT | "(" expr ")"
//                  | "buy" "(" args ")" | "sell" "(" args ")"
//                  | signal_call
// ────────────────────────────────────────────────────────────────────────────

class Parser {
private:
    std::vector<Node*>& tokens;
    int pos;

    // ── helpers ──────────────────────────────────────────────────────────

    bool at_end() {
        return pos >= (int)tokens.size();
    }

    Node* peek() {
        if (at_end()) return nullptr;
        return tokens[pos];
    }

    Node* advance() {
        Node* tok = tokens[pos];
        pos++;
        return tok;
    }

    bool check(const std::string& type, const std::string& value) {
        if (at_end()) return false;
        return tokens[pos]->type == type && tokens[pos]->value == value;
    }

    bool check_type(const std::string& type) {
        if (at_end()) return false;
        return tokens[pos]->type == type;
    }

    bool match(const std::string& type, const std::string& value) {
        if (check(type, value)) {
            advance();
            return true;
        }
        return false;
    }

    Node* expect(const std::string& type, const std::string& value) {
        if (check(type, value)) return advance();
        Node* got = peek();
        std::string got_desc = got ? (got->type + " '" + got->value + "'") : "end of input";
        throw std::runtime_error(
            "Parse error: expected " + type + " '" + value + "' but got " + got_desc);
    }

    // ── type recognition ────────────────────────────────────────────────

    bool is_type_keyword(const std::string& val) {
        return val == "int" || val == "long" || val == "float" || val == "double"
            || val == "bool" || val == "char" || val == "string" || val == "void"
            || val == "Timestamp" || val == "auto" || val == "const"
            || val == "Tuple" || val == "List" || val == "Array"
            || val == "Set" || val == "Map";
    }

    // Check if current position starts a type specifier (possibly followed by generic <…>)
    bool looking_at_type() {
        if (at_end()) return false;
        Node* tok = peek();
        if (tok->type == "keyword" && is_type_keyword(tok->value)) return true;
        // Could also be a user-defined type (identifier) followed by identifier (var name)
        
        return false;
    }

    // Parse a type specifier
    Node* parse_type() {
        Node* type_node = new Node("type", "");
        // optional "const"
        if (check("keyword", "const")) {
            type_node->value = "const ";
            advance();
        }
        // optional "auto"
        if (check("keyword", "auto")) {
            type_node->value += "auto";
            advance();
            return type_node;
        }
        Node* tok = expect_any("keyword", "identifier");
        type_node->value += tok->value;

        // generic parameters: Type<T, U>
        if (check("operator", "<")) {
            advance();
            type_node->value += "<";
            type_node->value += parse_type()->value;
            while (match("delimiter", ",")) {
                type_node->value += ",";
                type_node->value += parse_type()->value;
            }
            expect("operator", ">");
            type_node->value += ">";
        }

        return type_node;
    }

    Node* expect_any(const std::string& type1, const std::string& type2) {
        if (!at_end() && (tokens[pos]->type == type1 || tokens[pos]->type == type2))
            return advance();
        Node* got = peek();
        std::string got_desc = got ? (got->type + " '" + got->value + "'") : "end of input";
        throw std::runtime_error(
            "Parse error: expected " + type1 + " or " + type2 + " but got " + got_desc);
    }

    // Check if we're looking at: type_spec IDENT  (i.e. a declaration)
    bool looking_at_declaration() {
        if (at_end()) return false;
        int save = pos;
        bool result = false;
        try {
            // try to consume a type
            if (check("keyword", "const")) pos++;
            if (at_end()) { pos = save; return false; }

            Node* tok = peek();
            bool is_type = (tok->type == "keyword" && is_type_keyword(tok->value));
            if (!is_type) { pos = save; return false; }
            pos++;

            // skip generic params
            if (!at_end() && check("operator", "<")) {
                int depth = 1;
                pos++;
                while (!at_end() && depth > 0) {
                    if (check("operator", "<")) depth++;
                    else if (check("operator", ">")) depth--;
                    pos++;
                }
            }

            // next should be an identifier
            if (!at_end() && tokens[pos]->type == "identifier") {
                result = true;
            }
        } catch (...) {}
        pos = save;
        return result;
    }

    // ── statements ──────────────────────────────────────────────────────

    Node* parse_program() {
        Node* program = new Node("program", "");
        while (!at_end()) {
            program->add_child(parse_statement());
        }
        return program;
    }

    Node* parse_statement() {
        Node* tok = peek();
        if (!tok) throw std::runtime_error("Parse error: unexpected end of input");

        // import / from
        if (tok->type == "keyword" && tok->value == "import") return parse_import();
        if (tok->type == "keyword" && tok->value == "from")   return parse_from_import();

        // class / struct / enum
        if (tok->type == "keyword" && tok->value == "class")  return parse_class();
        if (tok->type == "keyword" && tok->value == "struct") return parse_struct();
        if (tok->type == "keyword" && tok->value == "enum")   return parse_enum();

        // control flow
        if (tok->type == "keyword" && tok->value == "if")       return parse_if();
        if (tok->type == "keyword" && tok->value == "while")    return parse_while();
        if (tok->type == "keyword" && tok->value == "for")      return parse_for();
        if (tok->type == "keyword" && tok->value == "return")   return parse_return();
        if (tok->type == "keyword" && tok->value == "break")    return parse_break();
        if (tok->type == "keyword" && tok->value == "continue") return parse_continue();

        // trade / metric blocks
        if (tok->type == "keyword" && tok->value == "trade")  return parse_trade();
        if (tok->type == "keyword" && tok->value == "metric") return parse_metric();

        // declaration (variable or function)
        if (looking_at_declaration()) return parse_declaration();

        // fallback: expression statement
        return parse_expr_stmt();
    }

    // ── import ──────────────────────────────────────────────────────────

    Node* parse_import() {
        // import foo.bar.baz;
        Node* node = new Node("import", "");
        advance(); // consume "import"
        std::string path;
        Node* name = expect_any("identifier", "keyword");
        path += name->value;
        while (match("delimiter", ".")) {
            path += ".";
            Node* part = expect_any("identifier", "keyword");
            path += part->value;
        }
        node->value = path;
        expect("delimiter", ";");
        return node;
    }

    Node* parse_from_import() {
        // from module import name1, name2;
        Node* node = new Node("from_import", "");
        advance(); // consume "from"
        Node* module = expect_any("identifier", "keyword");
        node->value = module->value;
        expect("keyword", "import");
        Node* first = expect_any("identifier", "keyword");
        node->add_child(new Node("import_name", first->value));
        while (match("delimiter", ",")) {
            Node* next = expect_any("identifier", "keyword");
            node->add_child(new Node("import_name", next->value));
        }
        expect("delimiter", ";");
        return node;
    }

    // ── class / struct / enum ───────────────────────────────────────────

    Node* parse_class() {
        advance(); // consume "class"
        Node* name = expect_any("identifier", "keyword");
        Node* node = new Node("class_decl", name->value);
        expect("delimiter", "{");
        while (!check("delimiter", "}")) {
            node->add_child(parse_statement());
        }
        expect("delimiter", "}");
        return node;
    }

    Node* parse_struct() {
        advance(); // consume "struct"
        Node* name = expect_any("identifier", "keyword");
        Node* node = new Node("struct_decl", name->value);
        expect("delimiter", "{");
        while (!check("delimiter", "}")) {
            node->add_child(parse_statement());
        }
        expect("delimiter", "}");
        return node;
    }

    Node* parse_enum() {
        advance(); // consume "enum"
        Node* name = expect_any("identifier", "keyword");
        Node* node = new Node("enum_decl", name->value);
        expect("delimiter", "{");
        if (!check("delimiter", "}")) {
            Node* val = expect_any("identifier", "keyword");
            node->add_child(new Node("enum_value", val->value));
            while (match("delimiter", ",")) {
                if (check("delimiter", "}")) break; // trailing comma
                Node* next = expect_any("identifier", "keyword");
                node->add_child(new Node("enum_value", next->value));
            }
        }
        expect("delimiter", "}");
        return node;
    }

    // ── trade / metric ──────────────────────────────────────────────────

    Node* parse_trade() {
        advance(); 
        Node* name = expect_any("identifier", "keyword");
        Node* node = new Node("trade_block", name->value);
        node->add_child(parse_block());
        return node;
    }

    Node* parse_metric() {
        advance(); 
        Node* name = expect_any("identifier", "keyword");
        Node* node = new Node("metric_block", name->value);
        node->add_child(parse_block());
        return node;
    }

    // ── control flow ────────────────────────────────────────────────────

    Node* parse_if() {
        advance(); 
        Node* node = new Node("if_stmt", "");
        expect("delimiter", "(");
        node->add_child(parse_expr()); // condition
        expect("delimiter", ")");
        node->add_child(parse_block()); // then-body
        if (match("keyword", "else")) {
            if (check("keyword", "if")) {
                node->add_child(parse_if()); // else-if chain
            } else {
                Node* else_node = new Node("else", "");
                else_node->add_child(parse_block());
                node->add_child(else_node);
            }
        }
        return node;
    }

    Node* parse_while() {
        advance(); // consume "while"
        Node* node = new Node("while_stmt", "");
        expect("delimiter", "(");
        node->add_child(parse_expr());
        expect("delimiter", ")");
        node->add_child(parse_block());
        return node;
    }

    Node* parse_for() {
        advance(); // consume "for"
        Node* node = new Node("for_stmt", "");
        expect("delimiter", "(");

        // init — either a declaration or expression statement
        if (looking_at_declaration()) {
            node->add_child(parse_declaration());
        } else {
            node->add_child(parse_expr_stmt());
        }

        // condition
        node->add_child(parse_expr());
        expect("delimiter", ";");

        // increment
        node->add_child(parse_expr());
        expect("delimiter", ")");

        node->add_child(parse_block());
        return node;
    }

    Node* parse_return() {
        advance(); // consume "return"
        Node* node = new Node("return_stmt", "");
        if (!check("delimiter", ";")) {
            node->add_child(parse_expr());
        }
        expect("delimiter", ";");
        return node;
    }

    Node* parse_break() {
        advance();
        Node* node = new Node("break_stmt", "");
        expect("delimiter", ";");
        return node;
    }

    Node* parse_continue() {
        advance();
        Node* node = new Node("continue_stmt", "");
        expect("delimiter", ";");
        return node;
    }

    // ── declarations ────────────────────────────────────────────────────

    Node* parse_declaration() {
        Node* type = parse_type();
        Node* name = expect_any("identifier", "keyword");

        // function declaration:  type name( ... ) { ... }
        if (check("delimiter", "(")) {
            Node* func = new Node("func_decl", name->value);
            func->add_child(type); // return type
            advance(); // consume "("
            func->add_child(parse_params());
            expect("delimiter", ")");
            func->add_child(parse_block());
            return func;
        }

        // variable declaration:  type name = expr ;
        Node* var = new Node("var_decl", name->value);
        var->add_child(type);
        if (match("operator", "=")) {
            var->add_child(parse_expr());
        }
        expect("delimiter", ";");
        return var;
    }

    Node* parse_params() {
        Node* params = new Node("params", "");
        if (check("delimiter", ")")) return params; // no params
        params->add_child(parse_param());
        while (match("delimiter", ",")) {
            params->add_child(parse_param());
        }
        return params;
    }

    Node* parse_param() {
        Node* type = parse_type();
        Node* name = expect_any("identifier", "keyword");
        Node* param = new Node("param", name->value);
        param->add_child(type);
        return param;
    }

    // ── block ───────────────────────────────────────────────────────────

    Node* parse_block() {
        Node* block = new Node("block", "");
        expect("delimiter", "{");
        while (!check("delimiter", "}")) {
            if (at_end()) throw std::runtime_error("Parse error: unterminated block");
            block->add_child(parse_statement());
        }
        expect("delimiter", "}");
        return block;
    }

    // ── expression statement ────────────────────────────────────────────

    Node* parse_expr_stmt() {
        Node* expr = parse_expr();
        expect("delimiter", ";");
        return expr;
    }

    // ── expressions (precedence climbing) ───────────────────────────────

    Node* parse_expr() {
        return parse_assignment();
    }

    Node* parse_assignment() {
        Node* left = parse_logical_or();
        if (match("operator", "=")) {
            Node* node = new Node("assign", "=");
            node->add_child(left);
            node->add_child(parse_assignment()); // right-associative
            return node;
        }
        return left;
    }

    Node* parse_logical_or() {
        Node* left = parse_logical_and();
        while (match("operator", "||")) {
            Node* node = new Node("binary_op", "||");
            node->add_child(left);
            node->add_child(parse_logical_and());
            left = node;
        }
        return left;
    }

    Node* parse_logical_and() {
        Node* left = parse_equality();
        while (match("operator", "&&")) {
            Node* node = new Node("binary_op", "&&");
            node->add_child(left);
            node->add_child(parse_equality());
            left = node;
        }
        return left;
    }

    Node* parse_equality() {
        Node* left = parse_comparison();
        while (true) {
            if (match("operator", "==")) {
                Node* node = new Node("binary_op", "==");
                node->add_child(left);
                node->add_child(parse_comparison());
                left = node;
            } else if (match("operator", "!=")) {
                Node* node = new Node("binary_op", "!=");
                node->add_child(left);
                node->add_child(parse_comparison());
                left = node;
            } else break;
        }
        return left;
    }

    Node* parse_comparison() {
        Node* left = parse_addition();
        while (true) {
            if (match("operator", "<")) {
                Node* node = new Node("binary_op", "<");
                node->add_child(left);
                node->add_child(parse_addition());
                left = node;
            } else if (match("operator", ">")) {
                Node* node = new Node("binary_op", ">");
                node->add_child(left);
                node->add_child(parse_addition());
                left = node;
            } else if (match("operator", "<=")) {
                Node* node = new Node("binary_op", "<=");
                node->add_child(left);
                node->add_child(parse_addition());
                left = node;
            } else if (match("operator", ">=")) {
                Node* node = new Node("binary_op", ">=");
                node->add_child(left);
                node->add_child(parse_addition());
                left = node;
            } else break;
        }
        return left;
    }

    Node* parse_addition() {
        Node* left = parse_multiplication();
        while (true) {
            if (match("operator", "+")) {
                Node* node = new Node("binary_op", "+");
                node->add_child(left);
                node->add_child(parse_multiplication());
                left = node;
            } else if (match("operator", "-")) {
                Node* node = new Node("binary_op", "-");
                node->add_child(left);
                node->add_child(parse_multiplication());
                left = node;
            } else break;
        }
        return left;
    }

    Node* parse_multiplication() {
        Node* left = parse_unary();
        while (true) {
            if (match("operator", "*")) {
                Node* node = new Node("binary_op", "*");
                node->add_child(left);
                node->add_child(parse_unary());
                left = node;
            } else if (match("operator", "/")) {
                Node* node = new Node("binary_op", "/");
                node->add_child(left);
                node->add_child(parse_unary());
                left = node;
            } else if (match("operator", "%")) {
                Node* node = new Node("binary_op", "%");
                node->add_child(left);
                node->add_child(parse_unary());
                left = node;
            } else break;
        }
        return left;
    }

    Node* parse_unary() {
        if (match("operator", "!")) {
            Node* node = new Node("unary_op", "!");
            node->add_child(parse_unary());
            return node;
        }
        if (match("operator", "-")) {
            Node* node = new Node("unary_op", "-");
            node->add_child(parse_unary());
            return node;
        }
        return parse_call();
    }

    Node* parse_call() {
        Node* left = parse_primary();
        while (true) {
            if (check("delimiter", "(")) {
                // function call
                advance();
                Node* call = new Node("call", "");
                call->add_child(left); // callee
                call->add_child(parse_args());
                expect("delimiter", ")");
                left = call;
            } else if (check("delimiter", "[")) {
                // index access
                advance();
                Node* index = new Node("index", "");
                index->add_child(left);
                index->add_child(parse_expr());
                expect("delimiter", "]");
                left = index;
            } else if (check("delimiter", ".")) {
                // member access
                advance();
                Node* member_name = expect_any("identifier", "keyword");
                Node* member = new Node("member_access", member_name->value);
                member->add_child(left);
                left = member;
            } else break;
        }
        return left;
    }

    Node* parse_args() {
        Node* args = new Node("args", "");
        if (check("delimiter", ")")) return args;
        args->add_child(parse_expr());
        while (match("delimiter", ",")) {
            args->add_child(parse_expr());
        }
        return args;
    }

    Node* parse_primary() {
        if (at_end()) throw std::runtime_error("Parse error: unexpected end of input in expression");
        Node* tok = peek();

        // literals
        if (tok->type == "int_literal" || tok->type == "float_literal"
            || tok->type == "string_literal" || tok->type == "char_literal") {
            return advance();
        }

        // boolean literals
        if (tok->type == "keyword" && (tok->value == "true" || tok->value == "false")) {
            advance();
            return new Node("bool_literal", tok->value);
        }

        // buy / sell  (trading actions used as expressions)
        if (tok->type == "keyword" && (tok->value == "buy" || tok->value == "sell")) {
            std::string action = tok->value;
            advance();
            expect("delimiter", "(");
            Node* node = new Node("trade_action", action);
            node->add_child(parse_args());
            expect("delimiter", ")");
            return node;
        }

        // signal calls
        if (tok->type == "keyword"
            && (tok->value == "signal_int" || tok->value == "signal_string"
                || tok->value == "signal_bool")) {
            std::string sig = tok->value;
            advance();
            expect("delimiter", "(");
            Node* node = new Node("signal", sig);
            node->add_child(parse_args());
            expect("delimiter", ")");
            return node;
        }

        // identifiers
        if (tok->type == "identifier") {
            return advance();
        }

        // grouped expression
        if (tok->type == "delimiter" && tok->value == "(") {
            advance();
            Node* expr = parse_expr();
            expect("delimiter", ")");
            return expr;
        }

        throw std::runtime_error(
            "Parse error: unexpected token " + tok->type + " '" + tok->value + "'");
    }

public:
    Parser(std::vector<Node*>& toks) : tokens(toks), pos(0) {}

    Node* parse() {
        return parse_program();
    }
};

// ── Entry point called from compile() in compile.cpp ────────────────────────
Node* parse(Node* token_root) {
    Parser parser(token_root->children);
    Node* ast = parser.parse();

    // Replace the flat token list with the structured AST
    // Clear old children without deleting (AST now owns the token nodes it kept)
    token_root->children.clear();
    token_root->type = "program";

    // Move AST children into root
    for (Node* child : ast->children) {
        token_root->add_child(child);
    }
    ast->children.clear(); // prevent double-free
    delete ast;

    return token_root;
}
