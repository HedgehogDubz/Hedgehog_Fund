#include "native_loader.h"
#include <dlfcn.h>
#include <sys/stat.h>
#include <filesystem>
#include <cstdio>
#include <stdexcept>
#include <sstream>

static std::string g_hog_include_dir = "Hog";

void set_hog_include_dir(const std::string& dir) { g_hog_include_dir = dir; }

static time_t mtime_or_zero(const std::string& path) {
    struct stat s{};
    if (stat(path.c_str(), &s) != 0) return 0;
    return s.st_mtime;
}

static std::string run_command_capture(const std::string& cmd, int* exit_code) {
    FILE* pipe = popen(cmd.c_str(), "r");
    if (!pipe) {
        *exit_code = -1;
        return "popen failed";
    }
    std::string out;
    char buf[4096];
    while (fgets(buf, sizeof(buf), pipe)) out += buf;
    int rc = pclose(pipe);
    *exit_code = WEXITSTATUS(rc);
    return out;
}

NativeModule load_native_module(const std::string& source_dir,
                                const std::string& module_name) {
    NativeModule mod;
    mod.name = module_name;

    std::string cpp_path = source_dir + "/" + module_name + ".cpp";
    std::string cache_dir = source_dir + "/.hog_cache";
    std::string dylib_path = cache_dir + "/" + module_name + ".dylib";

    if (!std::filesystem::exists(cpp_path)) {
        throw std::runtime_error("Native module not found: " + cpp_path);
    }
    std::filesystem::create_directories(cache_dir);

    time_t cpp_mtime = mtime_or_zero(cpp_path);
    time_t dylib_mtime = mtime_or_zero(dylib_path);
    if (cpp_mtime >= dylib_mtime) {
        std::ostringstream cmd;
        cmd << "/Library/Developer/CommandLineTools/usr/bin/clang++ "
            << "-std=c++17 -O2 -shared -fPIC "
            << "-isysroot /Library/Developer/CommandLineTools/SDKs/MacOSX.sdk "
            << "-I'" << g_hog_include_dir << "' "
            << "-o '" << dylib_path << "' "
            << "'" << cpp_path << "' 2>&1";
        int rc = 0;
        std::string output = run_command_capture(cmd.str(), &rc);
        if (rc != 0) {
            throw std::runtime_error(
                "Failed to compile native module '" + module_name + "':\n" + output);
        }
    }

    mod.handle = dlopen(dylib_path.c_str(), RTLD_NOW | RTLD_LOCAL);
    if (!mod.handle) {
        const char* err = dlerror();
        throw std::runtime_error("dlopen failed for '" + dylib_path + "': "
                                 + (err ? err : "unknown error"));
    }

    using ExportsFn = const char* const* (*)(int*);
    ExportsFn exports_fn = reinterpret_cast<ExportsFn>(dlsym(mod.handle, "hog_exports"));
    if (!exports_fn) {
        throw std::runtime_error("Native module '" + module_name +
                                 "' has no hog_exports() — did you forget HOG_EXPORTS(...)?");
    }
    int count = 0;
    const char* const* names = exports_fn(&count);
    for (int i = 0; i < count; i++) {
        if (names[i]) mod.exports.emplace_back(names[i]);
    }
    return mod;
}

void* native_module_get_fn(const NativeModule& mod, const std::string& fn_name) {
    if (!mod.handle) return nullptr;
    std::string sym = "hog_fn_" + fn_name;
    return dlsym(mod.handle, sym.c_str());
}
