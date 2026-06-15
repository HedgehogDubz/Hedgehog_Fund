#include "node.h"
#include "native_loader.h"
#include <fstream>
#include <sstream>

// Defined in compile.cpp
Node* compile(std::string code, const std::string& source_dir);
// Defined in interpret.cpp
void interpret(Node* ast, const std::string& source_dir);
// Defined in trade.cpp
std::vector<std::string> get_trades();
int run_trade_with_data(const std::string& trade_name, const std::string& csv_path);

static void print_usage() {
    std::cerr << "Usage: hog <command> [filename.hog]\n";
    std::cerr << "Commands:\n";
    std::cerr << "  build                          Compile and type-check only\n";
    std::cerr << "  run                            Compile, type-check, and execute\n";
    std::cerr << "  build-run                      Same as run (compile + execute)\n";
    std::cerr << "  trades                         List all trade blocks across .hog files\n";
    std::cerr << "  run-trade <name> <csv>         Run a trade block with OHLCV data\n";
}

// Extract directory from a file path
static std::string dir_of(const std::string& filepath) {
    size_t pos = filepath.find_last_of("/\\");
    if (pos == std::string::npos) return ".";
    return filepath.substr(0, pos);
}

int main(int argc, char* argv[]) {
    // Tell native_loader where hog_native.h lives so user .cpp modules can
    // #include it. Derived from argv[0] — the dir containing the hog binary.
    if (argc >= 1) {
        std::string self = argv[0];
        size_t slash = self.find_last_of("/\\");
        set_hog_include_dir(slash == std::string::npos ? "." : self.substr(0, slash));
    }

    if (argc < 2) {
        print_usage();
        return 1;
    }

    std::string command = argv[1];

    // "hog trades" — no filename needed
    if (command == "trades") {
        try {
            auto trades = get_trades();
            for (auto& name : trades)
                std::cout << name << "\n";
        } catch (const std::exception& e) {
            std::cerr << "Error: " << e.what() << "\n";
            return 1;
        }
        return 0;
    }

    // "hog run-trade <trade_name> <csv_path>"
    if (command == "run-trade") {
        if (argc < 4) { print_usage(); return 1; }
        return run_trade_with_data(argv[2], argv[3]);
    }

    if (argc < 3) {
        print_usage();
        return 1;
    }

    std::string filename = argv[2];

    if (command != "build" && command != "run" && command != "build-run") {
        std::cerr << "Error: unknown command '" << command << "'\n";
        print_usage();
        return 1;
    }

    std::ifstream file(filename);
    if (!file.is_open()) {
        std::cerr << "Error: cannot open '" << filename << "'\n";
        return 1;
    }

    std::ostringstream buf;
    buf << file.rdbuf();
    std::string code = buf.str();

    try {
        std::string src_dir = dir_of(filename);
        Node* ast = compile(code, src_dir);

        if (command == "build") {
            std::cout << "Build Complete\n";
        } else {
            interpret(ast, src_dir);
        }

        delete ast;
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }

    return 0;
}
