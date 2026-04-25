#include "node.h"
#include <filesystem>
#include <fstream>
#include <sstream>
#include
// Defined in compile.cpp
Node* compile(std::string code);

std::vector<std::string> get_hog_files() {
    std::vector<std::string> hog_files;
    for (const auto& entry : std::filesystem::directory_iterator("User_Creations")) {
        if (entry.path().extension() == ".hog") {
            hog_files.push_back(entry.path().string());
        }
    }
    return hog_files;
}

std::vector<std::string> get_trades() {
    std::vector<std::string> trades;
    std::vector<std::string> hog_files = get_hog_files();

    for (const auto& filepath : hog_files) {
        std::ifstream f(filepath);
        if (!f.is_open()) continue;
        std::ostringstream buf;
        buf << f.rdbuf();

        try {
            Node* ast = compile(buf.str());

            // Walk top-level nodes looking for trade_block (including exported ones)
            for (Node* c : ast->children) {
                if (c->type == "trade_block")
                    trades.push_back(c->value);
                else if (c->type == "export" && !c->children.empty()
                         && c->children[0]->type == "trade_block")
                    trades.push_back(c->children[0]->value);
            }

            delete ast;
        } catch (...) {
            // Skip files that fail to parse
        }
    }

    return trades;
}
int run_trade(std::string trade_name){
    std::vector<std::string> hog_files = get_hog_files();
    for (const auto& filepath : hog_files) {
        std::ifstream f(filepath);
        if (!f.is_open()) continue;
        std::ostringstream buf;
        buf << f.rdbuf();

        try {
            Node* ast = compile(buf.str());

            // Walk top-level nodes looking for trade_block (including exported ones)
            for (Node* c : ast->children) {
                if (c->type == "trade_block" && c->value == trade_name){
                    interpret(ast, "User_Creations");
                    delete ast;
                    return 0;
                }
            }

            delete ast;
        } catch (...) {

            // Skip files that fail to parse
        }
    }

    return 1;
}
