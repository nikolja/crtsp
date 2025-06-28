#pragma once

#ifndef __UTILS_HPP
#define __UTILS_HPP

#include <map>
#include <cmath>
#include <deque>
#include <string>
#include <vector>
#include <cstddef>
#include <sstream>
#include <utility>
#include <algorithm>
#include <string_view>
#include <type_traits>

// logging
#include <log.hpp>

namespace utils {

inline bool is_number(std::string const& val) {
   return !val.empty() && val.find_first_not_of("-.0123456789") == std::string::npos;
}

inline bool is_float(std::string const& val) {
    if (val.empty())
        return false;
    char* end = nullptr;
    std::strtod(val.c_str(), &end);
    return end == val.c_str() + val.size() && val.find('.') != std::string::npos;
}

inline bool is_int(std::string const& val) {
    if (val.empty()) return false;
    char* end = nullptr;
    std::strtol(val.c_str(), &end, 10);
    return end == val.c_str() + val.size();
}

inline bool is_unsigned(const std::string& val) {
    if (val.empty() || val[0] == '-') return false;
    char* end = nullptr;
    std::strtoul(val.c_str(), &end, 10);
    return end == val.c_str() + val.size();
}

inline bool is_unsigned_int(const std::string& val) {
    if (val.empty()) return false;
    if (val[0] == '+') return false; // не допускаємо '+'
    char* end = nullptr;
    unsigned long v = std::strtoul(val.c_str(), &end, 10);
    return end == val.c_str() + val.size();
}

inline bool to_bool(std::string const& val) {
    bool res;
    std::stringstream os;
    os << val;
    if (is_number(val))
        os >> res;
    else os >> std::boolalpha >> res;
    return res;
}

template<typename T, typename F>
inline T lex_cast(F const &val) {
    T ret;
    std::stringstream os;
    os << val;
    os >> ret;
    return ret;
}

inline bool str_exists(std::string const& str, std::string const& sub) {
    return (str.find(sub)!=std::string::npos);
}

template <typename T> std::basic_string<T> str_lower(std::basic_string<T> const& src) {
    std::basic_string<T> dst{ src };
    std::transform(dst.begin(), dst.end(), dst.begin(), [](const T v){ return static_cast<T>(std::tolower(v)); });
    return dst;
}

template <typename T> std::basic_string<T> str_upper(std::basic_string<T> const& src) {
    std::basic_string<T> dst{ src };
    std::transform(dst.begin(), dst.end(), dst.begin(), [](const T v){ return static_cast<T>(std::toupper(v)); });
    return dst;
}

inline std::vector<std::string> str_split(std::string const& input, std::string const& delimiter) {
    std::string token;
    std::size_t pos_start = 0, pos_end, delim_len = delimiter.length();
    std::vector<std::string> tokens;

    while ((pos_end = input.find(delimiter, pos_start)) != std::string::npos) {
        token = input.substr(pos_start, pos_end - pos_start);
        pos_start = pos_end + delim_len;
        tokens.push_back(token);
    }

    tokens.push_back(input.substr(pos_start));
    return tokens;
}

inline size_t str_replace(std::string& mutable_input, std::string const& look_for, std::string const& replace_with) {
    size_t occurances = 0;
    if (look_for.size() > 0) {
        size_t start = 0;
        size_t find = std::string::npos;
        do {
            find = mutable_input.find(look_for, start);
            if (find != std::string::npos) {
                mutable_input.replace(find, look_for.length(), replace_with);
                start = find + replace_with.length();
                occurances++;
            }
        } while (find != -1);
    }
    return occurances;
}

// function to get the last n lines from an input string
std::string str_last_lines(std::string const& input, size_t n) {
    std::string line;
    std::stringstream ss(input);
    std::deque<std::string> lines;
    // read all lines into the deque, keeping only the last n lines
    while (std::getline(ss, line)) {
        if (lines.size() == n) {
            lines.pop_front(); // remove the oldest line if we have more than n
        }
        lines.push_back(line);
    }
    // join the last n lines into a single string
    std::string result;
    for (auto const& l : lines) {
        result += l + "\n";
    }
    return result;
}

inline std::string_view trim(std::string_view s) {
    s.remove_prefix(std::min(s.find_first_not_of(" \t\r\v\n"), s.size()));
    s.remove_suffix(std::min(s.size() - s.find_last_not_of(" \t\r\v\n") - 1, s.size()));
    return s;
}

template<typename M>
inline bool is_map_exist(M const& m, const typename M::mapped_type &v) {
    typename M::const_iterator it = std::find_if(m.begin(), m.end(), [&v](const auto& p) { return p.second == v; });
    return it != m.end();
}

template <typename K, typename V>
inline auto get_map_pair(std::map<K, V> const& m, V const& v) {
    return std::find_if(m.begin(), m.end(), [&v](const auto& p) { return p.second == v; });
}

template <typename K, typename V>
inline auto get_map_key(std::map<K, V> const& m, V const& v) {
    auto it = get_map_pair(m, v);
    return (it != m.end()) ? it->first : K{};
}

template<typename M>
inline typename M::key_type get_map_key(M const& m, const typename M::mapped_type &v, std::string const& err_msg) {
    typename M::const_iterator it = std::find_if(m.begin(), m.end(), [&v](const auto& p) {return p.second == v; });
    if (it == m.end()) {
        if (!err_msg.empty())
            LOG_ERROR_FMT( "{}: {}", err_msg, "function arg/param is bad" );
        return typename M::key_type();
    }
    return it->first;
}

template <typename K, typename V>
inline auto get_map_value(std::map<K, V> const& m, K const& k) {
    auto it = m.find(k);
    return (it != m.end()) ? it->second : V{};
}

template<typename M>
inline typename M::mapped_type get_map_value(M const& dict, const typename M::key_type &key, std::string const& error_message) {
    typename M::const_iterator it = dict.find(key);
    if (it == dict.end()) {
        if (!error_message.empty())
            LOG_ERROR_FMT( "{}: {}", error_message, "function arg/param is bad" );
        return typename M::mapped_type();
    }
    return it->second;
}

struct size_r {
    int width;
    int height;
    bool operator==(const size_r& other) const {
        return width == other.width && height == other.height;
    }
};

inline const std::vector<size_r> size_list {
    size_r(320, 240),
    size_r(480, 360),
    size_r(640, 480),
    size_r(800, 600),
    size_r(1280, 720),
    size_r(1280, 960),
    size_r(1920, 1080),
    size_r(2560, 1440),
    size_r(3840, 2160)
};

inline const std::map<std::string, size_r> size_by_res {
    { "240p",  size_list[0] },
    { "360p",  size_list[1] },
    { "480p",  size_list[2] },
    { "svga",  size_list[3] },
    { "720p",  size_list[4] },
    { "960p",  size_list[5] },
    { "1080p", size_list[6] }, // full hd
    { "2k",    size_list[7] },
    { "4k",    size_list[8] }
    //3280x2464, 4056x3040
};

inline size_r str_to_size(std::string const& str) {
    size_r size;
    std::vector<std::string> tokens;
    if (str_exists(str, "*"))
        tokens = str_split(str, "*");
    else if (str_exists(str, "x"))
        tokens = str_split(str, "x");
    else if (str_exists(str, ","))
        tokens = str_split(str, ",");
    else if (str_exists(str, ";"))
        tokens = str_split(str, ";");
    if (tokens.size()>1) {
        size.width = lex_cast<int>(tokens[0]);
        size.height = lex_cast<int>(tokens[1]);
    } else {
        size = get_map_value(size_by_res, str);
    }
    return size;
}

inline std::string size_to_str(size_r size) {
    std::string str = get_map_key(size_by_res, size);
    if (str.empty())
        str = fmt::format("{}x{}", size.width, size.height);
    return str;
}

template<typename T>
inline T trunc_value(T value, int precision) {
    T factor = std::pow(10, precision);
    return std::trunc(value * factor) / factor;
}

} // namespace utils

#endif // #ifndef __UTILS_HPP