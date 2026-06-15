#include "node.h"
#include <filesystem>
#include <fstream>
#include <sstream>
// Defined in compile.cpp
Node* compile(std::string code);
// Defined in interpret.cpp
void interpret(Node* ast, const std::string& source_dir);
void interpret_trade_with_ohlcv(Node* ast, const std::string& source_dir,
                                 const std::string& trade_name,
                                 const std::vector<double>& open_v,
                                 const std::vector<double>& high_v,
                                 const std::vector<double>& low_v,
                                 const std::vector<double>& close_v,
                                 const std::vector<double>& volume_v);

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
            std::cout << "Error: " << filepath << " failed to parse\n";
            // Skip files that fail to parse
        }
    }

    return -1;
}

// Locate the .hog file that defines a given trade block.
// Returns empty string if not found.
static std::string find_trade_file(const std::string& trade_name) {
    for (const auto& filepath : get_hog_files()) {
        std::ifstream f(filepath);
        if (!f.is_open()) continue;
        std::ostringstream buf; buf << f.rdbuf();
        try {
            Node* ast = compile(buf.str());
            for (Node* c : ast->children) {
                Node* tb = nullptr;
                if (c->type == "trade_block") tb = c;
                else if (c->type == "export" && !c->children.empty()
                         && c->children[0]->type == "trade_block") tb = c->children[0];
                if (tb && tb->value == trade_name) {
                    delete ast;
                    return filepath;
                }
            }
            delete ast;
        } catch (...) {}
    }
    return "";
}

// Parse a CSV file with header containing open/high/low/close/volume (case-insensitive,
// any order). Missing columns default to 0.0. Returns true on success.
static bool read_ohlcv_csv(const std::string& path,
                           std::vector<double>& open_v,
                           std::vector<double>& high_v,
                           std::vector<double>& low_v,
                           std::vector<double>& close_v,
                           std::vector<double>& volume_v) {
    std::ifstream f(path);
    if (!f.is_open()) return false;

    std::string line;
    int o = -1, h = -1, l = -1, c = -1, v = -1;
    bool first = true;
    while (std::getline(f, line)) {
        std::vector<std::string> fields;
        std::stringstream ss(line);
        std::string tok;
        while (std::getline(ss, tok, ',')) fields.push_back(tok);
        if (first) {
            for (size_t i = 0; i < fields.size(); i++) {
                std::string col = fields[i];
                for (char& ch : col) ch = (char)std::tolower((unsigned char)ch);
                if (col == "open") o = (int)i;
                else if (col == "high") h = (int)i;
                else if (col == "low") l = (int)i;
                else if (col == "close") c = (int)i;
                else if (col == "volume") v = (int)i;
            }
            first = false;
            continue;
        }
        auto safe_parse = [&](int idx) -> double {
            if (idx < 0 || idx >= (int)fields.size()) return 0.0;
            try { return std::stod(fields[idx]); } catch (...) { return 0.0; }
        };
        open_v.push_back(safe_parse(o));
        high_v.push_back(safe_parse(h));
        low_v.push_back(safe_parse(l));
        close_v.push_back(safe_parse(c));
        volume_v.push_back(safe_parse(v));
    }
    return true;
}

// hog run-trade <trade_name> <csv_path>
int run_trade_with_data(const std::string& trade_name, const std::string& csv_path) {
    std::vector<double> open_v, high_v, low_v, close_v, volume_v;
    if (!read_ohlcv_csv(csv_path, open_v, high_v, low_v, close_v, volume_v)) {
        std::cerr << "Error: cannot open data file '" << csv_path << "'\n";
        return 1;
    }

    std::string filepath = find_trade_file(trade_name);
    if (filepath.empty()) {
        std::cerr << "Error: no trade block named '" << trade_name << "' found\n";
        return 1;
    }

    std::ifstream f(filepath);
    std::ostringstream buf; buf << f.rdbuf();

    try {
        Node* ast = compile(buf.str());
        interpret_trade_with_ohlcv(ast, "User_Creations", trade_name,
                                   open_v, high_v, low_v, close_v, volume_v);
        delete ast;
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }
    return 0;
}

