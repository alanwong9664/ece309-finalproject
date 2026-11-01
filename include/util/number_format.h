#pragma once
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <string>

inline std::string format_number(double v) {
    if (!std::isfinite(v)) return "null";
    if (v == 0.0) return "0";
    if (std::fabs(v) < 9007199254740992.0 && v == std::floor(v)) {
        char buf[32];
        std::snprintf(buf, sizeof buf, "%.0f", v);
        return buf;
    }
    char buf[40];
    for (int prec = 15; prec <= 17; ++prec) {
        std::snprintf(buf, sizeof buf, "%.*g", prec, v);
        if (std::strtod(buf, nullptr) == v) break;
    }
    return buf;
}
