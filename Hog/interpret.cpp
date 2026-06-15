#include "node.h"
#include "hog_native.h"
#include "native_loader.h"
#include <stdexcept>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <memory>
#include <algorithm>
#include <fstream>
#include <sstream>
#include <cmath>
#include <ctime>
#include <functional>
#include <numbers>

// Defined in compile.cpp
Node* compile(std::string code);

// Forward declaration — defined at bottom of this file
void register_builtin_types();

struct Value;
using VecPtr = std::shared_ptr<std::vector<Value>>;
using MapPtr = std::shared_ptr<std::vector<std::pair<Value, Value>>>;
using ObjPtr = std::shared_ptr<std::unordered_map<std::string, Value>>;

struct Value {
    enum Kind { VOID, INT, LONG, FLOAT, DOUBLE, BOOL, CHAR, STRING,
                LIST, ARRAY, TUPLE, SET, MAP, OBJECT };
    Kind kind;
    long long ival;
    double fval;
    bool bval;
    char cval;
    std::string sval;
    VecPtr elems;       // LIST, ARRAY, TUPLE, SET
    MapPtr pairs;       // MAP
    ObjPtr obj_fields;  // OBJECT (sval holds class name)

    Value() : kind(VOID), ival(0), fval(0), bval(false), cval(0) {}

    static Value make_void()                      { Value v; v.kind = VOID; return v; }
    static Value make_int(long long n)            { Value v; v.kind = INT; v.ival = n; return v; }
    static Value make_long(long long n)           { Value v; v.kind = LONG; v.ival = n; return v; }
    static Value make_float(double n)             { Value v; v.kind = FLOAT; v.fval = n; return v; }
    static Value make_double(double n)            { Value v; v.kind = DOUBLE; v.fval = n; return v; }
    static Value make_bool(bool b)                { Value v; v.kind = BOOL; v.bval = b; return v; }
    static Value make_char(char c)                { Value v; v.kind = CHAR; v.cval = c; return v; }
    static Value make_string(const std::string& s){ Value v; v.kind = STRING; v.sval = s; return v; }

    static Value make_list(const std::vector<Value>& e) {
        Value v; v.kind = LIST; v.elems = std::make_shared<std::vector<Value>>(e); return v;
    }
    static Value make_array(const std::vector<Value>& e) {
        Value v; v.kind = ARRAY; v.elems = std::make_shared<std::vector<Value>>(e); return v;
    }
    static Value make_tuple(const std::vector<Value>& e) {
        Value v; v.kind = TUPLE; v.elems = std::make_shared<std::vector<Value>>(e); return v;
    }
    static Value make_set(const std::vector<Value>& e) {
        Value v; v.kind = SET; v.elems = std::make_shared<std::vector<Value>>(e); return v;
    }
    static Value make_map(const std::vector<std::pair<Value, Value>>& p) {
        Value v; v.kind = MAP; v.pairs = std::make_shared<std::vector<std::pair<Value, Value>>>(p); return v;
    }
    static Value make_object(const std::string& cls, const std::unordered_map<std::string, Value>& f) {
        Value v; v.kind = OBJECT; v.sval = cls;
        v.obj_fields = std::make_shared<std::unordered_map<std::string, Value>>(f); return v;
    }

    bool is_numeric()   const { return kind == INT || kind == LONG || kind == FLOAT || kind == DOUBLE; }
    bool is_integer()   const { return kind == INT || kind == LONG; }
    bool is_container() const { return kind >= LIST && kind <= MAP; }

    double    as_double() const { return (kind == DOUBLE || kind == FLOAT) ? fval : (double)ival; }
    long long as_int()    const { return (kind == INT || kind == LONG) ? ival : (long long)fval; }

    bool truthy() const {
        switch (kind) {
            case BOOL: return bval;
            case INT: case LONG: return ival != 0;
            case FLOAT: case DOUBLE: return fval != 0.0;
            case STRING: return !sval.empty();
            case CHAR: return cval != 0;
            case LIST: case ARRAY: case TUPLE: case SET: return elems && !elems->empty();
            case MAP: return pairs && !pairs->empty();
            case OBJECT: return true;
            default: return false;
        }
    }

    bool operator==(const Value& o) const {
        if (kind != o.kind) return false;
        switch (kind) {
            case INT: case LONG: return ival == o.ival;
            case FLOAT: case DOUBLE: return fval == o.fval;
            case BOOL: return bval == o.bval;
            case CHAR: return cval == o.cval;
            case STRING: return sval == o.sval;
            default: return false;
        }
    }

    std::string to_string() const {
        switch (kind) {
            case VOID: return "void";
            case INT: case LONG: return std::to_string(ival);
            case FLOAT: case DOUBLE: {
                std::string s = std::to_string(fval);
                size_t dot = s.find('.');
                if (dot != std::string::npos) {
                    size_t last = s.find_last_not_of('0');
                    if (last == dot) last++;
                    s = s.substr(0, last + 1);
                }
                return s;
            }
            case BOOL: return bval ? "true" : "false";
            case CHAR: return std::string(1, cval);
            case STRING: return sval;
            case LIST: case ARRAY: case TUPLE: case SET: {
                char open  = (kind == TUPLE) ? '(' : (kind == SET ? '{' : '[');
                char close = (kind == TUPLE) ? ')' : (kind == SET ? '}' : ']');
                std::string s(1, open);
                if (elems) for (size_t i = 0; i < elems->size(); i++) {
                    if (i) s += ", ";
                    auto& el = (*elems)[i];
                    s += (el.kind == STRING) ? ("\"" + el.sval + "\"") : el.to_string();
                }
                s += close;
                return s;
            }
            case MAP: {
                std::string s = "{";
                if (pairs) for (size_t i = 0; i < pairs->size(); i++) {
                    if (i) s += ", ";
                    auto& [k, val] = (*pairs)[i];
                    s += (k.kind == STRING) ? ("\"" + k.sval + "\"") : k.to_string();
                    s += ": ";
                    s += (val.kind == STRING) ? ("\"" + val.sval + "\"") : val.to_string();
                }
                s += "}";
                return s;
            }
            case OBJECT: {
                std::string s = sval + "{";
                if (obj_fields) {
                    bool first = true;
                    for (auto& [name, val] : *obj_fields) {
                        if (!first) s += ", ";
                        first = false;
                        s += name + ": " + val.to_string();
                    }
                }
                s += "}";
                return s;
            }
        }
        return "";
    }
};

struct ReturnException { Value value; };
struct BreakException {};
struct ContinueException {};
struct FuncDef { Node* node; };
struct ClassDef {
    Node* node;
    std::unordered_map<std::string, FuncDef> methods;
    std::unordered_map<std::string, std::string> access;  // member name -> "public"/"private"/"protected"/"static_*"
    std::unordered_map<std::string, FuncDef> static_methods;
    std::unordered_map<std::string, Value> static_fields;
};

// ── Native-module bridge ────────────────────────────────────────────────────
// Glue between hog Values and the HogContext callbacks user .cpp code uses.
struct NativeBridge {
    const std::vector<Value>* args = nullptr;
    Value result = Value::make_void();
    bool result_set = false;
    bool building_map = false;
    std::vector<std::pair<Value, Value>> map_pairs;
    // Owned buffers so accessor return pointers stay valid through the call.
    std::vector<std::vector<double>>    double_bufs;
    std::vector<std::vector<long long>> int_bufs;
    std::vector<std::string>            string_bufs;
};

static NativeBridge* bridge_of(HogContext* ctx) {
    return static_cast<NativeBridge*>(ctx->internal);
}

static int  br_arg_count (HogContext* ctx)         { return (int)bridge_of(ctx)->args->size(); }
static long long br_arg_int(HogContext* ctx, int i){
    auto* b = bridge_of(ctx);
    if (i < 0 || i >= (int)b->args->size()) return 0;
    return (*b->args)[i].as_int();
}
static double br_arg_double(HogContext* ctx, int i){
    auto* b = bridge_of(ctx);
    if (i < 0 || i >= (int)b->args->size()) return 0.0;
    return (*b->args)[i].as_double();
}
static const char* br_arg_string(HogContext* ctx, int i){
    auto* b = bridge_of(ctx);
    if (i < 0 || i >= (int)b->args->size()) return "";
    b->string_bufs.push_back((*b->args)[i].sval);
    return b->string_bufs.back().c_str();
}
static const double* br_arg_list_double(HogContext* ctx, int i, int* out_len){
    auto* b = bridge_of(ctx);
    if (i < 0 || i >= (int)b->args->size() || !(*b->args)[i].elems) {
        *out_len = 0; return nullptr;
    }
    const auto& src = *(*b->args)[i].elems;
    std::vector<double> buf;
    buf.reserve(src.size());
    for (auto& el : src) buf.push_back(el.as_double());
    *out_len = (int)buf.size();
    b->double_bufs.push_back(std::move(buf));
    return b->double_bufs.back().data();
}
static const long long* br_arg_list_int(HogContext* ctx, int i, int* out_len){
    auto* b = bridge_of(ctx);
    if (i < 0 || i >= (int)b->args->size() || !(*b->args)[i].elems) {
        *out_len = 0; return nullptr;
    }
    const auto& src = *(*b->args)[i].elems;
    std::vector<long long> buf;
    buf.reserve(src.size());
    for (auto& el : src) buf.push_back(el.as_int());
    *out_len = (int)buf.size();
    b->int_bufs.push_back(std::move(buf));
    return b->int_bufs.back().data();
}

static void br_ret_int(HogContext* ctx, long long v){
    auto* b = bridge_of(ctx); b->result = Value::make_int(v); b->result_set = true;
}
static void br_ret_double(HogContext* ctx, double v){
    auto* b = bridge_of(ctx); b->result = Value::make_double(v); b->result_set = true;
}
static void br_ret_string(HogContext* ctx, const char* s){
    auto* b = bridge_of(ctx); b->result = Value::make_string(s ? s : ""); b->result_set = true;
}
static void br_ret_list_double(HogContext* ctx, const double* data, int len){
    auto* b = bridge_of(ctx);
    std::vector<Value> elems; elems.reserve(len);
    for (int i = 0; i < len; i++) elems.push_back(Value::make_double(data[i]));
    b->result = Value::make_list(elems); b->result_set = true;
}
static void br_ret_list_int(HogContext* ctx, const long long* data, int len){
    auto* b = bridge_of(ctx);
    std::vector<Value> elems; elems.reserve(len);
    for (int i = 0; i < len; i++) elems.push_back(Value::make_int(data[i]));
    b->result = Value::make_list(elems); b->result_set = true;
}
static void br_ret_map_begin(HogContext* ctx){
    auto* b = bridge_of(ctx); b->building_map = true; b->map_pairs.clear();
}
static void br_ret_map_set_double(HogContext* ctx, const char* k, double v){
    bridge_of(ctx)->map_pairs.push_back({Value::make_string(k ? k : ""), Value::make_double(v)});
}
static void br_ret_map_set_int(HogContext* ctx, const char* k, long long v){
    bridge_of(ctx)->map_pairs.push_back({Value::make_string(k ? k : ""), Value::make_int(v)});
}
static void br_ret_map_set_list_double(HogContext* ctx, const char* k, const double* data, int len){
    auto* b = bridge_of(ctx);
    std::vector<Value> elems; elems.reserve(len);
    for (int i = 0; i < len; i++) elems.push_back(Value::make_double(data[i]));
    b->map_pairs.push_back({Value::make_string(k ? k : ""), Value::make_list(elems)});
}
static void br_ret_map_set_list_int(HogContext* ctx, const char* k, const long long* data, int len){
    auto* b = bridge_of(ctx);
    std::vector<Value> elems; elems.reserve(len);
    for (int i = 0; i < len; i++) elems.push_back(Value::make_int(data[i]));
    b->map_pairs.push_back({Value::make_string(k ? k : ""), Value::make_list(elems)});
}

static HogContext make_hog_ctx(NativeBridge& b) {
    HogContext c{};
    c.arg_count = br_arg_count;
    c.arg_int = br_arg_int;
    c.arg_double = br_arg_double;
    c.arg_string = br_arg_string;
    c.arg_list_double = br_arg_list_double;
    c.arg_list_int = br_arg_list_int;
    c.ret_int = br_ret_int;
    c.ret_double = br_ret_double;
    c.ret_string = br_ret_string;
    c.ret_list_double = br_ret_list_double;
    c.ret_list_int = br_ret_list_int;
    c.ret_map_begin = br_ret_map_begin;
    c.ret_map_set_double = br_ret_map_set_double;
    c.ret_map_set_int = br_ret_map_set_int;
    c.ret_map_set_list_double = br_ret_map_set_list_double;
    c.ret_map_set_list_int = br_ret_map_set_list_int;
    c.internal = &b;
    return c;
}

class Interpreter {
private:
    std::vector<std::unordered_map<std::string, Value>> scopes;
    std::unordered_map<std::string, FuncDef> functions;
    std::unordered_map<std::string, ClassDef> classes;
    std::unordered_map<std::string, Node*> trades;  // registered trade/metric blocks
    std::string source_dir;
    std::string current_class;  // non-empty when executing inside a class method
    std::unordered_set<std::string> imported_files; // prevent circular imports
    std::vector<NativeModule> native_modules;        // loaded .cpp modules
    // The bar index currently being processed inside a per-bar trade run.
    // -1 outside of a per-bar trade. buy()/sell() with no args use this.
    long long current_bar_index = -1;

    // ── built-in functions ──────────────────────────────────────────────

    using BuiltinFn = std::function<Value(Node*)>;
    std::unordered_map<std::string, BuiltinFn> builtins;

    void init_builtins() {
        register_builtin_types();
        make_builtin_function("print", [this](Node* args) -> Value {
            if (args) {
                for (size_t i = 0; i < args->children.size(); i++) {
                    if (i > 0) std::cout << " ";
                    std::cout << eval(args->children[i]).to_string();
                }
            }
            std::cout << "\n";
            return Value::make_void();
        });
        make_builtin_function("time", [this](Node*) -> Value {
            return Value::make_int(static_cast<int>(std::time(nullptr)));
        });
        make_builtin_function("chrono", [this](Node*) -> Value {
            return Value::make_int(std::chrono::high_resolution_clock::now().time_since_epoch().count());
        });

        // ── math ─────────────────────────────────────────────────────────
        make_builtin_function("pow", [this](Node* args) -> Value {
            if (!args || args->children.size() < 2)
                throw std::runtime_error("Runtime error: pow expects {base: number, exponent: number}");
            double b = eval(args->children[0]).as_double();
            double e = eval(args->children[1]).as_double();
            return Value::make_double(std::pow(b, e));
        });
        make_builtin_function("sqrt", [this](Node* args) -> Value {
            if (!args || args->children.empty())
                throw std::runtime_error("Runtime error: sqrt expects {x: number}");
            double x = eval(args->children[0]).as_double();
            return Value::make_double(std::sqrt(x));
        });
        make_builtin_function("log", [this](Node* args) -> Value {
            if (!args || args->children.empty())
                throw std::runtime_error("Runtime error: log expects {x: number, base?: number}");
            double x = eval(args->children[0]).as_double();
            if (args->children.size() >= 2) {
                double base = eval(args->children[1]).as_double();
                return Value::make_double(std::log(x) / std::log(base));
            }
            return Value::make_double(std::log(x));
        });
        make_builtin_function("abs", [this](Node* args) -> Value {
            if (!args || args->children.empty())
                throw std::runtime_error("Runtime error: abs expects {x: number}");
            Value v = eval(args->children[0]);
            if (v.is_integer()) return Value::make_int(std::llabs(v.as_int()));
            return Value::make_double(std::fabs(v.as_double()));
        });
    }

    void make_builtin_function(const std::string& name, BuiltinFn fn){
        builtins[name] = fn;
    }

    void push_scope() { scopes.emplace_back(); }
    void pop_scope()  { scopes.pop_back(); }

    void set_var(const std::string& name, const Value& val) {
        scopes.back()[name] = val;
    }

    Value* lookup_var(const std::string& name) {
        for (int i = (int)scopes.size() - 1; i >= 0; i--) {
            auto it = scopes[i].find(name);
            if (it != scopes[i].end()) return &it->second;
        }
        return nullptr;
    }

    void assign_var(const std::string& name, const Value& val) {
        for (int i = (int)scopes.size() - 1; i >= 0; i--) {
            auto it = scopes[i].find(name);
            if (it != scopes[i].end()) { it->second = val; return; }
        }
        throw std::runtime_error("Runtime error: assignment to undeclared variable '" + name + "'");
    }

    // ── statements ──────────────────────────────────────────────────────

    void exec(Node* node) {
        if (!node) return;
        const std::string& t = node->type;
        if (t == "program") exec_program(node);
        else if (t == "var_decl" || t == "parameter_decl") exec_var_decl(node);
        else if (t == "func_decl") {}
        else if (t == "block") exec_block(node);
        else if (t == "if_stmt") exec_if(node);
        else if (t == "while_stmt") exec_while(node);
        else if (t == "for_stmt") exec_for(node);
        else if (t == "return_stmt") exec_return(node);
        else if (t == "break_stmt") throw BreakException{};
        else if (t == "continue_stmt") throw ContinueException{};
        else if (t == "trade_block" || t == "metric_block") {} // registered in exec_program, only called externally
        else if (t == "export") exec_export(node);
        else if (t == "class_decl") exec_class_decl(node);
        else if (t == "struct_decl" || t == "enum_decl") {}
        else if (t == "import") exec_import(node);
        else if (t == "from_import") exec_from_import(node);
        else if (t == "import_cpp") exec_import_cpp(node);
        else eval(node);
    }

    void exec_import_cpp(Node* node) {
        NativeModule mod = load_native_module(source_dir, node->value);
        native_modules.push_back(mod);
        for (const std::string& fn_name : mod.exports) {
            void* sym = native_module_get_fn(mod, fn_name);
            if (!sym) {
                throw std::runtime_error("Native module '" + mod.name +
                                         "' missing symbol hog_fn_" + fn_name);
            }
            HogFn fn = reinterpret_cast<HogFn>(sym);
            builtins[fn_name] = [this, fn](Node* args) -> Value {
                std::vector<Value> arg_vals;
                if (args) {
                    arg_vals.reserve(args->children.size());
                    for (Node* a : args->children) arg_vals.push_back(eval(a));
                }
                NativeBridge bridge;
                bridge.args = &arg_vals;
                HogContext ctx = make_hog_ctx(bridge);
                fn(&ctx);
                if (bridge.building_map) return Value::make_map(bridge.map_pairs);
                if (!bridge.result_set) return Value::make_void();
                return bridge.result;
            };
            // Mirror in the typecheck registry so future typechecks see it too.
            register_builtin_type(fn_name, "auto", {"auto"});
        }
    }

    void exec_program(Node* node) {
        push_scope();
        for (Node* c : node->children) {
            if (c->type == "func_decl") functions[c->value] = FuncDef{c};
            else if (c->type == "trade_block" || c->type == "metric_block")
                trades[c->value] = c;
            else if (c->type == "export" && !c->children.empty()) {
                Node* inner = c->children[0];
                if (inner->type == "func_decl")
                    functions[inner->value] = FuncDef{inner};
                else if (inner->type == "class_decl")
                    exec_class_decl(inner);
                else if (inner->type == "trade_block" || inner->type == "metric_block")
                    trades[inner->value] = inner;
            }
        }
        for (Node* c : node->children) exec(c);
        pop_scope();
    }

    void exec_export(Node* node) {
        // execute the inner declaration normally
        if (!node->children.empty())
            exec(node->children[0]);
    }

    // Helper: extract the access tag from a class member node
    std::string get_member_access(Node* member) {
        for (Node* c : member->children)
            if (c->type == "access") return c->value;
        return "private"; // default
    }

    bool is_static_access(const std::string& acc) {
        return acc.find("static_") == 0;
    }

    // Check if access is allowed from the current context
    void check_access(const std::string& class_name, const std::string& member_name,
                      const std::string& acc) {
        std::string base = acc;
        if (is_static_access(base)) base = base.substr(7); // strip "static_"
        if (base == "public") return;
        if (base == "private" && current_class != class_name)
            throw std::runtime_error("Runtime error: cannot access private member '"
                + member_name + "' of class '" + class_name + "'");
        if (base == "protected" && current_class != class_name)
            throw std::runtime_error("Runtime error: cannot access protected member '"
                + member_name + "' of class '" + class_name + "'");
    }

    void exec_class_decl(Node* node) {
        ClassDef cls;
        cls.node = node;
        for (Node* c : node->children) {
            std::string acc = get_member_access(c);
            if (c->type == "func_decl") {
                cls.access[c->value] = acc;
                if (is_static_access(acc))
                    cls.static_methods[c->value] = FuncDef{c};
                else
                    cls.methods[c->value] = FuncDef{c};
            } else if (c->type == "var_decl") {
                cls.access[c->value] = acc;
                if (is_static_access(acc)) {
                    // Initialize static field now
                    push_scope();
                    exec_var_decl(c);
                    Value* v = lookup_var(c->value);
                    if (v) cls.static_fields[c->value] = *v;
                    pop_scope();
                }
            }
        }
        classes[node->value] = cls;
    }

    void exec_var_decl(Node* node) {
        Value val = Value::make_void();
        std::string dt;
        if (!node->children.empty() && node->children[0]->type == "type")
            dt = node->children[0]->value;

        if (node->children.size() >= 2) {
            val = eval(node->children[1]);
            // coerce [..] literal to the declared container type
            if (val.kind == Value::LIST) {
                if (dt.find("Array") == 0) val.kind = Value::ARRAY;
                else if (dt.find("Tuple") == 0) val.kind = Value::TUPLE;
                else if (dt.find("Set") == 0) {
                    val.kind = Value::SET;
                    // deduplicate
                    if (val.elems) {
                        std::vector<Value> unique;
                        for (auto& el : *val.elems) {
                            bool found = false;
                            for (auto& u : unique) if (u == el) { found = true; break; }
                            if (!found) unique.push_back(el);
                        }
                        *val.elems = unique;
                    }
                }
            }
        } else {
            if (dt == "int") val = Value::make_int(0);
            else if (dt == "long") val = Value::make_long(0);
            else if (dt == "float") val = Value::make_float(0.0);
            else if (dt == "double") val = Value::make_double(0.0);
            else if (dt == "bool") val = Value::make_bool(false);
            else if (dt == "char") val = Value::make_char('\0');
            else if (dt == "string") val = Value::make_string("");
            else if (dt.find("List") == 0) val = Value::make_list({});
            else if (dt.find("Array") == 0) val = Value::make_array({});
            else if (dt.find("Tuple") == 0) val = Value::make_tuple({});
            else if (dt.find("Set") == 0) val = Value::make_set({});
            else if (dt.find("Map") == 0) val = Value::make_map({});
        }
        set_var(node->value, val);
    }

    // Convert "math.utils" → "math/utils.hog"
    std::string resolve_import_path(const std::string& module_path) {
        std::string rel = module_path;
        for (char& c : rel) if (c == '.') c = '/';
        rel += ".hog";
        return source_dir + "/" + rel;
    }

    // Read a file into a string
    std::string read_file(const std::string& path) {
        std::ifstream f(path);
        if (!f.is_open())
            throw std::runtime_error("Runtime error: cannot open '" + path + "'");
        std::ostringstream buf;
        buf << f.rdbuf();
        return buf.str();
    }

    // Compile and execute an imported file, returning only its exported symbols
    std::unordered_map<std::string, Value> run_module(const std::string& filepath) {
        if (imported_files.count(filepath))
            return {};
        imported_files.insert(filepath);

        std::string code = read_file(filepath);
        Node* ast = compile(code);

        std::string mod_dir = filepath;
        size_t slash = mod_dir.find_last_of("/\\");
        if (slash != std::string::npos) mod_dir = mod_dir.substr(0, slash);
        else mod_dir = ".";

        std::string prev_dir = source_dir;
        auto prev_functions = functions;
        auto prev_classes = classes;
        source_dir = mod_dir;

        // Execute module in its own scope
        push_scope();
        // Register all functions (exported and internal — internals needed for module execution)
        for (Node* c : ast->children) {
            if (c->type == "func_decl")
                functions[c->value] = FuncDef{c};
            else if (c->type == "export" && !c->children.empty()
                     && c->children[0]->type == "func_decl")
                functions[c->children[0]->value] = FuncDef{c->children[0]};
        }
        for (Node* c : ast->children) exec(c);

        // Collect ONLY exported names
        std::unordered_set<std::string> exported_names;
        for (Node* c : ast->children) {
            if (c->type == "export" && !c->children.empty())
                exported_names.insert(c->children[0]->value);
        }

        // Capture exported variables from scope
        std::unordered_map<std::string, Value> exports;
        for (auto& [name, val] : scopes.back()) {
            if (exported_names.count(name))
                exports[name] = val;
        }

        // Capture ALL module functions and classes (internals needed by exported ones)
        auto mod_functions = functions;
        auto mod_classes = classes;
        pop_scope();

        source_dir = prev_dir;
        functions = prev_functions;
        classes = prev_classes;

        // Merge all module functions into the function table so exported
        // functions can call their internal helpers at runtime
        for (auto& [name, def] : mod_functions) {
            if (!prev_functions.count(name))
                functions[name] = def;
        }

        // Merge module classes
        for (auto& [name, def] : mod_classes) {
            if (!prev_classes.count(name))
                classes[name] = def;
        }

        return exports;
    }

    // import math.utils;  →  import all exported names
    void exec_import(Node* node) {
        std::string filepath = resolve_import_path(node->value);
        auto exports = run_module(filepath);
        for (auto& [name, val] : exports) {
            set_var(name, val);
        }
    }

    // from utils import add, sub;  →  import only named symbols
    void exec_from_import(Node* node) {
        std::string filepath = resolve_import_path(node->value);
        auto exports = run_module(filepath);

        for (Node* child : node->children) {
            if (child->type == "import_name") {
                const std::string& name = child->value;
                auto it = exports.find(name);
                if (it != exports.end()) {
                    set_var(name, it->second);
                }
                // If not in exports, might be a function (already merged into functions table)
            }
        }
    }

    void exec_block(Node* node) {
        push_scope();
        for (Node* c : node->children) exec(c);
        pop_scope();
    }

    void exec_if(Node* node) {
        Value cond = eval(node->children[0]);
        if (cond.truthy()) {
            exec(node->children[1]);
        } else if (node->children.size() >= 3) {
            Node* ep = node->children[2];
            if (ep->type == "if_stmt") exec_if(ep);
            else if (ep->type == "else" && !ep->children.empty()) exec(ep->children[0]);
            else exec(ep);
        }
    }

    void exec_while(Node* node) {
        while (true) {
            if (!eval(node->children[0]).truthy()) break;
            try { exec(node->children[1]); }
            catch (BreakException&) { break; }
            catch (ContinueException&) { continue; }
        }
    }

    void exec_for(Node* node) {
        push_scope();
        exec(node->children[0]);
        while (true) {
            if (!eval(node->children[1]).truthy()) break;
            try { exec(node->children[3]); }
            catch (BreakException&) { break; }
            catch (ContinueException&) {}
            eval(node->children[2]);
        }
        pop_scope();
    }

    void exec_return(Node* node) {
        Value val = Value::make_void();
        if (!node->children.empty()) val = eval(node->children[0]);
        throw ReturnException{val};
    }

    void exec_trade_block(Node* node) {
        push_scope();

        // Inject trade parameters as float variables
        for (Node* c : node->children) {
            if (c->type == "trade_params") {
                for (Node* p : c->children) {
                    if (p->type == "trade_param")
                        set_var(p->value, Value::make_float(0.0));
                }
            }
        }

        // Execute the block body
        for (Node* c : node->children) {
            if (c->type == "block") {
                for (Node* s : c->children) exec(s);
            }
        }

        pop_scope();
    }

    // ── trade block call ─────────────────────────────────────────────────

    Value call_trade(Node* trade_node, Node* args_node) {
        // Collect param names
        std::vector<std::string> param_names;
        Node* body = nullptr;
        for (Node* c : trade_node->children) {
            if (c->type == "trade_params") {
                for (Node* p : c->children)
                    if (p->type == "trade_param") param_names.push_back(p->value);
            }
            if (c->type == "block") body = c;
        }

        // Evaluate call arguments
        std::vector<Value> args;
        if (args_node)
            for (Node* a : args_node->children) args.push_back(eval(a));

        // Bind params to args
        push_scope();
        for (size_t i = 0; i < param_names.size(); i++) {
            Value val = (i < args.size()) ? args[i] : Value::make_float(0.0);
            set_var(param_names[i], val);
        }

        Value result = Value::make_void();
        try {
            if (body) for (Node* s : body->children) exec(s);
        } catch (ReturnException& ret) { result = ret.value; }
        pop_scope();
        return result;
    }

    // ── expressions ─────────────────────────────────────────────────────

    Value eval(Node* node) {
        if (!node) return Value::make_void();
        const std::string& t = node->type;

        if (t == "int_literal")    return Value::make_int(std::stoll(node->value));
        if (t == "float_literal")  return Value::make_double(std::stod(node->value));
        if (t == "string_literal") return Value::make_string(node->value);
        if (t == "char_literal")   return Value::make_char(node->value.empty() ? '\0' : node->value[0]);
        if (t == "bool_literal")   return Value::make_bool(node->value == "true");
        if (t == "list_literal")   return eval_list_literal(node);
        if (t == "map_literal")    return eval_map_literal(node);

        if (t == "identifier")     return eval_identifier(node);
        if (t == "assign")         return eval_assign(node);
        if (t == "binary_op")      return eval_binary(node);
        if (t == "unary_op")       return eval_unary(node);
        if (t == "call")           return eval_call(node);
        if (t == "new_expr")       return eval_new(node);
        if (t == "index")          return eval_index(node);
        if (t == "member_access")  return eval_member_access(node);
        if (t == "postfix_op")     return eval_postfix(node);
        if (t == "trade_action")   return eval_trade_action(node);
        if (t == "signal")         return eval_signal(node);

        return Value::make_void();
    }

    // ── new expression (object construction) ─────────────────────────────

    Value eval_new(Node* node) {
        auto it = classes.find(node->value);
        if (it == classes.end())
            throw std::runtime_error("Runtime error: undefined class '" + node->value + "'");

        ClassDef& cls = it->second;
        std::unordered_map<std::string, Value> fields;

        // Initialize fields by executing var_decls from class body
        push_scope();
        for (Node* c : cls.node->children) {
            if (c->type == "var_decl") {
                exec_var_decl(c);
                Value* v = lookup_var(c->value);
                if (v) fields[c->value] = *v;
            }
        }
        pop_scope();

        return Value::make_object(node->value, fields);
    }

    // ── container literals ──────────────────────────────────────────────

    Value eval_list_literal(Node* node) {
        std::vector<Value> v;
        for (Node* c : node->children) v.push_back(eval(c));
        return Value::make_list(v);
    }

    Value eval_map_literal(Node* node) {
        std::vector<std::pair<Value, Value>> p;
        for (Node* c : node->children)
            if (c->type == "pair" && c->children.size() >= 2)
                p.push_back({eval(c->children[0]), eval(c->children[1])});
        return Value::make_map(p);
    }

    // ── basic expression eval ───────────────────────────────────────────

    Value eval_identifier(Node* node) {
        Value* v = lookup_var(node->value);
        if (!v) throw std::runtime_error("Runtime error: undefined variable '" + node->value + "'");
        return *v;
    }

    Value eval_assign(Node* node) {
        Value val = eval(node->children[1]);
        Node* target = node->children[0];
        if (target->type == "identifier") {
            assign_var(target->value, val);
        } else if (target->type == "index") {
            assign_index(target, val);
        } else if (target->type == "member_access") {
            Node* obj_node = target->children[0];
            // Static field assignment: ClassName.field = val
            if (obj_node->type == "identifier") {
                auto cls_it = classes.find(obj_node->value);
                if (cls_it != classes.end()) {
                    auto& cls = cls_it->second;
                    auto sf_it = cls.static_fields.find(target->value);
                    if (sf_it != cls.static_fields.end()) {
                        auto acc_it = cls.access.find(target->value);
                        if (acc_it != cls.access.end())
                            check_access(obj_node->value, target->value, acc_it->second);
                        sf_it->second = val;
                        return val;
                    }
                }
            }
            Value obj = eval(obj_node);
            if (obj.kind == Value::OBJECT && obj.obj_fields) {
                // Enforce access
                auto cls_it = classes.find(obj.sval);
                if (cls_it != classes.end()) {
                    auto acc_it = cls_it->second.access.find(target->value);
                    if (acc_it != cls_it->second.access.end())
                        check_access(obj.sval, target->value, acc_it->second);
                }
                (*obj.obj_fields)[target->value] = val;
            } else {
                throw std::runtime_error("Runtime error: cannot assign to member of non-object");
            }
        }
        return val;
    }

    void assign_index(Node* idx_node, const Value& val) {
        Node* obj_node = idx_node->children[0];
        Value idx = eval(idx_node->children[1]);
        if (obj_node->type != "identifier")
            throw std::runtime_error("Runtime error: can only assign to indexed variable");
        Value* container = lookup_var(obj_node->value);
        if (!container)
            throw std::runtime_error("Runtime error: undefined variable '" + obj_node->value + "'");

        if (container->kind == Value::MAP) {
            if (!container->pairs)
                container->pairs = std::make_shared<std::vector<std::pair<Value, Value>>>();
            for (auto& [k, v] : *container->pairs) {
                if (k == idx) { v = val; return; }
            }
            container->pairs->push_back({idx, val});
        } else if (container->elems) {
            long long i = idx.as_int();
            if (i < 0 || i >= (long long)container->elems->size())
                throw std::runtime_error("Runtime error: index " + std::to_string(i) + " out of range");
            (*container->elems)[i] = val;
        }
    }

    // ── indexing ────────────────────────────────────────────────────────

    Value eval_index(Node* node) {
        Value obj = eval(node->children[0]);
        Value idx = eval(node->children[1]);

        if (obj.kind == Value::MAP) {
            if (obj.pairs)
                for (auto& [k, v] : *obj.pairs)
                    if (k == idx) return v;
            throw std::runtime_error("Runtime error: key not found in map");
        }
        if (obj.kind == Value::STRING) {
            long long i = idx.as_int();
            if (i < 0 || i >= (long long)obj.sval.size())
                throw std::runtime_error("Runtime error: string index out of range");
            return Value::make_char(obj.sval[i]);
        }
        if (obj.elems) {
            long long i = idx.as_int();
            if (i < 0 || i >= (long long)obj.elems->size())
                throw std::runtime_error("Runtime error: index " + std::to_string(i) + " out of range");
            return (*obj.elems)[i];
        }
        throw std::runtime_error("Runtime error: value is not indexable");
    }

    // ── member access (property without call) ───────────────────────────

    Value eval_member_access(Node* node) {
        Node* obj_node = node->children[0];
        const std::string& m = node->value;

        // Static access: ClassName.field
        if (obj_node->type == "identifier") {
            auto cls_it = classes.find(obj_node->value);
            if (cls_it != classes.end()) {
                auto& cls = cls_it->second;
                auto sf_it = cls.static_fields.find(m);
                if (sf_it != cls.static_fields.end()) {
                    auto acc_it = cls.access.find(m);
                    if (acc_it != cls.access.end())
                        check_access(obj_node->value, m, acc_it->second);
                    return sf_it->second;
                }
            }
        }

        Value obj = eval(obj_node);
        if (obj.kind == Value::OBJECT && obj.obj_fields) {
            auto it = obj.obj_fields->find(m);
            if (it != obj.obj_fields->end()) {
                // Enforce access
                auto cls_it = classes.find(obj.sval);
                if (cls_it != classes.end()) {
                    auto acc_it = cls_it->second.access.find(m);
                    if (acc_it != cls_it->second.access.end())
                        check_access(obj.sval, m, acc_it->second);
                }
                return it->second;
            }
        }
        if (m == "size" || m == "length") return container_size(obj);
        if (obj.kind == Value::OBJECT)
            throw std::runtime_error("Runtime error: no field '" + m + "' on " + obj.sval);
        throw std::runtime_error("Runtime error: unknown property '" + m + "'");
    }

    Value container_size(const Value& obj) {
        if (obj.kind == Value::STRING) return Value::make_int(obj.sval.size());
        if (obj.kind == Value::MAP && obj.pairs) return Value::make_int(obj.pairs->size());
        if (obj.elems) return Value::make_int(obj.elems->size());
        return Value::make_int(0);
    }

    // ── method calls (member_access as callee) ──────────────────────────

    Value call_method(Node* member_node, Node* args_node) {
        const std::string& method = member_node->value;
        Node* obj_node = member_node->children[0];
        std::string var_name;
        if (obj_node->type == "identifier") var_name = obj_node->value;

        // Evaluate args early (needed for both static and instance paths)
        std::vector<Value> args;
        if (args_node)
            for (Node* a : args_node->children) args.push_back(eval(a));

        // ── Static method dispatch (ClassName.method()) ─────────────────
        // Must come before eval(obj_node) since class names aren't variables
        if (obj_node->type == "identifier") {
            auto cls_it = classes.find(obj_node->value);
            if (cls_it != classes.end()) {
                auto& cls = cls_it->second;
                auto sm_it = cls.static_methods.find(method);
                if (sm_it != cls.static_methods.end()) {
                    // Enforce access
                    auto acc_it = cls.access.find(method);
                    if (acc_it != cls.access.end())
                        check_access(obj_node->value, method, acc_it->second);

                    Node* func = sm_it->second.node;
                    Node* params_node = nullptr;
                    Node* body = nullptr;
                    for (Node* c : func->children) {
                        if (c->type == "params") params_node = c;
                        if (c->type == "block") body = c;
                    }

                    std::vector<std::pair<std::string, Value>> bindings;
                    if (params_node) {
                        for (size_t i = 0; i < params_node->children.size(); i++) {
                            Value val = (i < args.size()) ? args[i] : Value::make_void();
                            bindings.push_back({params_node->children[i]->value, val});
                        }
                    }

                    std::string prev_class = current_class;
                    current_class = obj_node->value;
                    push_scope();
                    // Load static fields into scope
                    for (auto& [name, val] : cls.static_fields)
                        set_var(name, val);
                    for (auto& [pn, val] : bindings)
                        set_var(pn, val);

                    Value result = Value::make_void();
                    try {
                        if (body) for (Node* s : body->children) exec(s);
                    } catch (ReturnException& ret) { result = ret.value; }

                    // Write back static fields
                    for (auto& [name, fval] : cls.static_fields) {
                        Value* v = lookup_var(name);
                        if (v) fval = *v;
                    }
                    pop_scope();
                    current_class = prev_class;
                    return result;
                }
            }
        }

        Value obj = eval(obj_node);

        if (method == "size" || method == "length") return container_size(obj);

        // ── Object method dispatch ──────────────────────────────────────
        if (obj.kind == Value::OBJECT) {
            auto cls_it = classes.find(obj.sval);
            if (cls_it == classes.end())
                throw std::runtime_error("Runtime error: unknown class '" + obj.sval + "'");

            auto& cls = cls_it->second;
            auto meth_it = cls.methods.find(method);
            if (meth_it == cls.methods.end())
                throw std::runtime_error("Runtime error: unknown method '" + method + "' on class '" + obj.sval + "'");

            // Enforce access
            auto acc_it = cls.access.find(method);
            if (acc_it != cls.access.end())
                check_access(obj.sval, method, acc_it->second);

            Node* func = meth_it->second.node;
            Node* params_node = nullptr;
            Node* body = nullptr;
            for (Node* c : func->children) {
                if (c->type == "params") params_node = c;
                if (c->type == "block") body = c;
            }

            // Bind parameters
            std::vector<std::pair<std::string, Value>> bindings;
            if (params_node) {
                for (size_t i = 0; i < params_node->children.size(); i++) {
                    Value val = (i < args.size()) ? args[i] : Value::make_void();
                    bindings.push_back({params_node->children[i]->value, val});
                }
            }

            // Push scope with object fields, then params
            // Set current_class so access checks inside the method pass
            std::string prev_class = current_class;
            current_class = obj.sval;
            push_scope();
            if (obj.obj_fields)
                for (auto& [name, val] : *obj.obj_fields)
                    set_var(name, val);
            for (auto& [pn, val] : bindings)
                set_var(pn, val);

            Value result = Value::make_void();
            try {
                if (body) for (Node* s : body->children) exec(s);
            } catch (ReturnException& ret) { result = ret.value; }

            // Write modified fields back to the object
            if (obj.obj_fields)
                for (auto& [name, fval] : *obj.obj_fields) {
                    Value* v = lookup_var(name);
                    if (v) fval = *v;
                }
            pop_scope();
            current_class = prev_class;

            return result;
        }

        // ── List / Array ────────────────────────────────────────────────
        if (obj.kind == Value::LIST || obj.kind == Value::ARRAY) {
            if (!obj.elems) throw std::runtime_error("Runtime error: null container");

            if (method == "push" || method == "append" || method == "add") {
                for (auto& a : args) obj.elems->push_back(a);
                if (!var_name.empty()) assign_var(var_name, obj);
                return Value::make_void();
            }
            if (method == "pop") {
                if (obj.elems->empty()) throw std::runtime_error("Runtime error: pop from empty list");
                Value last = obj.elems->back();
                obj.elems->pop_back();
                if (!var_name.empty()) assign_var(var_name, obj);
                return last;
            }
            if (method == "insert") {
                if (args.size() < 2) throw std::runtime_error("Runtime error: insert(index, value)");
                long long i = args[0].as_int();
                if (i < 0 || i > (long long)obj.elems->size())
                    throw std::runtime_error("Runtime error: insert index out of range");
                obj.elems->insert(obj.elems->begin() + i, args[1]);
                if (!var_name.empty()) assign_var(var_name, obj);
                return Value::make_void();
            }
            if (method == "remove") {
                if (args.empty()) throw std::runtime_error("Runtime error: remove(index)");
                long long i = args[0].as_int();
                if (i < 0 || i >= (long long)obj.elems->size())
                    throw std::runtime_error("Runtime error: remove index out of range");
                Value removed = (*obj.elems)[i];
                obj.elems->erase(obj.elems->begin() + i);
                if (!var_name.empty()) assign_var(var_name, obj);
                return removed;
            }
            if (method == "contains") {
                if (args.empty()) return Value::make_bool(false);
                for (auto& el : *obj.elems) if (el == args[0]) return Value::make_bool(true);
                return Value::make_bool(false);
            }
            if (method == "clear") {
                obj.elems->clear();
                if (!var_name.empty()) assign_var(var_name, obj);
                return Value::make_void();
            }
            if (method == "get") {
                if (args.empty()) throw std::runtime_error("Runtime error: get(index)");
                long long i = args[0].as_int();
                if (i < 0 || i >= (long long)obj.elems->size())
                    throw std::runtime_error("Runtime error: index out of range");
                return (*obj.elems)[i];
            }
        }

        // ── Tuple ───────────────────────────────────────────────────────
        if (obj.kind == Value::TUPLE) {
            if (!obj.elems) throw std::runtime_error("Runtime error: null tuple");
            if (method == "get") {
                if (args.empty()) throw std::runtime_error("Runtime error: get(index)");
                long long i = args[0].as_int();
                if (i < 0 || i >= (long long)obj.elems->size())
                    throw std::runtime_error("Runtime error: tuple index out of range");
                return (*obj.elems)[i];
            }
            if (method == "contains") {
                if (args.empty()) return Value::make_bool(false);
                for (auto& el : *obj.elems) if (el == args[0]) return Value::make_bool(true);
                return Value::make_bool(false);
            }
        }

        // ── Set ─────────────────────────────────────────────────────────
        if (obj.kind == Value::SET) {
            if (!obj.elems) throw std::runtime_error("Runtime error: null set");
            if (method == "add" || method == "push") {
                for (auto& a : args) {
                    bool found = false;
                    for (auto& el : *obj.elems) if (el == a) { found = true; break; }
                    if (!found) obj.elems->push_back(a);
                }
                if (!var_name.empty()) assign_var(var_name, obj);
                return Value::make_void();
            }
            if (method == "remove") {
                if (!args.empty()) {
                    auto& e = *obj.elems;
                    e.erase(std::remove_if(e.begin(), e.end(),
                        [&](const Value& el) { return el == args[0]; }), e.end());
                    if (!var_name.empty()) assign_var(var_name, obj);
                }
                return Value::make_void();
            }
            if (method == "contains") {
                if (args.empty()) return Value::make_bool(false);
                for (auto& el : *obj.elems) if (el == args[0]) return Value::make_bool(true);
                return Value::make_bool(false);
            }
            if (method == "clear") {
                obj.elems->clear();
                if (!var_name.empty()) assign_var(var_name, obj);
                return Value::make_void();
            }
        }

        // ── Map ─────────────────────────────────────────────────────────
        if (obj.kind == Value::MAP) {
            if (!obj.pairs) throw std::runtime_error("Runtime error: null map");
            if (method == "get") {
                if (args.empty()) throw std::runtime_error("Runtime error: get(key)");
                for (auto& [k, v] : *obj.pairs) if (k == args[0]) return v;
                if (args.size() >= 2) return args[1]; // default
                throw std::runtime_error("Runtime error: key not found in map");
            }
            if (method == "set" || method == "put" || method == "add") {
                if (args.size() < 2) throw std::runtime_error("Runtime error: set(key, value)");
                for (auto& [k, v] : *obj.pairs) {
                    if (k == args[0]) { v = args[1]; if (!var_name.empty()) assign_var(var_name, obj); return Value::make_void(); }
                }
                obj.pairs->push_back({args[0], args[1]});
                if (!var_name.empty()) assign_var(var_name, obj);
                return Value::make_void();
            }
            if (method == "remove" || method == "delete") {
                if (!args.empty()) {
                    auto& p = *obj.pairs;
                    p.erase(std::remove_if(p.begin(), p.end(),
                        [&](const std::pair<Value, Value>& kv) { return kv.first == args[0]; }), p.end());
                    if (!var_name.empty()) assign_var(var_name, obj);
                }
                return Value::make_void();
            }
            if (method == "contains" || method == "has") {
                if (args.empty()) return Value::make_bool(false);
                for (auto& [k, v] : *obj.pairs) if (k == args[0]) return Value::make_bool(true);
                return Value::make_bool(false);
            }
            if (method == "keys") {
                std::vector<Value> keys;
                for (auto& [k, v] : *obj.pairs) keys.push_back(k);
                return Value::make_list(keys);
            }
            if (method == "values") {
                std::vector<Value> vals;
                for (auto& [k, v] : *obj.pairs) vals.push_back(v);
                return Value::make_list(vals);
            }
            if (method == "clear") {
                obj.pairs->clear();
                if (!var_name.empty()) assign_var(var_name, obj);
                return Value::make_void();
            }
        }

        // ── String ──────────────────────────────────────────────────────
        if (obj.kind == Value::STRING) {
            if (method == "contains") {
                if (args.empty()) return Value::make_bool(false);
                return Value::make_bool(obj.sval.find(args[0].to_string()) != std::string::npos);
            }
            if (method == "substr") {
                if (args.empty()) return obj;
                long long start = args[0].as_int();
                if (args.size() >= 2) return Value::make_string(obj.sval.substr(start, args[1].as_int()));
                return Value::make_string(obj.sval.substr(start));
            }
        }

        throw std::runtime_error("Runtime error: unknown method '" + method + "'");
    }

    // ── postfix  ──────────────────────────────────────────────────

    Value eval_postfix(Node* node) {
        Node* operand = node->children[0];
        if (operand->type != "identifier")
            throw std::runtime_error("Runtime error: ++ and -- require a variable");

        Value val = eval_identifier(operand);
        Value result = val; // return the old value (postfix semantics)

        if (node->value == "++") {
            if (val.is_integer()) assign_var(operand->value, Value::make_int(val.as_int() + 1));
            else if (val.is_numeric()) assign_var(operand->value, Value::make_double(val.as_double() + 1.0));
            else throw std::runtime_error("Runtime error: '++' requires a numeric variable");
        } else if (node->value == "--"){
            if (val.is_integer()) assign_var(operand->value, Value::make_int(val.as_int() - 1));
            else if (val.is_numeric()) assign_var(operand->value, Value::make_double(val.as_double() - 1.0));
            else throw std::runtime_error("Runtime error: '--' requires a numeric variable");
        }
        return result;
    }

    // ── binary / unary ──────────────────────────────────────────────────

    Value eval_binary(Node* node) {
        const std::string& op = node->value;

        if (op == "&&") { Value l = eval(node->children[0]); return Value::make_bool(l.truthy() && eval(node->children[1]).truthy()); }
        if (op == "||") { Value l = eval(node->children[0]); return Value::make_bool(l.truthy() || eval(node->children[1]).truthy()); }

        Value left = eval(node->children[0]);
        Value right = eval(node->children[1]);


        // container concat
        if (op == "+" && left.elems && right.elems && left.kind == right.kind) {
            std::vector<Value> combined = *left.elems;
            combined.insert(combined.end(), right.elems->begin(), right.elems->end());
            if (left.kind == Value::LIST) return Value::make_list(combined);
            if (left.kind == Value::ARRAY) return Value::make_array(combined);
            if (left.kind == Value::TUPLE) return Value::make_tuple(combined);
            return Value::make_list(combined);
        }

        // string concat
        if (op == "+" && (left.kind == Value::STRING || right.kind == Value::STRING))
            return Value::make_string(left.to_string() + right.to_string());

        // comparison
        if (op == "==" || op == "!=" || op == "<" || op == ">" || op == "<=" || op == ">=") {
            if (left.kind == Value::STRING && right.kind == Value::STRING) {
                int cmp = left.sval.compare(right.sval);
                if (op == "==") return Value::make_bool(cmp == 0);
                if (op == "!=") return Value::make_bool(cmp != 0);
                if (op == "<")  return Value::make_bool(cmp < 0);
                if (op == ">")  return Value::make_bool(cmp > 0);
                if (op == "<=") return Value::make_bool(cmp <= 0);
                return Value::make_bool(cmp >= 0);
            }
            if (left.is_numeric() && right.is_numeric()) {
                double l = left.as_double(), r = right.as_double();
                if (op == "==") return Value::make_bool(l == r);
                if (op == "!=") return Value::make_bool(l != r);
                if (op == "<")  return Value::make_bool(l < r);
                if (op == ">")  return Value::make_bool(l > r);
                if (op == "<=") return Value::make_bool(l <= r);
                return Value::make_bool(l >= r);
            }
            if (left.kind == Value::BOOL && right.kind == Value::BOOL) {
                if (op == "==") return Value::make_bool(left.bval == right.bval);
                if (op == "!=") return Value::make_bool(left.bval != right.bval);
            }
            return Value::make_bool(false);
        }

        // arithmetic
        if (left.is_numeric() && right.is_numeric()) {
            if (op == "%") return Value::make_int(left.as_int() % right.as_int());
            bool use_float = !left.is_integer() || !right.is_integer();
            if (use_float) {
                double l = left.as_double(), r = right.as_double();
                if (op == "+") return Value::make_double(l + r);
                if (op == "-") return Value::make_double(l - r);
                if (op == "*") return Value::make_double(l * r);
                if (op == "/") { if (r == 0) throw std::runtime_error("Runtime error: division by zero"); return Value::make_double(l / r); }
            } else {
                long long l = left.as_int(), r = right.as_int();
                if (op == "+") return Value::make_int(l + r);
                if (op == "-") return Value::make_int(l - r);
                if (op == "*") return Value::make_int(l * r);
                if (op == "/") { if (r == 0) throw std::runtime_error("Runtime error: division by zero"); return Value::make_int(l / r); }
            }
        }

        throw std::runtime_error("Runtime error: unsupported binary operation '" + op + "'");
    }

    Value eval_unary(Node* node) {
        Value operand = eval(node->children[0]);
        if (node->value == "!") return Value::make_bool(!operand.truthy());
        if (node->value == "-") {
            if (operand.is_integer()) return Value::make_int(-operand.as_int());
            if (operand.is_numeric()) return Value::make_double(-operand.as_double());
        }
        return operand;
    }

    // ── function / method calls ─────────────────────────────────────────

    Value eval_call(Node* node) {
        Node* callee = node->children[0];
        Node* args_node = (node->children.size() >= 2) ? node->children[1] : nullptr;

        // method call
        if (callee->type == "member_access")
            return call_method(callee, args_node);

        // built-in functions
        if (callee->type == "identifier") {
            auto bit = builtins.find(callee->value);
            if (bit != builtins.end())
                return bit->second(args_node);
        }

        std::string name = callee->value;

        // user function
        auto it = functions.find(name);
        if (it == functions.end())
            throw std::runtime_error("Runtime error: undefined function '" + name + "'");

        Node* func = it->second.node;
        std::vector<std::pair<std::string, Value>> bindings;
        Node* params_node = nullptr;
        Node* body = nullptr;
        for (Node* c : func->children) {
            if (c->type == "params") params_node = c;
            if (c->type == "block") body = c;
        }
        if (params_node && args_node) {
            for (size_t i = 0; i < params_node->children.size(); i++) {
                Value val = (i < args_node->children.size()) ?
                    eval(args_node->children[i]) : Value::make_void();
                bindings.push_back({params_node->children[i]->value, val});
            }
        }

        push_scope();
        for (auto& [pn, val] : bindings) set_var(pn, val);

        Value result = Value::make_void();
        try {
            if (body) for (Node* s : body->children) exec(s);
        } catch (ReturnException& ret) { result = ret.value; }
        pop_scope();
        return result;
    }

    Value eval_trade_action(Node* node) {
        Node* args = node->children.empty() ? nullptr : node->children[0];
        std::vector<Value> arg_vals;
        if (args) for (Node* a : args->children) arg_vals.push_back(eval(a));
        // Structured signal line: "SIGNAL <action> <index> [extras...]"
        // If no args, default the index to the current per-bar index.
        std::cout << "SIGNAL " << node->value;
        if (arg_vals.empty() && current_bar_index >= 0) {
            std::cout << " " << current_bar_index;
        } else {
            for (auto& v : arg_vals) std::cout << " " << v.to_string();
        }
        std::cout << "\n";
        return Value::make_void();
    }

    Value eval_signal(Node* node) {
        Node* args = node->children.empty() ? nullptr : node->children[0];
        std::vector<Value> arg_vals;
        if (args) for (Node* a : args->children) arg_vals.push_back(eval(a));

        // signal_line(name, index, value) — series the chart renders as an
        // overlay line. Emits "LINE <name> <index> <value>".
        if (node->value == "signal_line" && arg_vals.size() >= 3) {
            std::cout << "LINE " << arg_vals[0].to_string()
                      << " " << arg_vals[1].to_string()
                      << " " << arg_vals[2].to_string() << "\n";
            return Value::make_double(0.0);
        }

        // Default: debug print for signal_int/string/bool (and any malformed
        // signal_line call so the user sees the bad args).
        std::cout << "[" << node->value << "(";
        for (size_t i = 0; i < arg_vals.size(); i++) {
            if (i) std::cout << ", ";
            std::cout << arg_vals[i].to_string();
        }
        std::cout << ")]\n";
        if (node->value == "signal_int") return Value::make_int(0);
        if (node->value == "signal_bool") return Value::make_bool(false);
        if (node->value == "signal_line") return Value::make_double(0.0);
        return Value::make_string("");
    }
    

public:
    void run(Node* ast, const std::string& dir) {
        source_dir = dir;
        init_builtins();
        exec(ast);
    }

    // Invoke a registered trade block by name with data values
    void run_trade(const std::string& name, const std::vector<double>& data) {
        auto it = trades.find(name);
        if (it == trades.end())
            throw std::runtime_error("Runtime error: no trade block named '" + name + "'");

        Node* trade_node = it->second;
        std::vector<std::string> param_names;
        Node* body = nullptr;
        for (Node* c : trade_node->children) {
            if (c->type == "trade_params")
                for (Node* p : c->children)
                    if (p->type == "trade_param") param_names.push_back(p->value);
            if (c->type == "block") body = c;
        }

        push_scope();
        for (size_t i = 0; i < param_names.size(); i++) {
            double val = (i < data.size()) ? data[i] : 0.0;
            set_var(param_names[i], Value::make_double(val));
        }

        try {
            if (body) for (Node* s : body->children) exec(s);
        } catch (ReturnException&) {}
        pop_scope();
    }

    // Get names of all registered trade blocks
    std::vector<std::string> get_trade_names() const {
        std::vector<std::string> names;
        for (auto& [name, _] : trades) names.push_back(name);
        return names;
    }

    // Run a named trade block once per bar. Each call sees the OHLCV truncated
    // to indices [0..i] inclusive — past and current data, never future.
    // The trade body executes in a fresh scope per bar (locals don't leak),
    // but variables declared at the .hog file's TOP LEVEL persist across bars
    // so users can carry state forward (e.g. `bool prev_above;`).
    // buy()/sell() with no args record the current bar as the signal index.
    void run_with_trade_ohlcv(Node* ast, const std::string& dir,
                              const std::string& trade_name,
                              const std::vector<double>& open_v,
                              const std::vector<double>& high_v,
                              const std::vector<double>& low_v,
                              const std::vector<double>& close_v,
                              const std::vector<double>& volume_v) {
        source_dir = dir;
        init_builtins();

        // Inline exec_program's logic, but keep the program scope alive for
        // the per-bar loop so top-level vars survive between bars.
        push_scope();
        for (Node* c : ast->children) {
            if (c->type == "func_decl") functions[c->value] = FuncDef{c};
            else if (c->type == "trade_block" || c->type == "metric_block")
                trades[c->value] = c;
            else if (c->type == "export" && !c->children.empty()) {
                Node* inner = c->children[0];
                if (inner->type == "func_decl")
                    functions[inner->value] = FuncDef{inner};
                else if (inner->type == "class_decl")
                    exec_class_decl(inner);
                else if (inner->type == "trade_block" || inner->type == "metric_block")
                    trades[inner->value] = inner;
            }
        }
        for (Node* c : ast->children) exec(c);

        auto it = trades.find(trade_name);
        if (it == trades.end())
            throw std::runtime_error("Runtime error: no trade block named '" + trade_name + "'");

        Node* trade_node = it->second;
        Node* body = nullptr;
        for (Node* c : trade_node->children)
            if (c->type == "block") body = c;

        // Shared growing vectors — each iteration appends one element and the
        // Values set in scope reference the same shared_ptr, so close.size()
        // reflects the current bar index + 1.
        Value open_val   = Value::make_list({});
        Value high_val   = Value::make_list({});
        Value low_val    = Value::make_list({});
        Value close_val  = Value::make_list({});
        Value volume_val = Value::make_list({});
        // Pre-reserve to avoid reallocation invalidating shared_ptr access.
        const size_t cap = std::max({open_v.size(), high_v.size(), low_v.size(),
                                     close_v.size(), volume_v.size()});
        open_val.elems->reserve(cap);
        high_val.elems->reserve(cap);
        low_val.elems->reserve(cap);
        close_val.elems->reserve(cap);
        volume_val.elems->reserve(cap);

        const int n = (int)close_v.size();
        for (int i = 0; i < n; i++) {
            open_val.elems->push_back(Value::make_double(i < (int)open_v.size()   ? open_v[i]   : 0.0));
            high_val.elems->push_back(Value::make_double(i < (int)high_v.size()   ? high_v[i]   : 0.0));
            low_val.elems->push_back(Value::make_double(i < (int)low_v.size()    ? low_v[i]    : 0.0));
            close_val.elems->push_back(Value::make_double(i < (int)close_v.size()  ? close_v[i]  : 0.0));
            volume_val.elems->push_back(Value::make_double(i < (int)volume_v.size() ? volume_v[i] : 0.0));

            current_bar_index = i;
            push_scope();
            set_var("open",   open_val);
            set_var("high",   high_val);
            set_var("low",    low_val);
            set_var("close",  close_val);
            set_var("volume", volume_val);
            set_var("index",  Value::make_int(i));

            try {
                if (body) for (Node* s : body->children) exec(s);
            } catch (ReturnException&) {}
            pop_scope();
        }
        current_bar_index = -1;
        pop_scope();  // program scope
    }
};

// Register builtin type signatures (callable before interpretation for typechecking)
void register_builtin_types() {
    register_builtin_type("print", "void", {"auto"});
    register_builtin_type("time",  "int",  {});
    register_builtin_type("chrono", "int", {});
    register_builtin_type("pow", "double", {"double", "double"});
    register_builtin_type("sqrt", "double", {"double"});
    register_builtin_type("abs", "double", {"double"});
    // log accepts 1 or 2 args, so register as variadic auto.
    register_builtin_type("log", "double", {"auto"});
}

// ── Entry point ─────────────────────────────────────────────────────────────
void interpret(Node* ast, const std::string& source_dir) {
    Interpreter interp;
    interp.run(ast, source_dir);
}

void interpret_trade_with_ohlcv(Node* ast, const std::string& source_dir,
                                 const std::string& trade_name,
                                 const std::vector<double>& open_v,
                                 const std::vector<double>& high_v,
                                 const std::vector<double>& low_v,
                                 const std::vector<double>& close_v,
                                 const std::vector<double>& volume_v) {
    Interpreter interp;
    interp.run_with_trade_ohlcv(ast, source_dir, trade_name,
                                open_v, high_v, low_v, close_v, volume_v);
}
