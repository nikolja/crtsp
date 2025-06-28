#pragma once

#ifndef __JSON_HPP
#define __JSON_HPP

#include <memory>
#include <string>
#include <stack>
#include <vector>
#include <unordered_map>

#include <nlohmann/json.hpp>

//#define WITH_METAJSON
#include <meta.hpp>

namespace meta {

template <typename T, typename = std::enable_if_t<is_registered<T>()>> nlohmann::json serialize(T const& obj);
template <typename T, typename = std::enable_if_t<!is_registered<T>()>, typename = void> nlohmann::json serialize(T const& obj);
template <typename T> nlohmann::json serialize_basic(T const& obj);
template <typename T> nlohmann::json serialize_basic(std::vector<T> const& obj);
template <typename T> nlohmann::json serialize_basic(std::stack<T> const& obj);
template <typename K, typename V> nlohmann::json serialize_basic(std::unordered_map<K, V> const& obj);

template <typename T, typename = std::enable_if_t<is_registered<T>()>> void deserialize(T& obj, nlohmann::json const& j);
template <typename T, typename = std::enable_if_t<!is_registered<T>()>, typename = void> void deserialize(T& obj, nlohmann::json const& j);
template <typename T> void deserialize(std::vector<T>& obj, nlohmann::json const& j);
template <typename T> void deserialize(std::stack<T>& obj, nlohmann::json const& j);
template <typename K, typename V> void deserialize(std::unordered_map<K, V>& obj, nlohmann::json const& j);

} // end of namespace meta

template <typename T> void to_json(nlohmann::json& j, T const& obj) { j = meta::serialize(obj); }
template <typename T> void from_json(nlohmann::json const& j, T& obj) { meta::deserialize(obj, j); }

template <typename T>
inline std::string cast_to_string(T const& value) { return std::string(); } // return empty string if no conversion possible
inline std::string cast_to_string(bool const& value) { return value ? "true" : "false"; }
inline std::string cast_to_string(int const& value) { return std::to_string(value); }
inline std::string cast_to_string(float const& value) { return std::to_string(value); }
inline std::string cast_to_string(std::string const& value) { return value; }

template <typename T>
inline T from_string(std::string const& value) { return T(); }
template <> inline bool from_string(std::string const& value) { return (value == "true"); }
template <> inline int from_string(std::string const& value) { return std::stoi(value); }
template <> inline float from_string(std::string const& value) { return std::stof(value); }
template <> inline std::string from_string(std::string const& value) { return value; }

inline std::string safe_string(nlohmann::json const& j, std::string const& key) {
    if (!j.contains(key))
        return "-";
    try {
        if (j.at(key).is_string()) return j.at(key).get<std::string>();
        return j.at(key).dump(); // надає string-подібне представлення будь-якого типу
    } catch (...) {
        return "-";
    }
}

namespace meta {

template <typename T, typename>
nlohmann::json serialize(T const& obj) {
    nlohmann::json j;
    //if (!is_registered<T>()) return j;
    do_for_all_members<T>(
        [&obj, &j](auto const& member) {
            //j[member.get_name()] = member.to_json(obj);
            auto& v = j[member.get_name()];
            if (member.can_get_const_ref()) {
                v = member.get(obj);
            } else if (member.has_getter()) {
                v = member.get_copy(obj); // passing copy as const ref
            }
        }
    );
    return j;
}

template <typename T, typename, typename>
nlohmann::json serialize(T const& obj) { return serialize_basic(obj); }

template <typename T>
nlohmann::json serialize_basic(T const& obj) { return nlohmann::json(obj); }

// specialization for std::vector
template <typename T>
nlohmann::json serialize_basic(std::vector<T> const& obj) {
    int i = 0;
    nlohmann::json j;
    for (auto const& elem : obj) {
        //j[i] = elem; ++i;
        j[i] = serialize(elem); ++i;
    }
    return j;
}

// specialization for std::stack
template <typename T>
nlohmann::json serialize_basic(std::stack<T> const& obj) {
    int i = 0;
    nlohmann::json j;
    /*auto const& container{ obj._Get_container() };
    for (auto const& elem : container) {
        //j[i] = elem; ++i;
        j[i] = serialize(elem); ++i;
    }*/
    auto obj_copy = obj;
    size_t obj_index{ obj_copy.size()-1 };
    while(!obj_copy.empty()) {
        j[obj_index] = serialize(obj_copy.top());
        obj_copy.pop(); obj_index--;
    }
    return j;
}

// specialization for std::unodered_map
template <typename K, typename V>
nlohmann::json serialize_basic(std::unordered_map<K, V> const& obj) {
    nlohmann::json j;
    for (auto& pair : obj) {
        j.emplace(cast_to_string(pair.first), pair.second);
    }
    return j;
}

template <typename T, typename>
void deserialize(T& obj, nlohmann::json const& j) {
    //if (!is_registered<T>()) return;
    if (j.is_object()) {
        do_for_all_members<T>(
            [&obj, &j](auto& member) {
                //member.from_json( obj, j[member.get_name()] );
                if (j.contains(member.get_name())) {
                    auto& v = j[member.get_name()];
                    if (!v.is_null()) {
                        using MemberT = get_member_type<decltype(member)>;
                        if (member.has_setter()) {
                            member.set(obj, v.template get<MemberT>());
                        } else if (member.can_get_ref()) {
                            member.get_ref(obj) = v.template get<MemberT>();
                        }// else throw std::runtime_error("error: can't deserialize member because it's read only");
                    }
                }
            }
        );
    }// else throw std::runtime_error("error: can't deserialize from nlohmann::json to T");
}

template <typename T, typename, typename>
void deserialize(T& obj, nlohmann::json const& j) { obj = j.get<T>(); }

// specialization for std::vector
template <typename T>
void deserialize(std::vector<T>& obj, nlohmann::json const& j) {
    obj.reserve(j.size());
    for (auto const& elem : j) {
        T item;
        deserialize(item, elem);
        obj.push_back(item);
        //obj.push_back(elem);
    }
}

// specialization for std::stack
template <typename T>
void deserialize(std::stack<T>& obj, nlohmann::json const& j) {
    for (auto& elem : j) {
        T item;
        deserialize(item, elem);
        obj.push(item);
    }
}

// specialization for std::unodered_map
template <typename K, typename V>
void deserialize(std::unordered_map<K, V>& obj, nlohmann::json const& j) {
    for (auto it = j.begin(); it != j.end(); ++it) {
        obj.emplace(from_string<K>(it.key()), it.value());
    }
}

// meta_info

template <typename T>
struct meta_info {
    using member_uptr_t = std::unique_ptr<imember<T>>;
    using member_map_t = std::unordered_map<std::string, member_uptr_t>;

    static nlohmann::json serialize(T const& obj) {
        nlohmann::json j;
        for (auto const& pair : members) {
            auto const& member_name{ pair.first };
            auto const& member_uptr{ pair.second };
            //j[member_name] = member_uptr->to_json(obj);
            auto& v = j[member_name];
            if (member_uptr->can_get_const_ref()) {
                v = member_uptr->get(obj);
            } else if (member_uptr->has_getter()) {
                v = member_uptr->get_copy(obj);
            }
        }
        return j;
    }

    static void deserialize(T& obj, nlohmann::json const& j) {
        for (auto const& pair : members) {
            auto const& member_name{ pair.first };
            auto const& member_uptr{ pair.second };
            //member_uptr->from_json( obj, j[member_name] );
            if (!j.contains(member_name))
                continue;
            auto& v = j[member_name];
            if (!v.is_null()) {
                if (member_uptr->has_setter()) {
                    member_uptr->set(obj, v.template get<member_uptr->member_t>());
                } else if (member_uptr->can_get_ref()) {
                    member_uptr->get_ref(obj) = v.template get<member_uptr->member_t>();
                }
            }
        }
    }

    template <typename M>
    static void register_member(const char* name, M T::* ptr) {
        members.emplace(name, std::make_unique<member<T, M>>(name, ptr));
    }

    template <typename M>
    static void register_member(const char* name, ref_getter_func_ptr_t<T, M> getter_ptr, ref_setter_func_ptr_t<T, M> setter_ptr) {
        members.emplace(name, std::make_unique<member<T, M>>(name, getter_ptr, setter_ptr));
    }

    template <typename M>
    static void register_member(const char* name, val_getter_func_ptr_t<T, M> getter_ptr, val_setter_func_ptr_t<T, M> setter_ptr) {
        members.emplace(name, std::make_unique<member<T, M>>(name, getter_ptr, setter_ptr));
    }

private:
    static member_map_t members;
};

template <typename T>
typename meta_info<T>::member_map_t meta_info<T>::members;

/*
// ..
struct foo {
    int age;
    std::string name;
    static void register_meta_info() {
        meta_info<foo>::register_member("age", &foo::age);
        meta_info<foo>::register_member("name", &foo::name);
    }
};
// ..
foo::register_meta_info();
foo f{ "test", 10 };
nlohmann::json j = meta_info<foo>::serialize(f);
std::cout << std::setw(4) << j << std::endl;
// ..
*/

} // end of namespace meta

#endif // #ifndef __JSON_HPP