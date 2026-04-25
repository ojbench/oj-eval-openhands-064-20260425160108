
#pragma once
#include <array>
#include <cmath>
#include <concepts>
#include <cstddef>
#include <exception>
#include <iostream>
#include <ostream>
#include <span>
#include <string_view>
#include <string>
#include <vector>
#include <type_traits>
#include <cstdint>

namespace sjtu {

using sv_t = std::string_view;

struct format_error : std::exception {
public:
    format_error(const char *msg = "invalid format") : msg(msg) {}
    auto what() const noexcept -> const char * override {
        return msg;
    }

private:
    const char *msg;
};

template <typename Tp>
struct formatter;

struct format_info {
    inline static constexpr auto npos = static_cast<std::size_t>(-1);
    std::size_t position; // where is the specifier
    std::size_t consumed; // how many characters consumed
};

template <typename... Args>
struct format_string {
public:
    // must be constructed at compile time, to ensure the format string is valid
    consteval format_string(const char *fmt);
    constexpr auto get_format() const -> std::string_view {
        return fmt_str;
    }
    constexpr auto get_index() const -> std::span<const format_info> {
        return fmt_idx;
    }

private:
    inline static constexpr auto Nm = sizeof...(Args);
    std::string_view fmt_str;            // the format string
    std::array<format_info, Nm> fmt_idx; // where are the specifiers
};

consteval auto find_specifier(sv_t &fmt) -> bool {
    do {
        if (const auto next = fmt.find('%'); next == sv_t::npos) {
            return false;
        } else if (next + 1 == fmt.size()) {
            throw format_error{"missing specifier after '%'"};
        } else if (fmt[next + 1] == '%') {
            // escape the specifier
            fmt.remove_prefix(next + 2);
        } else {
            fmt.remove_prefix(next + 1);
            return true;
        }
    } while (true);
};

template <typename Tp>
consteval auto parse_one(sv_t &fmt, format_info &info) -> void {
    const auto last_pos = fmt.data();
    if (!find_specifier(fmt)) {
        throw format_error{"too few specifiers"};
    }

    const auto position = static_cast<std::size_t>(fmt.data() - last_pos - 1);
    const auto consumed = formatter<Tp>::parse(fmt);

    info = {
        .position = position,
        .consumed = consumed,
    };

    if (consumed > 0) {
        fmt.remove_prefix(consumed);
    } else {
        throw format_error{"invalid specifier"};
    }
}

template <typename... Args>
consteval auto compile_time_format_check(sv_t fmt, std::span<format_info> idx) -> void {
    std::size_t n = 0;
    (parse_one<Args>(fmt, idx[n++]), ...);
    if (find_specifier(fmt)) // extra specifier found
        throw format_error{"too many specifiers"};
}

template <typename... Args>
consteval format_string<Args...>::format_string(const char *fmt) :
    fmt_str(fmt), fmt_idx() {
    compile_time_format_check<Args...>(fmt_str, fmt_idx);
}

// Formatter for strings
template <typename StrLike>
    requires(
        std::same_as<StrLike, std::string> ||      
        std::same_as<StrLike, std::string_view> || 
        std::same_as<StrLike, char *> ||           
        std::same_as<StrLike, const char *> ||
        std::is_array_v<StrLike> // for char[]
    )
struct formatter<StrLike> {
    static constexpr auto parse(sv_t fmt) -> std::size_t {
        if (fmt.starts_with("s")) return 1;
        if (fmt.starts_with("_")) return 1;
        return 0;
    }
    static auto format_to(std::ostream &os, const StrLike &val, sv_t fmt) -> void {
        if (fmt.starts_with("s") || fmt.starts_with("_")) {
            os << val;
        } else {
            throw format_error{};
        }
    }
};

// Formatter for integers
template <typename IntType>
    requires(std::is_integral_v<IntType> && !std::same_as<IntType, bool> && !std::same_as<IntType, char>)
struct formatter<IntType> {
    static constexpr auto parse(sv_t fmt) -> std::size_t {
        if (fmt.starts_with("d")) return 1;
        if (fmt.starts_with("u")) return 1;
        if (fmt.starts_with("_")) return 1;
        return 0;
    }
    static auto format_to(std::ostream &os, const IntType &val, sv_t fmt) -> void {
        if (fmt.starts_with("d")) {
            if constexpr (std::is_signed_v<IntType>) {
                os << static_cast<int64_t>(val);
            } else {
                os << static_cast<int64_t>(static_cast<uint64_t>(val));
            }
        } else if (fmt.starts_with("u")) {
            os << static_cast<uint64_t>(val);
        } else if (fmt.starts_with("_")) {
            if constexpr (std::is_signed_v<IntType>) {
                os << static_cast<int64_t>(val);
            } else {
                os << static_cast<uint64_t>(val);
            }
        } else {
            throw format_error{};
        }
    }
};

// Formatter for vectors
template <typename T>
struct formatter<std::vector<T>> {
    static constexpr auto parse(sv_t fmt) -> std::size_t {
        if (fmt.starts_with("_")) return 1;
        return 0;
    }
    static auto format_to(std::ostream &os, const std::vector<T> &val, sv_t fmt) -> void {
        if (fmt.starts_with("_")) {
            os << "[";
            for (std::size_t i = 0; i < val.size(); ++i) {
                formatter<T>::format_to(os, val[i], "_");
                if (i + 1 < val.size()) os << ",";
            }
            os << "]";
        } else {
            throw format_error{};
        }
    }
};

template <typename... Args>
using format_string_t = format_string<std::decay_t<Args>...>;

template <typename... Args>
inline auto printf(format_string_t<Args...> fmt, const Args &...args) -> void {
    sv_t fmt_str = fmt.get_format();
    std::size_t arg_idx = 0;
    
    auto process_args = [&](auto&... args_list) {
        std::size_t i = 0;
        while (i < fmt_str.size()) {
            if (fmt_str[i] == '%') {
                if (i + 1 < fmt_str.size() && fmt_str[i + 1] == '%') {
                    std::cout << '%';
                    i += 2;
                } else {
                    // This must be a specifier.
                    // We use a fold expression to find the right argument.
                    std::size_t current_arg = 0;
                    auto format_one_arg = [&](const auto& arg) {
                        if (current_arg == arg_idx) {
                            i++; // skip '%'
                            sv_t sub = fmt_str.substr(i);
                            std::size_t consumed = formatter<std::decay_t<decltype(arg)>>::parse(sub);
                            formatter<std::decay_t<decltype(arg)>>::format_to(std::cout, arg, sub.substr(0, consumed));
                            i += consumed;
                        }
                        current_arg++;
                    };
                    (format_one_arg(args_list), ...);
                    arg_idx++;
                }
            } else {
                std::cout << fmt_str[i];
                i++;
            }
        }
    };
    
    process_args(args...);
}

} // namespace sjtu
