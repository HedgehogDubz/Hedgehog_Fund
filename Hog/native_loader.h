#pragma once
#include <string>
#include <vector>

struct NativeModule {
    void* handle = nullptr;
    std::string name;
    std::vector<std::string> exports;
};

// Where hog_native.h lives. Defaults to "Hog"; hog.cpp sets it from argv[0].
void set_hog_include_dir(const std::string& dir);

// Compile (if stale) and dlopen <source_dir>/<module_name>.cpp.
// Returns a module with .handle != nullptr on success; throws on failure.
NativeModule load_native_module(const std::string& source_dir,
                                const std::string& module_name);

// dlsym for `hog_fn_<fn_name>`. Returns nullptr if not found.
void* native_module_get_fn(const NativeModule& mod, const std::string& fn_name);
