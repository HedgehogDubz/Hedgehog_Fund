#pragma once
#include <iostream>
#include <string>
#include <vector>
#include <unordered_map>

struct BuiltinSig {
    std::string return_type;
    std::vector<std::string> param_types;
};

inline std::unordered_map<std::string, BuiltinSig>& builtin_registry() {
    static std::unordered_map<std::string, BuiltinSig> registry;
    return registry;
}

inline void register_builtin_type(const std::string& name, const std::string& return_type, const std::vector<std::string>& param_types) {
    builtin_registry()[name] = BuiltinSig{return_type, param_types};
}

struct Node {
    std::string type;
    std::string value;
    std::vector<Node*> children;
    int line = 0;
    int col = 0;

    Node(std::string t, std::string val) : type(t), value(val) {}
    Node(std::string t, std::string val, int ln, int cl) : type(t), value(val), line(ln), col(cl) {}

    void add_child(Node* child) {
        children.push_back(child);
    }

    ~Node() {
        for (Node* child : children) {
            delete child;
        }
    }

    void print(int indent = 0) {
        std::string indent_str(indent, ' ');
        std::cout << indent_str << type << " " << value << '\n';
        for (Node* child : children) {
            child->print(indent + 2);
        }
    }
};
