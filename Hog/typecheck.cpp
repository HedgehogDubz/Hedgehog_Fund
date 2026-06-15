#include "node.h"
#include <stdexcept>
#include <fstream>
#include <sstream>
#include <unordered_map>
#include <unordered_set>
#include <vector>

// Defined in compile.cpp — tokenize + parse (no typecheck) for imported files
Node* tokenize(std::string code);
Node* parse(Node* token_root);
// Defined in interpret.cpp — register builtin type signatures
void register_builtin_types();

static const std::unordered_set<std::string> NUMERIC_TYPES = {
    "int", "long", "float", "double"
};

static bool is_numeric(const std::string& t) { return NUMERIC_TYPES.count(t) > 0; }
static bool is_integer(const std::string& t) { return t == "int" || t == "long"; }
static bool is_float(const std::string& t)   { return t == "float" || t == "double"; }

static bool compatible(const std::string& from, const std::string& to) {
    if (from == to) return true;
    if (to == "auto" || from == "auto") return true;
    if (is_numeric(from) && is_numeric(to)) {
        if (to == "double") return true;
        if (to == "float" && is_integer(from)) return true;
        if (to == "long" && from == "int") return true;
    }
    return false;
}

static std::string arithmetic_result(const std::string& a, const std::string& b) {
    if (a == "double" || b == "double") return "double";
    if (a == "float"  || b == "float")  return "float";
    if (a == "long"   || b == "long")   return "long";
    return "int";
}

struct FuncSig {
    std::string return_type;
    std::vector<std::string> param_types;
};

// Exported symbol: either a variable type or a function signature
struct ExportedSym {
    std::string var_type;       // non-empty for variables
    bool is_func = false;
    FuncSig sig;
};

class TypeChecker {
private:
    std::vector<std::unordered_map<std::string, std::string>> scopes;
    std::unordered_map<std::string, FuncSig> functions;
    std::string current_return_type;
    std::vector<std::string> errors;
    std::string source_dir;
    std::unordered_set<std::string> resolved_files; // prevent circular resolution

    void error(Node* at, const std::string& msg) {
        std::string loc;
        if (at && at->line > 0)
            loc = std::to_string(at->line) + ":" + std::to_string(at->col) + ": ";
        errors.push_back(loc + "Type error: " + msg);
    }

    void push_scope() { scopes.emplace_back(); }
    void pop_scope()  { scopes.pop_back(); }

    void declare(Node* at, const std::string& name, const std::string& type) {
        if (scopes.back().count(name)) {
            error(at, "redeclaration of '" + name + "'");
            return;
        }
        scopes.back()[name] = type;
    }

    std::string lookup(const std::string& name) {
        for (int i = (int)scopes.size() - 1; i >= 0; i--) {
            auto it = scopes[i].find(name);
            if (it != scopes[i].end()) return it->second;
        }
        return "";
    }

    // ── import resolution ───────────────────────────────────────────────

    std::string resolve_path(const std::string& module_path) {
        std::string rel = module_path;
        for (char& c : rel) if (c == '.') c = '/';
        return source_dir + "/" + rel + ".hog";
    }

    // Parse an imported file and extract its exported symbols
    std::unordered_map<std::string, ExportedSym> resolve_exports(const std::string& filepath) {
        std::unordered_map<std::string, ExportedSym> exports;

        if (resolved_files.count(filepath)) return exports;
        resolved_files.insert(filepath);

        std::ifstream f(filepath);
        if (!f.is_open()) return exports; // file not found — will error at runtime
        std::ostringstream buf;
        buf << f.rdbuf();
        std::string code = buf.str();

        // tokenize + parse (no typecheck to avoid recursion issues)
        Node* tokens = tokenize(code);
        parse(tokens);

        // Walk top-level children looking for "export" nodes
        for (Node* child : tokens->children) {
            if (child->type != "export" || child->children.empty()) continue;
            Node* decl = child->children[0];

            if (decl->type == "func_decl") {
                ExportedSym sym;
                sym.is_func = true;
                sym.sig.return_type = "void";
                for (Node* fc : decl->children) {
                    if (fc->type == "type") sym.sig.return_type = fc->value;
                    else if (fc->type == "params") {
                        for (Node* p : fc->children) {
                            if (!p->children.empty() && p->children[0]->type == "type")
                                sym.sig.param_types.push_back(p->children[0]->value);
                        }
                    }
                }
                sym.var_type = sym.sig.return_type;
                exports[decl->value] = sym;
            } else if (decl->type == "var_decl") {
                ExportedSym sym;
                sym.is_func = false;
                sym.var_type = "auto";
                if (!decl->children.empty() && decl->children[0]->type == "type")
                    sym.var_type = decl->children[0]->value;
                exports[decl->value] = sym;
            } else if (decl->type == "class_decl") {
                // exported class — register as a known type
                ExportedSym sym;
                sym.is_func = false;
                sym.var_type = decl->value; // class name is its own type
                exports[decl->value] = sym;
            }
        }

        delete tokens;
        return exports;
    }

    // Process an import node: resolve file, register exported symbols
    void check_import(Node* node) {
        std::string filepath = resolve_path(node->value);
        auto exports = resolve_exports(filepath);

        if (exports.empty()) {
            error(node, "import '" + node->value + "' resolved no exported symbols");
            return;
        }

        for (auto& [name, sym] : exports) {
            if (sym.is_func) {
                functions[name] = sym.sig;
                declare(node, name, sym.sig.return_type);
            } else {
                declare(node, name, sym.var_type);
            }
        }
    }

    void check_from_import(Node* node) {
        std::string filepath = resolve_path(node->value);
        auto exports = resolve_exports(filepath);

        for (Node* child : node->children) {
            if (child->type != "import_name") continue;
            const std::string& name = child->value;
            auto it = exports.find(name);
            if (it == exports.end()) {
                error(child, "'" + name + "' is not exported by '" + node->value + "'");
                continue;
            }
            auto& sym = it->second;
            if (sym.is_func) {
                functions[name] = sym.sig;
                declare(child, name, sym.sig.return_type);
            } else {
                declare(child, name, sym.var_type);
            }
        }
    }

    // ── statement checking ──────────────────────────────────────────────

    void check_node(Node* node) {
        if (!node) return;
        const std::string& t = node->type;

        if (t == "program")        check_program(node);
        else if (t == "var_decl" || t == "parameter_decl") check_var_decl(node);
        else if (t == "func_decl") check_func_decl(node);
        else if (t == "export")    check_export(node);
        else if (t == "class_decl" || t == "struct_decl") check_class(node);
        else if (t == "if_stmt")    check_if(node);
        else if (t == "while_stmt") check_while(node);
        else if (t == "for_stmt")   check_for(node);
        else if (t == "return_stmt") check_return(node);
        else if (t == "block")      check_block(node);
        else if (t == "trade_block" || t == "metric_block") check_trade_block(node);
        else if (t == "import")      check_import(node);
        else if (t == "from_import") check_from_import(node);
        else if (t == "import_cpp")  check_import_cpp(node);
        else { infer(node); }
    }

    // Register exported names from a native .cpp module so calls type-check.
    // We don't compile here — just scan the source for HOG_EXPORTS({ ... }).
    void check_import_cpp(Node* node) {
        std::string cpp_path = source_dir + "/" + node->value + ".cpp";
        std::ifstream f(cpp_path);
        if (!f.is_open()) {
            error(node, "native module not found: " + cpp_path);
            return;
        }
        std::ostringstream buf; buf << f.rdbuf();
        const std::string content = buf.str();

        size_t pos = content.find("HOG_EXPORTS");
        if (pos == std::string::npos) {
            error(node, "native module '" + node->value + "' has no HOG_EXPORTS(...) macro");
            return;
        }
        size_t lbrace = content.find('{', pos);
        size_t rbrace = content.find('}', lbrace);
        if (lbrace == std::string::npos || rbrace == std::string::npos) {
            error(node, "malformed HOG_EXPORTS in '" + node->value + "'");
            return;
        }

        std::string inner = content.substr(lbrace + 1, rbrace - lbrace - 1);
        size_t i = 0;
        while (i < inner.size()) {
            size_t q1 = inner.find('"', i);
            if (q1 == std::string::npos) break;
            size_t q2 = inner.find('"', q1 + 1);
            if (q2 == std::string::npos) break;
            std::string name = inner.substr(q1 + 1, q2 - q1 - 1);
            if (!name.empty()) {
                // Variadic auto: any args allowed, any return.
                register_builtin_type(name, "auto", {"auto"});
            }
            i = q2 + 1;
        }
    }

    void check_export(Node* node) {
        // just check the inner declaration
        if (!node->children.empty())
            check_node(node->children[0]);
    }

    void check_program(Node* node) {
        push_scope();
        // first pass: resolve imports so their symbols are available
        for (Node* child : node->children) {
            if (child->type == "import" || child->type == "from_import")
                check_node(child);
        }
        // register local functions and trade blocks (including exported ones)
        for (Node* child : node->children) {
            if (child->type == "func_decl") register_func(child);
            else if (child->type == "trade_block" || child->type == "metric_block")
                register_trade(child);
            else if (child->type == "export" && !child->children.empty()) {
                Node* inner = child->children[0];
                if (inner->type == "func_decl")
                    register_func(inner);
                else if (inner->type == "trade_block" || inner->type == "metric_block")
                    register_trade(inner);
            }
        }
        // check everything (imports already handled, will skip)
        for (Node* child : node->children) {
            if (child->type != "import" && child->type != "from_import")
                check_node(child);
        }
        pop_scope();
    }

    void register_func(Node* func) {
        std::string name = func->value;
        std::string ret = "void";
        FuncSig sig;
        for (Node* child : func->children) {
            if (child->type == "type") ret = child->value;
            else if (child->type == "params") {
                for (Node* p : child->children) {
                    if (!p->children.empty() && p->children[0]->type == "type")
                        sig.param_types.push_back(p->children[0]->value);
                }
            }
        }
        sig.return_type = ret;
        functions[name] = sig;
        declare(func, name, ret);
    }

    void register_trade(Node* trade) {
        FuncSig sig;
        sig.return_type = "void";
        for (Node* child : trade->children) {
            if (child->type == "trade_params") {
                for (Node* p : child->children)
                    if (p->type == "trade_param")
                        sig.param_types.push_back("auto");
            }
        }
        functions[trade->value] = sig;
        declare(trade, trade->value, "void");
    }

    void check_var_decl(Node* node) {
        std::string declared_type = "auto";
        if (!node->children.empty() && node->children[0]->type == "type")
            declared_type = node->children[0]->value;

        if (node->children.size() >= 2) {
            std::string init_type = infer(node->children[1]);
            if (declared_type == "auto") {
                declared_type = init_type;
            } else if (init_type != "" && !compatible(init_type, declared_type)) {
                error(node->children[1], "cannot initialize '" + node->value + "' of type '"
                    + declared_type + "' with value of type '" + init_type + "'");
            }
        }
        declare(node, node->value, declared_type);
    }

    void check_func_decl(Node* node) {
        std::string ret = "void";
        for (Node* child : node->children)
            if (child->type == "type") { ret = child->value; break; }

        std::string prev_return = current_return_type;
        current_return_type = ret;

        push_scope();
        for (Node* child : node->children) {
            if (child->type == "params") {
                for (Node* p : child->children) {
                    std::string ptype = "auto";
                    if (!p->children.empty() && p->children[0]->type == "type")
                        ptype = p->children[0]->value;
                    declare(p, p->value, ptype);
                }
            }
        }
        for (Node* child : node->children)
            if (child->type == "block") check_block_inner(child);
        pop_scope();
        current_return_type = prev_return;
    }

    void check_class(Node* node) {
        push_scope();
        for (Node* child : node->children)
            if (child->type == "func_decl") register_func(child);
        for (Node* child : node->children)
            check_node(child);
        pop_scope();
    }

    void check_block(Node* node) {
        push_scope();
        check_block_inner(node);
        pop_scope();
    }

    void check_block_inner(Node* node) {
        for (Node* child : node->children) check_node(child);
    }

    void check_if(Node* node) {
        if (!node->children.empty()) {
            std::string cond_type = infer(node->children[0]);
            if (cond_type != "" && cond_type != "bool" && !is_numeric(cond_type))
                error(node->children[0], "condition in 'if' must be bool or numeric, got '" + cond_type + "'");
        }
        for (size_t i = 1; i < node->children.size(); i++) check_node(node->children[i]);
    }

    void check_while(Node* node) {
        if (!node->children.empty()) {
            std::string cond_type = infer(node->children[0]);
            if (cond_type != "" && cond_type != "bool" && !is_numeric(cond_type))
                error(node->children[0], "condition in 'while' must be bool or numeric, got '" + cond_type + "'");
        }
        for (size_t i = 1; i < node->children.size(); i++) check_node(node->children[i]);
    }

    void check_for(Node* node) {
        push_scope();
        if (node->children.size() >= 1) check_node(node->children[0]);
        if (node->children.size() >= 2) {
            std::string cond_type = infer(node->children[1]);
            if (cond_type != "" && cond_type != "bool" && !is_numeric(cond_type))
                error(node->children[1], "condition in 'for' must be bool or numeric, got '" + cond_type + "'");
        }
        if (node->children.size() >= 3) infer(node->children[2]);
        if (node->children.size() >= 4) check_block_inner(node->children[3]);
        pop_scope();
    }

    void check_return(Node* node) {
        if (current_return_type.empty()) { error(node, "'return' outside of function"); return; }
        if (node->children.empty()) {
            if (current_return_type != "void")
                error(node, "function expects return type '" + current_return_type + "' but 'return' has no value");
        } else {
            std::string ret_type = infer(node->children[0]);
            if (ret_type != "" && current_return_type != "void" && !compatible(ret_type, current_return_type))
                error(node->children[0], "cannot return '" + ret_type + "' from function returning '" + current_return_type + "'");
        }
    }

    void check_trade_block(Node* node) {
        push_scope();
        // Trade bodies are invoked per bar — bare `return;` is allowed to skip
        // the rest of the bar's logic.
        std::string prev_return = current_return_type;
        current_return_type = "void";
        // Implicit OHLCV bound by the runtime when a trade is invoked over data.
        declare(node, "open",   "List<double>");
        declare(node, "high",   "List<double>");
        declare(node, "low",    "List<double>");
        declare(node, "close",  "List<double>");
        declare(node, "volume", "List<double>");
        declare(node, "index",  "int");
        // Declare trade parameters as auto-typed variables
        for (Node* child : node->children) {
            if (child->type == "trade_params") {
                for (Node* p : child->children)
                    if (p->type == "trade_param")
                        declare(p, p->value, "auto");
            }
        }
        for (Node* child : node->children) {
            if (child->type != "trade_params")
                check_node(child);
        }
        current_return_type = prev_return;
        pop_scope();
    }

    // ── expression type inference ───────────────────────────────────────

    std::string infer(Node* node) {
        if (!node) return "";
        const std::string& t = node->type;

        if (t == "int_literal")    return "int";
        if (t == "float_literal")  return "double";
        if (t == "string_literal") return "string";
        if (t == "char_literal")   return "char";
        if (t == "bool_literal")   return "bool";
        if (t == "list_literal")   return "auto";
        if (t == "map_literal")    return "auto";

        if (t == "identifier")     return infer_identifier(node);
        if (t == "assign")         return infer_assign(node);
        if (t == "binary_op")      return infer_binary(node);
        if (t == "unary_op")       return infer_unary(node);
        if (t == "call")           return infer_call(node);
        if (t == "index")          return infer_index(node);
        if (t == "new_expr")       return node->value; // class name as type
        if (t == "member_access")  return "auto";
        if (t == "trade_action")   return "void";
        if (t == "signal") {
            if (node->value == "signal_int")    return "int";
            if (node->value == "signal_string") return "string";
            if (node->value == "signal_bool")   return "bool";
            if (node->value == "signal_line")   return "double";
            return "auto";
        }
        return "";
    }

    std::string infer_identifier(Node* node) {
        std::string type = lookup(node->value);
        if (type.empty()) {
            error(node, "use of undeclared identifier '" + node->value + "'");
            return "auto";
        }
        return type;
    }

    std::string infer_assign(Node* node) {
        if (node->children.size() < 2) return "";
        std::string lhs = infer(node->children[0]);
        std::string rhs = infer(node->children[1]);
        if (lhs != "" && rhs != "" && lhs != "auto" && rhs != "auto" && !compatible(rhs, lhs))
            error(node, "cannot assign '" + rhs + "' to variable of type '" + lhs + "'");
        return lhs;
    }

    std::string infer_binary(Node* node) {
        if (node->children.size() < 2) return "";
        std::string lhs = infer(node->children[0]);
        std::string rhs = infer(node->children[1]);
        const std::string& op = node->value;

        if (op == "==" || op == "!=" || op == "<" || op == ">" || op == "<=" || op == ">=") {
            if (lhs != "auto" && rhs != "auto" && lhs != rhs
                && !(is_numeric(lhs) && is_numeric(rhs)) && !(lhs == "char" && rhs == "char"))
                error(node, "cannot compare '" + lhs + "' with '" + rhs + "'");
            return "bool";
        }
        if (op == "&&" || op == "||") {
            if (lhs != "auto" && lhs != "bool" && !is_numeric(lhs))
                error(node->children[0], "left operand of '" + op + "' must be bool or numeric, got '" + lhs + "'");
            if (rhs != "auto" && rhs != "bool" && !is_numeric(rhs))
                error(node->children[1], "right operand of '" + op + "' must be bool or numeric, got '" + rhs + "'");
            return "bool";
        }
        if (op == "+" && (lhs == "string" || rhs == "string")) {
            // String concat with any type is valid (auto-converted via to_string)
            return "string";
        }
        if (op == "+" || op == "-" || op == "*" || op == "/" || op == "%") {
            if (lhs == "auto" || rhs == "auto") return "auto";
            if (!is_numeric(lhs)) { error(node->children[0], "left operand of '" + op + "' must be numeric, got '" + lhs + "'"); return "auto"; }
            if (!is_numeric(rhs)) { error(node->children[1], "right operand of '" + op + "' must be numeric, got '" + rhs + "'"); return "auto"; }
            if (op == "%" && (is_float(lhs) || is_float(rhs))) { error(node, "'%' operator requires integer operands"); return "int"; }
            return arithmetic_result(lhs, rhs);
        }
        return "auto";
    }

    std::string infer_unary(Node* node) {
        if (node->children.empty()) return "";
        std::string operand = infer(node->children[0]);
        if (node->value == "!") {
            if (operand != "auto" && operand != "bool" && !is_numeric(operand))
                error(node->children[0], "operand of '!' must be bool or numeric, got '" + operand + "'");
            return "bool";
        }
        if (node->value == "-") {
            if (operand != "auto" && !is_numeric(operand))
                error(node->children[0], "operand of unary '-' must be numeric, got '" + operand + "'");
            return operand;
        }
        return operand;
    }

    std::string infer_call(Node* node) {
        if (node->children.empty()) return "auto";
        Node* callee = node->children[0];
        std::string func_name = callee->value;

        // Check shared builtin registry
        if (callee->type == "identifier") {
            auto& registry = builtin_registry();
            auto bit = registry.find(func_name);
            if (bit != registry.end()) {
                const BuiltinSig& bsig = bit->second;
                if (node->children.size() >= 2 && node->children[1]->type == "args") {
                    Node* args = node->children[1];
                    // Only check arg count/types if the builtin has fixed params (not variadic like print)
                    if (!bsig.param_types.empty() && !(bsig.param_types.size() == 1 && bsig.param_types[0] == "auto")) {
                        int expected = (int)bsig.param_types.size();
                        int got = (int)args->children.size();
                        if (got != expected) {
                            error(node, "function '" + func_name + "' expects " + std::to_string(expected) + " argument(s), got " + std::to_string(got));
                        } else {
                            for (int i = 0; i < got; i++) {
                                std::string arg_type = infer(args->children[i]);
                                if (arg_type != "" && arg_type != "auto" && !compatible(arg_type, bsig.param_types[i]))
                                    error(args->children[i], "argument " + std::to_string(i + 1) + " of '" + func_name + "' expects '" + bsig.param_types[i] + "', got '" + arg_type + "'");
                            }
                        }
                    } else {
                        for (Node* a : args->children) infer(a);
                    }
                }
                return bsig.return_type;
            }
        }
        if (callee->type == "member_access") return "auto";

        auto it = functions.find(func_name);
        if (it == functions.end()) {
            infer(callee);
            return "auto";
        }

        const FuncSig& sig = it->second;
        if (node->children.size() >= 2 && node->children[1]->type == "args") {
            Node* args = node->children[1];
            int expected = (int)sig.param_types.size();
            int got = (int)args->children.size();
            if (got != expected) {
                error(node, "function '" + func_name + "' expects " + std::to_string(expected) + " argument(s), got " + std::to_string(got));
            } else {
                for (int i = 0; i < got; i++) {
                    std::string arg_type = infer(args->children[i]);
                    if (arg_type != "" && arg_type != "auto" && !compatible(arg_type, sig.param_types[i]))
                        error(args->children[i], "argument " + std::to_string(i + 1) + " of '" + func_name + "' expects '" + sig.param_types[i] + "', got '" + arg_type + "'");
                }
            }
        }
        return sig.return_type;
    }

    bool is_map_type(const std::string& t) { return t.find("Map") == 0; }

    std::string infer_index(Node* node) {
        if (node->children.size() >= 2) {
            std::string container_type = infer(node->children[0]);
            std::string idx_type = infer(node->children[1]);
            if (!is_map_type(container_type) && idx_type != "auto" && container_type != "auto"
                && container_type != "string" && !is_integer(idx_type))
                error(node->children[1], "index must be an integer type, got '" + idx_type + "'");
        }
        return "auto";
    }

public:
    TypeChecker() {}

    std::vector<std::string> check(Node* ast, const std::string& dir) {
        source_dir = dir;
        check_node(ast);
        return errors;
    }
};

// ── Entry point ─────────────────────────────────────────────────────────────
std::vector<std::string> typecheck(Node* ast, const std::string& source_dir) {
    register_builtin_types();
    TypeChecker checker;
    return checker.check(ast, source_dir);
}
