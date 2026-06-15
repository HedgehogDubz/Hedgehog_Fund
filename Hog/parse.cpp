#include "node.h"
#include <stdexcept>

// ─── Parser ─────────────────────────────────────────────────────────────────
// Transforms a flat token list (children of a "root" Node from tokenize())
// into a nested AST.  All AST nodes carry line/col from their source tokens.
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

    // Current token's position (0,0 if at end)
    int cur_line() { return at_end() ? 0 : tokens[pos]->line; }
    int cur_col()  { return at_end() ? 0 : tokens[pos]->col; }

    // Create a node at the current token's position
    Node* make(const std::string& type, const std::string& value) {
        return new Node(type, value, cur_line(), cur_col());
    }

    // Create a node at a saved position
    Node* make_at(const std::string& type, const std::string& value, int ln, int cl) {
        return new Node(type, value, ln, cl);
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

    Node* expect_any(const std::string& type1, const std::string& type2) {
        if (!at_end() && (tokens[pos]->type == type1 || tokens[pos]->type == type2))
            return advance();
        Node* got = peek();
        std::string got_desc = got ? (got->type + " '" + got->value + "'") : "end of input";
        throw std::runtime_error(
            "Parse error: expected " + type1 + " or " + type2 + " but got " + got_desc);
    }

    // ── type recognition ────────────────────────────────────────────────

    bool is_type_keyword(const std::string& val) {
        return val == "int" || val == "long" || val == "float" || val == "double"
            || val == "bool" || val == "char" || val == "string" || val == "void"
            || val == "Timestamp" || val == "auto" || val == "const"
            || val == "Tuple" || val == "List" || val == "Array"
            || val == "Set" || val == "Map";
    }

    bool looking_at_type() {
        if (at_end()) return false;
        Node* tok = peek();
        if (tok->type == "keyword" && is_type_keyword(tok->value)) return true;
        return false;
    }

    Node* parse_type() {
        Node* type_node = make("type", "");
        if (check("keyword", "const")) {
            type_node->value = "const ";
            advance();
        }
        if (check("keyword", "auto")) {
            type_node->value += "auto";
            advance();
            return type_node;
        }
        Node* tok = expect_any("keyword", "identifier");
        type_node->value += tok->value;

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

    bool looking_at_declaration() {
        if (at_end()) return false;
        int save = pos;
        bool result = false;
        try {
            if (check("keyword", "const")) pos++;
            if (at_end()) { pos = save; return false; }

            Node* tok = peek();
            bool is_type = (tok->type == "keyword" && is_type_keyword(tok->value));
            bool is_user_type = (tok->type == "identifier");
            if (!is_type && !is_user_type) { pos = save; return false; }
            pos++;

            if (!at_end() && check("operator", "<")) {
                int depth = 1;
                pos++;
                while (!at_end() && depth > 0) {
                    if (check("operator", "<")) depth++;
                    else if (check("operator", ">")) depth--;
                    pos++;
                }
            }

            if (!at_end() && tokens[pos]->type == "identifier") {
                // For user-defined types, verify it looks like a declaration
                // (TypeName varName = ... or TypeName varName; or TypeName funcName(...))
                if (is_user_type) {
                    int next = pos + 1;
                    if (next < (int)tokens.size()) {
                        result = (tokens[next]->value == "=" || tokens[next]->value == ";" || tokens[next]->value == "(");
                    }
                } else {
                    result = true;
                }
            }
        } catch (...) {}
        pos = save;
        return result;
    }

    // ── statements ──────────────────────────────────────────────────────

    Node* parse_program() {
        Node* program = make("program", "");
        while (!at_end()) {
            program->add_child(parse_statement());
        }
        return program;
    }

    Node* parse_statement() {
        Node* tok = peek();
        if (!tok) throw std::runtime_error("Parse error: unexpected end of input");

        if (tok->type == "keyword" && tok->value == "import") return parse_import();
        if (tok->type == "keyword" && tok->value == "from")   return parse_from_import();
        if (tok->type == "keyword" && tok->value == "export") return parse_export();
        if (tok->type == "keyword" && tok->value == "class")  return parse_class();
        if (tok->type == "keyword" && tok->value == "struct") return parse_struct();
        if (tok->type == "keyword" && tok->value == "enum")   return parse_enum();
        if (tok->type == "keyword" && tok->value == "if")       return parse_if();
        if (tok->type == "keyword" && tok->value == "while")    return parse_while();
        if (tok->type == "keyword" && tok->value == "for")      return parse_for();
        if (tok->type == "keyword" && tok->value == "return")   return parse_return();
        if (tok->type == "keyword" && tok->value == "break")    return parse_break();
        if (tok->type == "keyword" && tok->value == "continue") return parse_continue();
        if (tok->type == "keyword" && tok->value == "trade")  return parse_trade();
        if (tok->type == "keyword" && tok->value == "metric") return parse_metric();
        if (tok->type == "keyword" && tok->value == "parameter") return parse_parameter();
        if (looking_at_declaration()) return parse_declaration();
        return parse_expr_stmt();
    }

    // ── import ──────────────────────────────────────────────────────────

    Node* parse_import() {
        int ln = cur_line(), cl = cur_col();
        advance(); // consume "import"

        // `import cpp <name>;` — load a native (.cpp) module from User_Creations.
        if (check("keyword", "cpp")) {
            advance(); // consume "cpp"
            Node* name = expect_any("identifier", "keyword");
            Node* node = make_at("import_cpp", name->value, ln, cl);
            expect("delimiter", ";");
            return node;
        }

        std::string path;
        Node* name = expect_any("identifier", "keyword");
        path += name->value;
        while (match("delimiter", ".")) {
            path += ".";
            Node* part = expect_any("identifier", "keyword");
            path += part->value;
        }
        Node* node = make_at("import", path, ln, cl);
        expect("delimiter", ";");
        return node;
    }

    Node* parse_from_import() {
        int ln = cur_line(), cl = cur_col();
        advance(); // consume "from"
        Node* module = expect_any("identifier", "keyword");
        std::string path = module->value;
        while (match("delimiter", ".")) {
            Node* part = expect_any("identifier", "keyword");
            path += "." + part->value;
        }
        Node* node = make_at("from_import", path, ln, cl);
        expect("keyword", "import");
        Node* first = expect_any("identifier", "keyword");
        node->add_child(make_at("import_name", first->value, first->line, first->col));
        while (match("delimiter", ",")) {
            Node* next = expect_any("identifier", "keyword");
            node->add_child(make_at("import_name", next->value, next->line, next->col));
        }
        expect("delimiter", ";");
        return node;
    }

    // export <declaration | class | struct | enum>
    Node* parse_export() {
        int ln = cur_line(), cl = cur_col();
        advance(); // consume "export"
        Node* tok = peek();
        Node* decl;
        if (tok && tok->type == "keyword" && tok->value == "class")
            decl = parse_class();
        else if (tok && tok->type == "keyword" && tok->value == "struct")
            decl = parse_struct();
        else if (tok && tok->type == "keyword" && tok->value == "enum")
            decl = parse_enum();
        else
            decl = parse_declaration();
        Node* node = make_at("export", decl->value, ln, cl);
        node->add_child(decl);
        return node;
    }

    // ── class / struct / enum ───────────────────────────────────────────

    Node* parse_class() {
        int ln = cur_line(), cl = cur_col();
        advance();
        Node* name = expect_any("identifier", "keyword");
        Node* node = make_at("class_decl", name->value, ln, cl);
        std::string access_modifier = "private";
        // optional constructor parens after class name
        if (match("delimiter", "(")) {
            expect("delimiter", ")");
        }
        expect("delimiter", "{");
        while (!check("delimiter", "}")) {
            // consume access modifiers (public/private/protected/static)
            bool is_static = false;
            if (!at_end() && peek()->type == "keyword" &&
                (peek()->value == "public" || peek()->value == "private" || peek()->value == "protected")) {
                access_modifier = peek()->value;
                advance();
            }
            if (!at_end() && peek()->type == "keyword" && peek()->value == "static") {
                is_static = true;
                advance();
            }
            Node* member = parse_statement();
            // Tag the member with its access level
            std::string tag = access_modifier;
            if (is_static) tag = "static_" + tag;
            member->add_child(make_at("access", tag, member->line, member->col));
            node->add_child(member);
        }
        expect("delimiter", "}");
        return node;
    }

    Node* parse_struct() {
        int ln = cur_line(), cl = cur_col();
        advance();
        Node* name = expect_any("identifier", "keyword");
        Node* node = make_at("struct_decl", name->value, ln, cl);
        expect("delimiter", "{");
        while (!check("delimiter", "}")) {
            node->add_child(parse_statement());
        }
        expect("delimiter", "}");
        return node;
    }

    Node* parse_enum() {
        int ln = cur_line(), cl = cur_col();
        advance();
        Node* name = expect_any("identifier", "keyword");
        Node* node = make_at("enum_decl", name->value, ln, cl);
        expect("delimiter", "{");
        if (!check("delimiter", "}")) {
            Node* val = expect_any("identifier", "keyword");
            node->add_child(make_at("enum_value", val->value, val->line, val->col));
            while (match("delimiter", ",")) {
                if (check("delimiter", "}")) break;
                Node* next = expect_any("identifier", "keyword");
                node->add_child(make_at("enum_value", next->value, next->line, next->col));
            }
        }
        expect("delimiter", "}");
        return node;
    }

    // ── trade / metric ──────────────────────────────────────────────────

    Node* parse_trade() {
        int ln = cur_line(), cl = cur_col();
        advance();
        Node* name = expect_any("identifier", "keyword");
        Node* node = make_at("trade_block", name->value, ln, cl);
        // optional parameter list: trade name (volume, low, high, open, close) { ... }
        if (check("delimiter", "(")) {
            advance();
            node->add_child(parse_trade_params());
            expect("delimiter", ")");
        }
        node->add_child(parse_block());
        return node;
    }

    Node* parse_trade_params() {
        Node* params = make("trade_params", "");
        if (check("delimiter", ")")) return params;
        Node* first = expect_any("identifier", "keyword");
        params->add_child(make_at("trade_param", first->value, first->line, first->col));
        while (match("delimiter", ",")) {
            Node* next = expect_any("identifier", "keyword");
            params->add_child(make_at("trade_param", next->value, next->line, next->col));
        }
        return params;
    }

    // parameter <type> <name> = <main_value>;
    // Parsed as parameter_decl: value=name, children=[type, init_expr]
    Node* parse_parameter() {
        int ln = cur_line(), cl = cur_col();
        advance(); // consume "parameter"
        Node* type = parse_type();
        Node* name = expect_any("identifier", "keyword");
        Node* node = make_at("parameter_decl", name->value, ln, cl);
        node->add_child(type);
        expect("operator", "=");
        node->add_child(parse_expr());
        expect("delimiter", ";");
        return node;
    }

    Node* parse_metric() {
        int ln = cur_line(), cl = cur_col();
        advance();
        Node* name = expect_any("identifier", "keyword");
        Node* node = make_at("metric_block", name->value, ln, cl);
        // optional parameter list (same as trade)
        if (check("delimiter", "(")) {
            advance();
            node->add_child(parse_trade_params());
            expect("delimiter", ")");
        }
        node->add_child(parse_block());
        return node;
    }

    // ── control flow ────────────────────────────────────────────────────

    Node* parse_if() {
        int ln = cur_line(), cl = cur_col();
        advance();
        Node* node = make_at("if_stmt", "", ln, cl);
        expect("delimiter", "(");
        node->add_child(parse_expr());
        expect("delimiter", ")");
        node->add_child(parse_block());
        if (match("keyword", "else")) {
            if (check("keyword", "if")) {
                node->add_child(parse_if());
            } else {
                Node* else_node = make("else", "");
                else_node->add_child(parse_block());
                node->add_child(else_node);
            }
        }
        return node;
    }

    Node* parse_while() {
        int ln = cur_line(), cl = cur_col();
        advance();
        Node* node = make_at("while_stmt", "", ln, cl);
        expect("delimiter", "(");
        node->add_child(parse_expr());
        expect("delimiter", ")");
        node->add_child(parse_block());
        return node;
    }

    Node* parse_for() {
        int ln = cur_line(), cl = cur_col();
        advance();
        Node* node = make_at("for_stmt", "", ln, cl);
        expect("delimiter", "(");
        if (looking_at_declaration()) {
            node->add_child(parse_declaration());
        } else {
            node->add_child(parse_expr_stmt());
        }
        node->add_child(parse_expr());
        expect("delimiter", ";");
        node->add_child(parse_expr());
        expect("delimiter", ")");
        node->add_child(parse_block());
        return node;
    }

    Node* parse_return() {
        int ln = cur_line(), cl = cur_col();
        advance();
        Node* node = make_at("return_stmt", "", ln, cl);
        if (!check("delimiter", ";")) {
            node->add_child(parse_expr());
        }
        expect("delimiter", ";");
        return node;
    }

    Node* parse_break() {
        int ln = cur_line(), cl = cur_col();
        advance();
        Node* node = make_at("break_stmt", "", ln, cl);
        expect("delimiter", ";");
        return node;
    }

    Node* parse_continue() {
        int ln = cur_line(), cl = cur_col();
        advance();
        Node* node = make_at("continue_stmt", "", ln, cl);
        expect("delimiter", ";");
        return node;
    }

    // ── declarations ────────────────────────────────────────────────────

    Node* parse_declaration() {
        int ln = cur_line(), cl = cur_col();
        Node* type = parse_type();
        Node* name = expect_any("identifier", "keyword");

        if (check("delimiter", "(")) {
            Node* func = make_at("func_decl", name->value, ln, cl);
            func->add_child(type);
            advance();
            func->add_child(parse_params());
            expect("delimiter", ")");
            func->add_child(parse_block());
            return func;
        }

        Node* var = make_at("var_decl", name->value, ln, cl);
        var->add_child(type);
        if (match("operator", "=")) {
            var->add_child(parse_expr());
        }
        expect("delimiter", ";");
        return var;
    }

    Node* parse_params() {
        Node* params = make("params", "");
        if (check("delimiter", ")")) return params;
        params->add_child(parse_param());
        while (match("delimiter", ",")) {
            params->add_child(parse_param());
        }
        return params;
    }

    Node* parse_param() {
        int ln = cur_line(), cl = cur_col();
        Node* type = parse_type();
        Node* name = expect_any("identifier", "keyword");
        Node* param = make_at("param", name->value, ln, cl);
        param->add_child(type);
        return param;
    }

    // ── block ───────────────────────────────────────────────────────────

    Node* parse_block() {
        Node* block = make("block", "");
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
        int ln = cur_line(), cl = cur_col();
        if (match("operator", "=")) {
            Node* node = make_at("assign", "=", ln, cl);
            node->add_child(left);
            node->add_child(parse_assignment());
            return node;
        }
        // Compound assignment: +=, -=, *=, /=, %=
        // Desugars x += expr into x = x + expr
        for (const char* cop : {"+=", "-=", "*=", "/=", "%="}) {
            if (match("operator", cop)) {
                std::string base_op(1, cop[0]); // "+" from "+="
                // Duplicate left node for the binary_op (left is shared between assign target and binop operand)
                Node* left_copy = make_at(left->type, left->value, left->line, left->col);
                Node* binop = make_at("binary_op", base_op, ln, cl);
                binop->add_child(left_copy);
                binop->add_child(parse_assignment());
                Node* assign = make_at("assign", "=", ln, cl);
                assign->add_child(left);
                assign->add_child(binop);
                return assign;
            }
        }
        return left;
    }

    Node* parse_logical_or() {
        Node* left = parse_logical_and();
        while (true) {
            int ln = cur_line(), cl = cur_col();
            if (match("operator", "||")) {
                Node* node = make_at("binary_op", "||", ln, cl);
                node->add_child(left);
                node->add_child(parse_logical_and());
                left = node;
            } else break;
        }
        return left;
    }

    Node* parse_logical_and() {
        Node* left = parse_equality();
        while (true) {
            int ln = cur_line(), cl = cur_col();
            if (match("operator", "&&")) {
                Node* node = make_at("binary_op", "&&", ln, cl);
                node->add_child(left);
                node->add_child(parse_equality());
                left = node;
            } else break;
        }
        return left;
    }

    Node* parse_equality() {
        Node* left = parse_comparison();
        while (true) {
            int ln = cur_line(), cl = cur_col();
            if (match("operator", "==")) {
                Node* node = make_at("binary_op", "==", ln, cl);
                node->add_child(left);
                node->add_child(parse_comparison());
                left = node;
            } else if (match("operator", "!=")) {
                Node* node = make_at("binary_op", "!=", ln, cl);
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
            int ln = cur_line(), cl = cur_col();
            if (match("operator", "<")) {
                Node* node = make_at("binary_op", "<", ln, cl);
                node->add_child(left);
                node->add_child(parse_addition());
                left = node;
            } else if (match("operator", ">")) {
                Node* node = make_at("binary_op", ">", ln, cl);
                node->add_child(left);
                node->add_child(parse_addition());
                left = node;
            } else if (match("operator", "<=")) {
                Node* node = make_at("binary_op", "<=", ln, cl);
                node->add_child(left);
                node->add_child(parse_addition());
                left = node;
            } else if (match("operator", ">=")) {
                Node* node = make_at("binary_op", ">=", ln, cl);
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
            int ln = cur_line(), cl = cur_col();
            if (match("operator", "+")) {
                Node* node = make_at("binary_op", "+", ln, cl);
                node->add_child(left);
                node->add_child(parse_multiplication());
                left = node;
            } else if (match("operator", "-")) {
                Node* node = make_at("binary_op", "-", ln, cl);
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
            int ln = cur_line(), cl = cur_col();
            if (match("operator", "*")) {
                Node* node = make_at("binary_op", "*", ln, cl);
                node->add_child(left);
                node->add_child(parse_unary());
                left = node;
            } else if (match("operator", "/")) {
                Node* node = make_at("binary_op", "/", ln, cl);
                node->add_child(left);
                node->add_child(parse_unary());
                left = node;
            } else if (match("operator", "%")) {
                Node* node = make_at("binary_op", "%", ln, cl);
                node->add_child(left);
                node->add_child(parse_unary());
                left = node;
            } else break;
        }
        return left;
    }

    Node* parse_unary() {
        int ln = cur_line(), cl = cur_col();
        if (match("operator", "!")) {
            Node* node = make_at("unary_op", "!", ln, cl);
            node->add_child(parse_unary());
            return node;
        }
        if (match("operator", "-")) {
            Node* node = make_at("unary_op", "-", ln, cl);
            node->add_child(parse_unary());
            return node;
        }
        return parse_call();
    }

    Node* parse_call() {
        Node* left = parse_primary();
        while (true) {
            if (check("delimiter", "(")) {
                int ln = left->line, cl = left->col;
                advance();
                Node* call = make_at("call", "", ln, cl);
                call->add_child(left);
                call->add_child(parse_args());
                expect("delimiter", ")");
                left = call;
            } else if (check("delimiter", "[")) {
                int ln = left->line, cl = left->col;
                advance();
                Node* index = make_at("index", "", ln, cl);
                index->add_child(left);
                index->add_child(parse_expr());
                expect("delimiter", "]");
                left = index;
            } else if (check("delimiter", ".")) {
                int ln = left->line, cl = left->col;
                advance();
                Node* member_name = expect_any("identifier", "keyword");
                Node* member = make_at("member_access", member_name->value, ln, cl);
                member->add_child(left);
                left = member;
            } else if (check("operator", "++") || check("operator", "--")) {
                int ln = left->line, cl = left->col;
                std::string op = peek()->value;
                advance();
                Node* node = make_at("postfix_op", op, ln, cl);
                node->add_child(left);
                left = node;
            } else break;
        }
        return left;
    }

    Node* parse_args() {
        Node* args = make("args", "");
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

        // literals — tokens already have line/col from tokenizer
        if (tok->type == "int_literal" || tok->type == "float_literal"
            || tok->type == "string_literal" || tok->type == "char_literal") {
            return advance();
        }

        // new expression: new ClassName(args)
        if (tok->type == "keyword" && tok->value == "new") {
            int ln = tok->line, cl = tok->col;
            advance();
            Node* class_name = expect_any("identifier", "keyword");
            Node* node = make_at("new_expr", class_name->value, ln, cl);
            expect("delimiter", "(");
            node->add_child(parse_args());
            expect("delimiter", ")");
            return node;
        }

        // boolean literals
        if (tok->type == "keyword" && (tok->value == "true" || tok->value == "false")) {
            int ln = tok->line, cl = tok->col;
            advance();
            return make_at("bool_literal", tok->value, ln, cl);
        }

        // buy / sell
        if (tok->type == "keyword" && (tok->value == "buy" || tok->value == "sell")) {
            int ln = tok->line, cl = tok->col;
            std::string action = tok->value;
            advance();
            expect("delimiter", "(");
            Node* node = make_at("trade_action", action, ln, cl);
            node->add_child(parse_args());
            expect("delimiter", ")");
            return node;
        }

        // signal calls
        if (tok->type == "keyword"
            && (tok->value == "signal_int" || tok->value == "signal_string"
                || tok->value == "signal_bool" || tok->value == "signal_line")) {
            int ln = tok->line, cl = tok->col;
            std::string sig = tok->value;
            advance();
            expect("delimiter", "(");
            Node* node = make_at("signal", sig, ln, cl);
            node->add_child(parse_args());
            expect("delimiter", ")");
            return node;
        }

        // identifiers — already have line/col from tokenizer
        if (tok->type == "identifier") {
            return advance();
        }

        // list / array literal: [expr, expr, ...]
        if (tok->type == "delimiter" && tok->value == "[") {
            int ln = tok->line, cl = tok->col;
            advance();
            Node* node = make_at("list_literal", "", ln, cl);
            if (!check("delimiter", "]")) {
                node->add_child(parse_expr());
                while (match("delimiter", ",")) {
                    if (check("delimiter", "]")) break;
                    node->add_child(parse_expr());
                }
            }
            expect("delimiter", "]");
            return node;
        }

        // map literal: {key: val, key: val, ...}
        if (tok->type == "delimiter" && tok->value == "{") {
            int ln = tok->line, cl = tok->col;
            advance();
            Node* node = make_at("map_literal", "", ln, cl);
            if (!check("delimiter", "}")) {
                Node* pair = make("pair", "");
                pair->add_child(parse_expr());
                expect("delimiter", ":");
                pair->add_child(parse_expr());
                node->add_child(pair);
                while (match("delimiter", ",")) {
                    if (check("delimiter", "}")) break;
                    Node* p = make("pair", "");
                    p->add_child(parse_expr());
                    expect("delimiter", ":");
                    p->add_child(parse_expr());
                    node->add_child(p);
                }
            }
            expect("delimiter", "}");
            return node;
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
    token_root->children.clear();
    token_root->type = "program";

    for (Node* child : ast->children) {
        token_root->add_child(child);
    }
    ast->children.clear();
    delete ast;

    return token_root;
}
