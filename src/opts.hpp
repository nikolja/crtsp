#pragma once

#ifndef __OPTS_HPP
#define __OPTS_HPP

#include <map>
#include <vector>
#include <string>
#include <sstream>
#include <fstream>
#include <iostream>
#include <optional>
#include <algorithm>
#include <filesystem>

// cxxopts
#include <cxxopts.hpp>
// nlohmann_json
#include <nlohmann/json.hpp>

// logging
#include <log.hpp>

namespace opts {

template <typename>
inline constexpr bool always_false = false;

template <typename T>
void add_option(cxxopts::Options& options, std::string const& key, std::string const& desc, T const& def_value) {
    using namespace cxxopts;
    if constexpr (std::is_same_v<T, bool>) {
        options.add_options()(key, desc, value<bool>()->default_value(def_value ? "true" : "false"));
    } else if constexpr (std::is_same_v<T, int>) {
        options.add_options()(key, desc, value<int>()->default_value(std::to_string(def_value)));
    } else if constexpr (std::is_same_v<T, float>) {
        options.add_options()(key, desc, value<float>()->default_value(std::to_string(def_value)));
    } else if constexpr (std::is_same_v<T, double>) {
        options.add_options()(key, desc, value<double>()->default_value(std::to_string(def_value)));
    } else if constexpr (std::is_same_v<T, std::string>) {
        options.add_options()(key, desc, value<std::string>()->default_value(def_value));
    } else {
        //static_assert(always_false<T>, "unsupported type for add_option");
        LOG_WARNING_FMT( "unsupported type for add_option for key: {}", key );
    }
}

std::optional<cxxopts::HelpOptionDetails> get_option_detail(cxxopts::Options const& options, std::string const& key) {
    for (const auto& group : options.groups()) {
        const auto& help_group = options.group_help(group);
        for (const auto& opt : help_group.options) {
            if (!opt.l.empty() && std::find(opt.l.begin(), opt.l.end(), key) != opt.l.end())
                return opt;
            if (!opt.s.empty() && opt.s == key)
                return opt;
        }
    }
    return std::nullopt;
}

bool is_option_exists(cxxopts::Options const& options, std::string const& key) {
    for (const auto& group : options.groups()) {
        const auto& help_group = options.group_help(group);
        for (const auto& opt : help_group.options) {
            if (!opt.l.empty() && std::find(opt.l.begin(), opt.l.end(), key) != opt.l.end())
                return true;
            if (!opt.s.empty() && opt.s == key)
                return true;
        }
    }
    return false;
}

class parser {
public:
    explicit parser(int argc, const char* const argv[], std::string program = "app", std::string help = ""): name_app(std::move(program)), options(name_app, help) {
        origin_args.assign(argv, argv + argc);
        merged_args = origin_args;
    }

    cxxopts::Options& get_options() { return options; }

    auto add_options(const std::string& group = "") {
        return options.add_options(group);
    }

    void add_default(std::string conf_def = "") {
        add_options()
            ("config", "load arguments from JSON", cxxopts::value<std::string>()->default_value(conf_def))
            ("save_args", "save arguments to JSON", cxxopts::value<std::string>()->default_value(""))
            ("save_opts", "save options to JSON", cxxopts::value<std::string>()->default_value(""))
            ("help", "print help");
    }

    bool do_default() {
        bool res = false;
        if (result.count("help")) {
            LOG_INFO_FMT( options.help() );
            return true;
        }
        if (result.count("save_args")) {
            std::string save_path = result["save_args"].as<std::string>();
            if (!save_path.empty()) {
                if (!to_file(save_path)) {
                    LOG_ERROR_FMT( "failed to save arguments to file: {}", save_path );
                }
                res = true;
            }
        }
        if (result.count("save_opts")) {
            std::string save_path = result["save_opts"].as<std::string>();
            if (!save_path.empty()) {
                if (!to_file(save_path, false)) {
                    LOG_ERROR_FMT( "failed to save options to file: {}", save_path );
                }
                res = true;
            }
        }
        return res;
    }

    template <typename T>
    void add_option(std::string const& key, std::string const& desc, T const& def_value) {
        add_option(options, key, desc, def_value);
    }

    bool parse() {
        auto result_pre = options.parse(static_cast<int>(merged_args.size()), const_cast<char**>(to_c_argv(merged_args)));
        if (result_pre.count("config")) {
            std::string config_path = result_pre["config"].as<std::string>();
            if (!config_path.empty()) {
                if (std::filesystem::exists(config_path)) {
                    std::ifstream file(config_path);
                    nlohmann::json j;
                    try {
                        file >> j;
                        from_json(j);
                    } catch (const std::exception& e) {
                        LOG_ERROR_FMT( "failed to parse JSON config from {}: {}", config_path, e.what());
                        //return false;
                    }
                } else {
                    LOG_ERROR_FMT( "config file not found: {}", config_path );
                }
            }
        }
        result = options.parse(static_cast<int>(merged_args.size()), const_cast<char**>(to_c_argv(merged_args)));
        //do_default();
        return true;
    }

    template <typename T>
    void set(std::string const& key, T const& value) {
        std::ostringstream oss; oss << value;
        std::string kv = "--" + key + "=" + oss.str();
        auto it = std::find_if(merged_args.begin(), merged_args.end(), [&](const std::string& s) {
            return s.rfind("--" + key + "=", 0) == 0;
        });
        if (it != merged_args.end()) {
            *it = kv;
        } else {
            merged_args.push_back(kv);
        }
        // re-parse after setting
        result = options.parse(static_cast<int>(merged_args.size()), const_cast<char**>(to_c_argv(merged_args)));
    }

    inline auto const help() const {
        return options.help();
    }

    inline bool has(std::string const& key) const {
        return result.count(key) > 0;
    }

    inline cxxopts::ParseResult& get_parsed() {
        return result;
    }

    inline cxxopts::ParseResult const& get_result() const {
        return result;
    }

    inline std::vector<std::string> const& get_args() const {
        return merged_args;
    }

    inline bool const is_key_valid(std::string const& key) const {
        return is_option_exists(options, key);
    }

    inline bool const is_key_default(std::string const& key) const {
        return key == "help" || key == "config" || key == "save_opts" || key == "save_args";
    }

    std::map<std::string, std::string> to_map(bool just_result = true) const {
        std::map<std::string, std::string> result_map;
        if (just_result) {
            for (auto const& kv : result.arguments()) {
                std::string const& key = kv.key();
                if (key.empty() || is_key_default(key))
                    continue;
                try { result_map[key] = result[key].as<std::string>(); continue; } catch (...) {}
                try { result_map[key] = std::to_string(result[key].as<int>()); continue; } catch (...) {}
                try { result_map[key] = std::to_string(result[key].as<float>()); continue; } catch (...) {}
                try { result_map[key] = std::to_string(result[key].as<double>()); continue; } catch (...) {}
                try { result_map[key] = result[key].as<bool>() ? "true" : "false"; continue; } catch (...) {}
                result_map[key] = "<unknown>";
            }
        } else {
            for (const auto& group : options.groups()) {
                const auto& help_group = options.group_help(group);
                for (const auto& opt : help_group.options) {
                    // get a long name if exists, otherwise short
                    std::string key;
                    if (!opt.l.empty())
                        key = opt.l[0];
                    else if (!opt.s.empty())
                        key = opt.s;
                    else
                        continue;
                    if (is_key_default(key))
                        continue;
                    // if option exists in CLI - read value from result
                    if (result.count(key)) {
                        try { result_map[key] = result[key].as<std::string>(); continue; } catch (...) {}
                        try { result_map[key] = std::to_string(result[key].as<int>()); continue; } catch (...) {}
                        try { result_map[key] = std::to_string(result[key].as<float>()); continue; } catch (...) {}
                        try { result_map[key] = std::to_string(result[key].as<double>()); continue; } catch (...) {}
                        try { result_map[key] = result[key].as<bool>() ? "true" : "false"; continue; } catch (...) {}
                        result_map[key] = "<unknown>";
                    }
                    // otherwise - fallback to default_value
                    else if (opt.has_default) {
                        const std::string& def_val = opt.default_value;
                        if (opt.is_boolean) {
                            result_map[key] = (def_val == "true") ? "true" : "false";
                        } else {
                            result_map[key] = def_val;
                        }
                    } else {
                        result_map[key] = nullptr; 
                    }
                }
            }
        }
        return result_map;
    }

    nlohmann::json to_json(bool just_result = true) const {
        nlohmann::json j;
        if (just_result) {
            for (const auto& kv : result.arguments()) { //for (const auto& [k, v] : to_map()) j[k] = v;
                std::string const& key = kv.key();
                if (key.empty() || is_key_default(key))
                    continue;            
                try { j[key] = result[key].as<bool>(); continue; } catch (...) {}
                try { j[key] = result[key].as<int>(); continue; } catch (...) {}
                try { j[key] = result[key].as<double>(); continue; } catch (...) {}
                try { j[key] = result[key].as<std::string>(); continue; } catch (...) {}
                j[key] = nullptr;
            }
        } else {
            for (const auto& group : options.groups()) {
                const auto& help_group = options.group_help(group);
                for (const auto& opt : help_group.options) {
                    // get a long name if exists, otherwise short
                    std::string key;
                    if (!opt.l.empty())
                        key = opt.l[0];
                    else if (!opt.s.empty())
                        key = opt.s;
                    else
                        continue;
                    if (is_key_default(key))
                        continue;
                    // if option exists in CLI - read value from result
                    if (result.count(key)) {
                        try { j[key] = result[key].as<bool>(); continue; } catch (...) {}
                        try { j[key] = result[key].as<int>(); continue; } catch (...) {}
                        try { j[key] = result[key].as<double>(); continue; } catch (...) {}
                        try { j[key] = result[key].as<std::string>(); continue; } catch (...) {}
                        j[key] = nullptr;
                    }
                    // otherwise - fallback to default_value
                    else if (opt.has_default) {
                        const std::string& def_val = opt.default_value;
                        if (opt.is_boolean) {
                            j[key] = (def_val == "true");
                        } else {
                            // try to auto-detect type
                            if (utils::is_number(def_val)) {
                                if (utils::is_int(def_val)) {
                                    try { j[key] = std::stoi(def_val); continue; } catch (...) {}
                                } else if (utils::is_float(def_val)) {
                                    try { j[key] = std::stod(def_val); continue; } catch (...) {}
                                }
                            }
                            j[key] = def_val;
                        }
                    } else {
                        j[key] = nullptr;
                    }
                }
            }
        }
        return j;
    }

    bool to_file(std::string const& path, bool just_result = true) const {
        std::ofstream file(path);
        if (!file.is_open()) {
            LOG_ERROR_FMT( "failed to open file for writing: {}", path );
            return false;
        }
        file << to_json(just_result).dump(4); // pretty print with 4 spaces
        file.close();
        //LOG_INFO_FMT( "options saved to file: {}", path );
        return true;
    }

private:

    void from_json(nlohmann::json const& j, bool cli_overrides = true) {
        for (auto it = j.begin(); it != j.end(); ++it) {
            std::string const& key = it.key();
            // skip if key already in CLI
            bool already_in_cli = std::any_of(merged_args.begin(), merged_args.end(), [&](const std::string& s) {
                return s.rfind("--" + key + "=", 0) == 0;
            });
            if (already_in_cli && cli_overrides)
                continue;
            // otherwise set from JSON
            if (it.value().is_boolean()) {
                set(key, it.value().get<bool>() ? "true" : "false");
            } else if (it.value().is_number_integer()) {
                set(key, std::to_string(it.value().get<int>()));
            } else if (it.value().is_number_unsigned()) {
                set(key, std::to_string(it.value().get<unsigned>()));
            } else if (it.value().is_number_float()) {
                set(key, std::to_string(it.value().get<double>()));
            } else if (it.value().is_string()) {
                set(key, it.value().get<std::string>());
            } else if (it.value().is_null()) {
                continue;
            } else {
                // unsupported type (e.g., array, object) — skip
                // set(key, it.value().dump());
                LOG_WARNING_FMT( "unsupported json type (e.g., array, object) for key: {}", key );
            }
        }
    }

    static char** to_c_argv(std::vector<std::string>& args) {
        static std::vector<char*> c_args;
        c_args.clear();
        for (auto& s : args) {
            c_args.push_back(const_cast<char*>(s.c_str()));
        }
        c_args.push_back(nullptr);
        return c_args.data();
    }

    std::string name_app;
    cxxopts::Options options;
    cxxopts::ParseResult result;
    std::vector<std::string> origin_args;
    std::vector<std::string> merged_args;
};

} // namespace opts

#endif //__OPTS_HPP