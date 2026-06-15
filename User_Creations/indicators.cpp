// User-side native indicators module. Drop in User_Creations/.
// `import cpp indicators;` in a .hog file compiles this to a dylib (cached
// in User_Creations/.hog_cache/) and dlopens it.

#include "hog_native.h"
#include <vector>

// rolling_ma(series: List<double>, window: int) -> List<double>
HOG_FN(rolling_ma) {
    int n = 0;
    const double* in = ctx->arg_list_double(ctx, 0, &n);
    int window = (int)ctx->arg_int(ctx, 1);

    std::vector<double> out(n, 0.0);
    if (window > 0 && n > 0) {
        double sum = 0.0;
        for (int i = 0; i < n; i++) {
            sum += in[i];
            if (i >= window) sum -= in[i - window];
            if (i >= window - 1) out[i] = sum / (double)window;
        }
    }
    ctx->ret_list_double(ctx, out.data(), (int)out.size());
}

// rolling_ma_crossover(series: List<double>, fast: int, slow: int) -> Map
//   "fast": List<double>, "slow": List<double>,
//   "buy":  List<int>,    "sell": List<int>
HOG_FN(rolling_ma_crossover) {
    int n = 0;
    const double* in = ctx->arg_list_double(ctx, 0, &n);
    int fast = (int)ctx->arg_int(ctx, 1);
    int slow = (int)ctx->arg_int(ctx, 2);

    auto ma = [&](int window) {
        std::vector<double> out(n, 0.0);
        if (window <= 0 || n == 0) return out;
        double sum = 0.0;
        for (int i = 0; i < n; i++) {
            sum += in[i];
            if (i >= window) sum -= in[i - window];
            if (i >= window - 1) out[i] = sum / (double)window;
        }
        return out;
    };

    std::vector<double> fast_ma = ma(fast);
    std::vector<double> slow_ma = ma(slow);
    std::vector<long long> buys, sells;

    if (slow > 0 && fast > 0 && n > 0) {
        double prev_diff = 0.0;
        bool armed = false;
        for (int i = slow - 1; i < n; i++) {
            double diff = fast_ma[i] - slow_ma[i];
            if (armed) {
                if (diff > 0.0 && prev_diff <= 0.0) buys.push_back(i);
                else if (diff < 0.0 && prev_diff >= 0.0) sells.push_back(i);
            }
            prev_diff = diff;
            armed = true;
        }
    }

    ctx->ret_map_begin(ctx);
    ctx->ret_map_set_list_double(ctx, "fast", fast_ma.data(), (int)fast_ma.size());
    ctx->ret_map_set_list_double(ctx, "slow", slow_ma.data(), (int)slow_ma.size());
    ctx->ret_map_set_list_int   (ctx, "buy",  buys.data(),    (int)buys.size());
    ctx->ret_map_set_list_int   (ctx, "sell", sells.data(),   (int)sells.size());
}

HOG_EXPORTS({ "rolling_ma", "rolling_ma_crossover" })
