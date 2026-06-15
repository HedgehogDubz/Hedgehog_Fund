// hog_native.h
//
// Public ABI for native (.cpp) modules called from .hog files.
//
// To write a native module, drop a .cpp file in User_Creations/ that
// (a) includes this header, (b) defines functions with HOG_FN, and
// (c) lists them with HOG_EXPORTS.
//
//   #include "hog_native.h"
//
//   HOG_FN(double_it) {
//       double x = ctx->arg_double(ctx, 0);
//       ctx->ret_double(ctx, x * 2.0);
//   }
//
//   HOG_EXPORTS({ "double_it" })
//
// Then in your .hog file:
//
//   import cpp my_module;
//   double y = double_it(3.0);
//
// All argument-getters and return-setters are passed by pointer through
// HogContext, so the user module never links directly against hog and the
// ABI stays stable across hog versions.

#pragma once
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct HogContext HogContext;

struct HogContext {
    // ── Arg accessors (index = 0-based) ────────────────────────────────
    int            (*arg_count)        (HogContext*);
    long long      (*arg_int)          (HogContext*, int i);
    double         (*arg_double)       (HogContext*, int i);
    const char*    (*arg_string)       (HogContext*, int i);
    // Pointer + length for list/array args. Returns NULL if arg isn't a list.
    const double*  (*arg_list_double)  (HogContext*, int i, int* out_len);
    const long long* (*arg_list_int)   (HogContext*, int i, int* out_len);

    // ── Return setters (call exactly once per function) ────────────────
    void (*ret_int)         (HogContext*, long long);
    void (*ret_double)      (HogContext*, double);
    void (*ret_string)      (HogContext*, const char*);
    void (*ret_list_double) (HogContext*, const double* data, int len);
    void (*ret_list_int)    (HogContext*, const long long* data, int len);

    // For map returns (string keys only). Call begin once, then any number
    // of set_* calls. Hog finalizes the map when the function returns.
    void (*ret_map_begin)         (HogContext*);
    void (*ret_map_set_double)    (HogContext*, const char* key, double v);
    void (*ret_map_set_int)       (HogContext*, const char* key, long long v);
    void (*ret_map_set_list_double)(HogContext*, const char* key, const double*, int len);
    void (*ret_map_set_list_int)  (HogContext*, const char* key, const long long*, int len);

    void* internal;  // opaque
};

// Each user function has signature: void(HogContext*)
typedef void (*HogFn)(HogContext*);

#ifdef __cplusplus
}
#endif

// Declare a native function. Inside the body, use ctx->arg_*(...) and
// ctx->ret_*(...).
#define HOG_FN(name) extern "C" void hog_fn_##name(HogContext* ctx)

// List the function names this module exports. Hog dlsyms `hog_fn_<name>`
// for each entry and registers it as a callable.
#define HOG_EXPORTS(...)                                              \
    extern "C" const char* const* hog_exports(int* count) {           \
        static const char* names[] = __VA_ARGS__;                     \
        *count = sizeof(names) / sizeof(names[0]);                    \
        return names;                                                 \
    }
