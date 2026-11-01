#pragma once
#include <cstdio>
#include <exception>
#include <functional>
#include <map>
#include <sstream>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

namespace mt {

struct TestCase {
    const char*           name;
    std::function<void()> fn;
};

inline std::vector<TestCase>& registry() {
    static std::vector<TestCase> tests;
    return tests;
}

struct Registrar {
    Registrar(const char* name, std::function<void()> fn) {
        registry().push_back({name, std::move(fn)});
    }
};

struct AbortTest {};

inline int& failures_in_current() {
    static int n = 0;
    return n;
}

inline void report(const char* file, int line, const std::string& what) {
    ++failures_in_current();
    std::printf("    %s:%d: failed: %s\n", file, line, what.c_str());
}

template <typename T, typename = void>
struct printable : std::false_type {};
template <typename T>
struct printable<T, std::void_t<decltype(std::declval<std::ostream&>() << std::declval<const T&>())>>
    : std::true_type {};

template <typename T> void show(std::ostream& os, const T& v);
template <typename T> void show(std::ostream& os, const std::vector<T>& v);
template <typename A, typename B> void show(std::ostream& os, const std::pair<A, B>& p);
inline void show(std::ostream& os, const std::string& s) { os << '"' << s << '"'; }

template <typename T> void show(std::ostream& os, const std::vector<T>& v) {
    os << "{";
    for (std::size_t i = 0; i < v.size(); ++i) {
        if (i) os << ", ";
        if (i == 20) { os << "... (" << v.size() << " items)"; break; }
        show(os, v[i]);
    }
    os << "}";
}
template <typename A, typename B> void show(std::ostream& os, const std::pair<A, B>& p) {
    os << "(";
    show(os, p.first);
    os << ", ";
    show(os, p.second);
    os << ")";
}
template <typename T> void show(std::ostream& os, const T& v) {
    if constexpr (std::is_enum_v<T>) os << static_cast<long long>(v);
    else if constexpr (printable<T>::value) os << v;
    else os << "(value not printable)";
}

template <typename A, typename B>
std::string describe_eq(const char* ea, const char* eb, const A& a, const B& b) {
    std::ostringstream os;
    os << ea << " == " << eb << "\n        left:  ";
    show(os, a);
    os << "\n        right: ";
    show(os, b);
    return os.str();
}

inline int run_all(int argc, char** argv) {
    const std::string filter = argc > 1 ? argv[1] : "";
    if (filter == "--list") {
        for (const TestCase& t : registry()) std::printf("%s\n", t.name);
        return 0;
    }
    const bool exact = !filter.empty() && filter[0] == '=';
    std::map<std::string, std::pair<int, int>> by_area;
    int passed = 0, total = 0;

    for (const TestCase& t : registry()) {
        const std::string name = t.name;
        if (exact ? name != filter.substr(1)
                  : (!filter.empty() && name.compare(0, filter.size(), filter) != 0))
            continue;
        ++total;
        failures_in_current() = 0;
        std::string crash;
        try {
            t.fn();
        } catch (const AbortTest&) {
        } catch (const std::exception& e) {
            crash = std::string("uncaught exception: ") + e.what();
        } catch (...) {
            crash = "uncaught non-std exception";
        }
        if (!crash.empty()) {
            ++failures_in_current();
            std::printf("    %s\n", crash.c_str());
        }
        const bool ok = failures_in_current() == 0;
        std::printf("[%s] %s\n", ok ? "PASS" : "FAIL", t.name);
        std::fflush(stdout);

        const std::string area = name.substr(0, name.find('_'));
        auto& slot = by_area[area];
        slot.second += 1;
        if (ok) { ++passed; slot.first += 1; }
    }

    std::printf("\n---- summary ----\n");
    for (const auto& kv : by_area)
        std::printf("  %-12s %3d / %3d\n", kv.first.c_str(), kv.second.first, kv.second.second);
    std::printf("  %-12s %3d / %3d\n", "TOTAL", passed, total);
    if (total == 0) {
        std::printf("no tests matched '%s'\n", filter.c_str());
        return 1;
    }
    return passed == total ? 0 : 1;
}

}

#define MT_CAT2(a, b) a##b
#define MT_CAT(a, b) MT_CAT2(a, b)

#define TEST(name)                                                    \
    static void name();                                               \
    static ::mt::Registrar MT_CAT(mt_reg_, name)(#name, &name);       \
    static void name()

#define CHECK(cond)                                                   \
    do {                                                              \
        if (!(cond)) ::mt::report(__FILE__, __LINE__, #cond);         \
    } while (0)

#define REQUIRE(cond)                                                 \
    do {                                                              \
        if (!(cond)) {                                                \
            ::mt::report(__FILE__, __LINE__, #cond);                  \
            throw ::mt::AbortTest{};                                  \
        }                                                             \
    } while (0)

#define CHECK_EQ(a, b)                                                \
    do {                                                              \
        const auto& mt_a_ = (a);                                      \
        const auto& mt_b_ = (b);                                      \
        if (!(mt_a_ == mt_b_))                                        \
            ::mt::report(__FILE__, __LINE__,                          \
                         ::mt::describe_eq(#a, #b, mt_a_, mt_b_));    \
    } while (0)

#define CHECK_THROWS(expr, ex)                                        \
    do {                                                              \
        bool mt_threw_ = false;                                       \
        try { (void)(expr); } catch (const ex&) { mt_threw_ = true; } \
        catch (...) {}                                                \
        if (!mt_threw_)                                               \
            ::mt::report(__FILE__, __LINE__, #expr " throws " #ex);   \
    } while (0)
