#pragma once

#ifndef __META_HPP
#define __META_HPP

#include <tuple>
#include <typeinfo>
#include <type_traits>

#if (defined(WITH_METAJSON))
#include <nlohmann/json.hpp>
#endif

// has_member implementation
template<typename T, typename F>
constexpr auto has_member_impl(F&& f) -> decltype(f(std::declval<T>()), true) { return true; }
template<typename> constexpr bool has_member_impl(...) { return false; }
#define has_member_def(T, EXPR) \
has_member_impl<T>( [](auto&& obj)->decltype(obj.EXPR){} )
//if constexpr(has_member_def(T, some_member()))
//static_assert(has_member_def(T, some_member()), "T class must have some_member() member function");
//...
//if constexpr (requires{ obj->some_member(); })
//constexpr bool has_some_member = requires(const T& t) { t.some_member(); };
//template<typename T> concept has_some_member = requires(const T& t) { t.some_member(); };
//if constexpr (requires { &vertex.normal; }) vertex.normal = m * vertex.normal;

namespace meta {

template <typename T>
auto type_to_name() { return typeid(T).name(); }

template<class... Ts> struct overload : Ts... { using Ts::operator()...; };
template<class... Ts> overload(Ts...) -> overload<Ts...>;

// function used for registration of structs by user
template <typename T> inline auto register_members() { return std::make_tuple(); }
// function used for registration of struct name by user
template <typename T> constexpr auto register_name() { return ""; }

namespace detail {
// template_helpers
template <typename F, typename... Args, std::size_t... Idx>
void for_tuple_impl(F&& f, const std::tuple<Args...>& tuple, std::index_sequence<Idx...>) { (f(std::get<Idx>(tuple)), ...); }
// for_each_arg - call f for each element from tuple
template <typename F, typename... Args> void for_tuple(F&& f, const std::tuple<Args...>& tuple) { for_tuple_impl(f, tuple, std::index_sequence_for<Args...>{}); }
// overload for empty tuple which does nothing
template <typename F> void for_tuple(F&& /* f */, const std::tuple<>& /* tuple */) { /* do nothing */ }
// meta_holder holds all member_holder objects constructed via register_members<T> call.
// if the struct T is not registered, members is std::tuple<>
template <typename T, typename TupleType>
struct meta_holder {
    static TupleType members;
    static const char* name() { return register_name<T>(); }
};
template <typename T, typename TupleType> TupleType meta_holder<T, TupleType>::members = register_members<T>();
} // end of namespace detail

template <typename T, typename M>
using member_ptr_t = M T::*;
// reference getter/setter func pointer type
template <typename T, typename M> using ref_getter_func_ptr_t = const M& (T::*)() const;
template <typename T, typename M> using ref_setter_func_ptr_t = void (T::*)(const M&);
// value getter/setter func pointer type
template <typename T, typename M> using val_getter_func_ptr_t = const M(T::*)() const; // const M ?
template <typename T, typename M> using val_setter_func_ptr_t = void (T::*)(M);
// non const reference getter
template <typename T, typename M> using nonconst_ref_getter_func_ptr_t = M& (T::*)();
// MT is member<M, T>
template <typename MT>
using get_member_type = typename std::decay_t<MT>::member_t;

template <typename T>
struct imember {
    virtual ~imember() = default;

    #if (defined(WITH_METAJSON))
    virtual void from_json(T& obj, nlohmann::json const& j) const = 0;
    virtual nlohmann::json to_json(T const& obj) const = 0;
    #endif

    //virtual void print() { LOG_INFO_FMT( type_to_name<T>() ); }
};

template <typename T, typename M>
struct member : imember<T> {
    using struct_t = T;
    using member_t = M;
    
    member(member_ptr_t<T, M> ptr) : ptr(ptr), has_member_ptr(true) {} //std::is_same_v<member_ptr_t<T, M>, M T::*> - member_ptr_t<T, M> is M T::*
    member(ref_getter_func_ptr_t<T, M> getter_ptr, ref_setter_func_ptr_t<T, M> setter_ptr) : ref_getter_ptr(getter_ptr), ref_setter_ptr(setter_ptr) {}
    member(val_getter_func_ptr_t<T, M> getter_ptr, val_setter_func_ptr_t<T, M> setter_ptr) : val_getter_ptr(getter_ptr), val_setter_ptr(setter_ptr) {}

    member(int id, member_ptr_t<T, M> ptr) : id(id), ptr(ptr), has_member_ptr(true) {}
    member(int id, ref_getter_func_ptr_t<T, M> getter_ptr, ref_setter_func_ptr_t<T, M> setter_ptr) : id(id), ref_getter_ptr(getter_ptr), ref_setter_ptr(setter_ptr) {}
    member(int id, val_getter_func_ptr_t<T, M> getter_ptr, val_setter_func_ptr_t<T, M> setter_ptr) : id(id), val_getter_ptr(getter_ptr), val_setter_ptr(setter_ptr) {}

    member(const char* name, member_ptr_t<T, M> ptr) : name(name), ptr(ptr), has_member_ptr(true) {}
    member(const char* name, ref_getter_func_ptr_t<T, M> getter_ptr, ref_setter_func_ptr_t<T, M> setter_ptr) : name(name), ref_getter_ptr(getter_ptr), ref_setter_ptr(setter_ptr) {}
    member(const char* name, val_getter_func_ptr_t<T, M> getter_ptr, val_setter_func_ptr_t<T, M> setter_ptr) : name(name), val_getter_ptr(getter_ptr), val_setter_ptr(setter_ptr) {}

    member(const char* name, int id, member_ptr_t<T, M> ptr) : name(name), id(id), ptr(ptr), has_member_ptr(true) {}
    member(const char* name, int id, ref_getter_func_ptr_t<T, M> getter_ptr, ref_setter_func_ptr_t<T, M> setter_ptr) : name(name), id(id), ref_getter_ptr(getter_ptr), ref_setter_ptr(setter_ptr) {}
    member(const char* name, int id, val_getter_func_ptr_t<T, M> getter_ptr, val_setter_func_ptr_t<T, M> setter_ptr) : name(name), id(id), val_getter_ptr(getter_ptr), val_setter_ptr(setter_ptr) {}
    
    member& add_nonconst_getter(nonconst_ref_getter_func_ptr_t<T, M> nonconst_ref_getter_ptr) { this->nonconst_ref_getter_ptr = nonconst_ref_getter_ptr; return *this; }

    // gets const reference to the member
    const M& get(const T& obj) const { return ref_getter_ptr ? (obj.*ref_getter_ptr)() : obj.*ptr; }
    // gets copy of member (useful to if only value getter is provided, can't return const T& in that case)
    M get_copy(const T& obj) const { return ref_getter_ptr ? (obj.*ref_getter_ptr)() : (val_getter_ptr ? (obj.*val_getter_ptr)() : obj.*ptr); }
    // gets non const reference to the member
    M& get_ref(T& obj) const { return nonconst_ref_getter_ptr ? (obj.*nonconst_ref_getter_ptr)() : obj.*ptr; }
    member_ptr_t<T, M> get_ptr() const { return ptr; }
    // TODO: add rvalue_setter?
    // sets value to the member, lvalues and rvalues are accepted
    template <typename V, typename = std::enable_if_t<std::is_constructible_v<M, V>>>
    //void set(T& obj, V&& value) const { ref_setter_ptr ? (obj.*ref_setter_ptr)(value) : (val_setter_ptr ? (obj.*val_setter_ptr)(value)/*copy value*/ : obj.*ptr = value); } // accepts lvalues and rvalues!
    void set(T& obj, V&& value) const { if ( ref_setter_ptr ) (obj.*ref_setter_ptr)(value); else if ( val_setter_ptr ) (obj.*val_setter_ptr)(value); else obj.*ptr = value; }

    bool has_ptr() const { return has_member_ptr; }
    bool has_getter() const { return ref_getter_ptr || val_getter_ptr; }
    bool has_setter() const { return ref_setter_ptr || val_setter_ptr; }
    bool can_get_const_ref() const { return has_member_ptr || ref_getter_ptr; }
    bool can_get_ref() const { return has_member_ptr || nonconst_ref_getter_ptr; }

    // return of member id you've set during "registration"
    const int get_id() const { return id; }
    // return const char* of member name you've set during "registration"
    const char* get_name() const { return name; }

    #if (defined(WITH_METAJSON))
    void from_json(T& obj, nlohmann::json const& j) const override {
        //obj.*ptr = j;
        if (!j.is_null()) {
            if (has_setter()) {
                set(obj, j.template get<member_t>());
            } else if (can_get_ref()) {
                get_ref(obj) = j.template get<member_t>();
            }
        }
    }
    nlohmann::json to_json(T const& obj) const override {
        return can_get_const_ref() ? nlohmann::json( get(obj) ) : nlohmann::json( get_copy(obj) );
    }
    #endif

    //void print() { LOG_INFO_FMT( "T: {} M: {}", type_to_name<T>(), type_to_name<M>() ); }
private:
    int id{0};
    const char* name{nullptr};
    member_ptr_t<T, M> ptr{nullptr};
    bool has_member_ptr{false};
    ref_getter_func_ptr_t<T, M> ref_getter_ptr{nullptr};
    ref_setter_func_ptr_t<T, M> ref_setter_ptr{nullptr};
    val_getter_func_ptr_t<T, M> val_getter_ptr{nullptr};
    val_setter_func_ptr_t<T, M> val_setter_ptr{nullptr};
    nonconst_ref_getter_func_ptr_t<T, M> nonconst_ref_getter_ptr{nullptr};
};

// useful function similar to make_pair which is used so you don't have to write this:
// member<some_struct, some_member>(&some_struct::some_member); and can just to this:
// make_member(&some_struct::some_member);

// read/write
template <typename T, typename M> member<T, M> make_member(M T::* ptr) { return member<T, M>(ptr); }
template <typename T, typename M> member<T, M> make_member(ref_getter_func_ptr_t<T, M> getter_ptr, ref_setter_func_ptr_t<T, M> setter_ptr) { return member<T, M>(getter_ptr, setter_ptr); }
template <typename T, typename M, std::enable_if_t< !std::is_reference_v< M >, bool > = true> member<T, M> make_member(val_getter_func_ptr_t<T, M> getter_ptr, val_setter_func_ptr_t<T, M> setter_ptr) { return member<T, M>(getter_ptr, setter_ptr); }
// id
template <typename T, typename M> member<T, M> make_member(const int id, M T::* ptr) { return member<T, M>(id, ptr); }
template <typename T, typename M> member<T, M> make_member(const int id, ref_getter_func_ptr_t<T, M> getter_ptr, ref_setter_func_ptr_t<T, M> setter_ptr) { return member<T, M>(id, getter_ptr, setter_ptr); }
template <typename T, typename M, std::enable_if_t< !std::is_reference_v< M >, bool > = true> member<T, M> make_member(const int id, val_getter_func_ptr_t<T, M> getter_ptr, val_setter_func_ptr_t<T, M> setter_ptr) { return member<T, M>(id, getter_ptr, setter_ptr); }
// name
template <typename T, typename M> member<T, M> make_member(const char* name, M T::* ptr) { return member<T, M>(name, ptr); }
template <typename T, typename M> member<T, M> make_member(const char* name, ref_getter_func_ptr_t<T, M> getter_ptr, ref_setter_func_ptr_t<T, M> setter_ptr) { return member<T, M>(name, getter_ptr, setter_ptr); }
template <typename T, typename M, std::enable_if_t< !std::is_reference_v< M >, bool > = true> member<T, M> make_member(const char* name, val_getter_func_ptr_t<T, M> getter_ptr, val_setter_func_ptr_t<T, M> setter_ptr) { return member<T, M>(name, getter_ptr, setter_ptr); }
// name, id
template <typename T, typename M> member<T, M> make_member(const char* name, const int id, M T::* ptr) { return member<T, M>(name, id, ptr); }
template <typename T, typename M> member<T, M> make_member(const char* name, const int id, ref_getter_func_ptr_t<T, M> getter_ptr, ref_setter_func_ptr_t<T, M> setter_ptr) { return member<T, M>(name, id, getter_ptr, setter_ptr); }
template <typename T, typename M, std::enable_if_t< !std::is_reference_v< M >, bool > = true> member<T, M> make_member(const char* name, const int id, val_getter_func_ptr_t<T, M> getter_ptr, val_setter_func_ptr_t<T, M> setter_ptr) { return member<T, M>(name, id, getter_ptr, setter_ptr); }
// read only
template <typename T, typename M> member<T, M> make_member(ref_getter_func_ptr_t<T, M> getter_ptr) { return member<T, M>(getter_ptr, nullptr); }
template <typename T, typename M, std::enable_if_t< !std::is_reference_v< M >, bool > = true> member<T, M> make_member(val_getter_func_ptr_t<T, M> getter_ptr) { return member<T, M>(getter_ptr, nullptr); }
// id
template <typename T, typename M> member<T, M> make_member(const int id, ref_getter_func_ptr_t<T, M> getter_ptr) { return member<T, M>(id, getter_ptr, nullptr); }
template <typename T, typename M, std::enable_if_t< !std::is_reference_v< M >, bool > = true> member<T, M> make_member(const int id, val_getter_func_ptr_t<T, M> getter_ptr) { return member<T, M>(id, getter_ptr, nullptr); }
// name
template <typename T, typename M> member<T, M> make_member(const char* name, ref_getter_func_ptr_t<T, M> getter_ptr) { return member<T, M>(name, getter_ptr, nullptr); }
template <typename T, typename M, std::enable_if_t< !std::is_reference_v< M >, bool > = true> member<T, M> make_member(const char* name, val_getter_func_ptr_t<T, M> getter_ptr) { return member<T, M>(name, getter_ptr, nullptr); }
// name, id
template <typename T, typename M> member<T, M> make_member(const char* name, const int id, ref_getter_func_ptr_t<T, M> getter_ptr) { return member<T, M>(name, id, getter_ptr, nullptr); }
template <typename T, typename M, std::enable_if_t< !std::is_reference_v< M >, bool > = true> member<T, M> make_member(const char* name, const int id, val_getter_func_ptr_t<T, M> getter_ptr) { return member<T, M>(name, id, getter_ptr, nullptr); }
// write only
template <typename T, typename M> member<T, M> make_member(ref_setter_func_ptr_t<T, M> setter_ptr) { return member<T, M>(nullptr, setter_ptr); }
template <typename T, typename M, std::enable_if_t< !std::is_reference_v< M >, bool > = true> member<T, M> make_member(val_setter_func_ptr_t<T, M> setter_ptr) { return member<T, M>(nullptr, setter_ptr); }
// id
template <typename T, typename M> member<T, M> make_member(const int id, ref_setter_func_ptr_t<T, M> setter_ptr) { return member<T, M>(id, nullptr, setter_ptr); }
template <typename T, typename M, std::enable_if_t< !std::is_reference_v< M >, bool > = true> member<T, M> make_member(const int id, val_setter_func_ptr_t<T, M> setter_ptr) { return member<T, M>(id, nullptr, setter_ptr); }
// name
template <typename T, typename M> member<T, M> make_member(const char* name, ref_setter_func_ptr_t<T, M> setter_ptr) { return member<T, M>(name, nullptr, setter_ptr); }
template <typename T, typename M, std::enable_if_t< !std::is_reference_v< M >, bool > = true> member<T, M> make_member(const char* name, val_setter_func_ptr_t<T, M> setter_ptr) { return member<T, M>(name, nullptr, setter_ptr); }
// name, id
template <typename T, typename M> member<T, M> make_member(const char* name, const int id, ref_setter_func_ptr_t<T, M> setter_ptr) { return member<T, M>(name, id, nullptr, setter_ptr); }
template <typename T, typename M, std::enable_if_t< !std::is_reference_v< M >, bool > = true > member<T, M> make_member(const char* name, const int id, val_setter_func_ptr_t<T, M> setter_ptr) { return member<T, M>(name, id, nullptr, setter_ptr); }

template <typename... Args> auto make_members(Args&&... args) { return std::make_tuple(std::forward<Args>(args)...); }

// returns set name for struct
template <typename T> constexpr auto get_name() { return detail::meta_holder<T, decltype(register_members<T>())>::name(); }
// returns the number of registered members of the struct T
template <typename T> constexpr std::size_t get_member_count() { return std::tuple_size_v<decltype(register_members<T>())>; }
// returns std::tuple of members
template <typename T> const auto& get_members() { return detail::meta_holder<T, decltype(register_members<T>())>::members; }
// check if struct T has register_members<T> specialization (has been registered)
template <typename T> constexpr bool is_registered() { return !std::is_same_v<std::tuple<>, decltype(register_members<T>())>; }

// check if struct T has member
// id
template <typename T> bool has_member(const int id) {
    bool found = false;
    do_for_all_members<T>(
        [&found, &id](const auto& member) {
            if (id == member.get_id()) {
                found = true;
            }
        }
    );
    return found;
}
// name
template <typename T> bool has_member(const char* name) {
    bool found = false;
    do_for_all_members<T>(
        [&found, &name](const auto& member) {
            if (!strcmp(name, member.get_name())) {
                found = true;
            }
        }
    );
    return found;
}
// name, id
template <typename T> bool has_member(const char* name, const int id) {
    bool found = false;
    do_for_all_members<T>(
        [&found, &name, &id](const auto& member) {
            if (!strcmp(name, member.get_name()) && id == member.get_id()) {
                found = true;
            }
        }
    );
    return found;
}

template <typename T, typename F, typename = std::enable_if_t<is_registered<T>()>>
void do_for_all_members(F&& f) { detail::for_tuple(std::forward<F>(f), get_members<T>()); }
// version for non-registered classes (to generate less template stuff)
template <typename T, typename F, typename = std::enable_if_t<!is_registered<T>()>, typename = void>
void do_for_all_members(F&& /* f */) { /* do nothing */ }

// do F for member by 'id'
template <typename T, typename F> void do_for_member_all(const int id, F&& f) {
    do_for_all_members<T>( [&](const auto& member) { if (id == member.get_id()) { f(member); } } );
}
// do F for member by 'name'
template <typename T, typename F> void do_for_member_all(const char* name, F&& f) {
    do_for_all_members<T>( [&](const auto& member) { if (!strcmp(name, member.get_name())) { f(member); } } );
}
// do F for member by 'id' and 'name'
template <typename T, typename F> void do_for_member_all(const char* name, const int id, F&& f) {
    do_for_all_members<T>( [&](const auto& member) { if (!strcmp(name, member.get_name()) && id == member.get_id()) { f(member); } } );
}

// do F for member by 'id' with convertible type M
template <typename T, typename M, typename F> void do_for_member_convertible(const int id, F&& f) {
    do_for_all_members<T>(
        [&](const auto& member) {
            if (id == member.get_id()) {
                using MemberT = meta::get_member_type<decltype(member)>;
                if constexpr (std::is_convertible_v<MemberT, M>) {
                    f(member);
                }
            }
        }
    );
}

// do F for member by 'name' with convertible type M
template <typename T, typename M, typename F> void do_for_member_convertible(const char* name, F&& f) {
    do_for_all_members<T>(
        [&](const auto& member) {
            if (!strcmp(name, member.get_name())) {
                using MemberT = meta::get_member_type<decltype(member)>;
                if constexpr (std::is_convertible_v<MemberT, M>) {
                    f(member);
                }
            }
        }
    );
}

// do F for member by 'name' with convertible type M
template <typename T, typename M, typename F> void do_for_member_convertible(const char* name, const int id, F&& f) {
    do_for_all_members<T>(
        [&](const auto& member) {
            if (!strcmp(name, member.get_name()) && id == member.get_id()) {
                using MemberT = meta::get_member_type<decltype(member)>;
                if constexpr (std::is_convertible_v<MemberT, M>) {
                    f(member);
                }
            }
        }
    );
}

// do F for member by 'id' with type M, it's important to pass correct type of the member
template <typename T, typename M, typename F> void do_for_member(const int id, F&& f) { // do_for_member_same ?
    do_for_all_members<T>(
        [&](const auto& member) {
            if (id == member.get_id()) {
                using MemberT = meta::get_member_type<decltype(member)>;
                //assert((std::is_same_v<MemberT, M>) && "member doesn't have type M");
                if constexpr (std::is_same_v<MemberT, M>) {
                    f(member);
                }
            }
        }
    );
}
// do F for member named 'name' with type M, it's important to pass correct type of the member
template <typename T, typename M, typename F> void do_for_member(const char* name, F&& f) {
    do_for_all_members<T>(
        [&](const auto& member) {
            if (!strcmp(name, member.get_name())) {
                using MemberT = meta::get_member_type<decltype(member)>;
                //assert((std::is_same_v<MemberT, M>) && "member doesn't have type M");
                if constexpr (std::is_same_v<MemberT, M>) {
                    f(member);
                }
            }
        }
    );
}
// do F for member by 'id' and 'name' with type M, it's important to pass correct type of the member
template <typename T, typename M, typename F> void do_for_member(const char* name, const int id, F&& f) {
    do_for_all_members<T>(
        [&](const auto& member) {
            if (!strcmp(name, member.get_name()) && id == member.get_id()) {
                using MemberT = meta::get_member_type<decltype(member)>;
                //assert((std::is_same_v<MemberT, M>) && "member doesn't have type M");
                if constexpr (std::is_same_v<MemberT, M>) {
                    f(member);
                }
            }
        }
    );
}

// get typeid of the member value by 'id'
template <typename T> std::type_info get_member_typeid(T& obj, const int id) {
    std::type_info type;
    do_for_member_all<T>(id, [&type, &obj](const auto& member) { type = typeid(meta::get_member_type<decltype(member)>); } );
    return type;
}

// get typeid of the member value by 'name'
template <typename T> std::type_info get_member_typeid(T& obj, const char* name) {
    std::type_info type;
    do_for_member_all<T>(name, [&type, &obj](const auto& member) { type = typeid(meta::get_member_type<decltype(member)>); } );
    return type;
}
// get typeid of the member value by 'id' and 'name'
template <typename T> std::type_info get_member_typeid(T& obj, const char* name, const int id) {
    std::type_info type;
    do_for_member_all<T>(name, id, [&type, &obj](const auto& member) { type = typeid(meta::get_member_type<decltype(member)>); } );
    return type;
}

// get value of the member by 'id'
template <typename M, typename T> M get_member_value(T& obj, const int id) {
    M value;
    do_for_member<T, M>(id, [&value, &obj](const auto& member) { value = member.get_copy(obj); } );
    return value;
}
// get value of the member named 'name'
template <typename M, typename T> M get_member_value(T& obj, const char* name) {
    M value;
    do_for_member<T, M>(name, [&value, &obj](const auto& member) { value = member.get_copy(obj); } );
    return value;
}
// get value of the member by 'id' and 'name'
template <typename M, typename T> M get_member_value(T& obj, const char* name, const int id) {
    M value;
    do_for_member<T, M>(name, id, [&value, &obj](const auto& member) { value = member.get_copy(obj); } );
    return value;
}

// set value of the member by 'id'
template <typename M, typename T, typename V, typename = std::enable_if_t<std::is_constructible_v<M, V>>>
void set_member_value(T& obj, const int id, V&& value) {
    do_for_member<T, M>(id, [&obj, value = std::forward<V>(value)](const auto& member) { member.set(obj, value); } );
}
// set value of the member named 'name'
template <typename M, typename T, typename V, typename = std::enable_if_t<std::is_constructible_v<M, V>>>
void set_member_value(T& obj, const char* name, V&& value) {
    do_for_member<T, M>(name, [&obj, value = std::forward<V>(value)](const auto& member) { member.set(obj, value); } );
}
// set value of the member by 'id' and 'name'
template <typename M, typename T, typename V, typename = std::enable_if_t<std::is_constructible_v<M, V>>>
void set_member_value(T& obj, const char* name, const int id, V&& value) {
    do_for_member<T, M>(name, id, [&obj, value = std::forward<V>(value)](const auto& member) { member.set(obj, value); } );
}

// get value of the member by 'id'
template <typename M, typename T> M get_member_conval(T& obj, const int id) {
    M value;
    do_for_member_convertible<T, M>(id, [&value, &obj](const auto& member) { value = member.get_copy(obj); } );
    return value;
}
// get value of the member named 'name'
template <typename M, typename T> M get_member_conval(T& obj, const char* name) {
    M value;
    do_for_member_convertible<T, M>(name, [&value, &obj](const auto& member) { value = member.get_copy(obj); } );
    return value;
}
// get value of the member by 'id' and 'name'
template <typename M, typename T> M get_member_conval(T& obj, const char* name, const int id) {
    M value;
    do_for_member_convertible<T, M>(name, id, [&value, &obj](const auto& member) { value = member.get_copy(obj); } );
    return value;
}

// set value of the member by 'id'
template <typename T, typename V>
void set_member_conval(T& obj, const int id, V&& value) {
    do_for_member_convertible<T, V>(id, [&obj, value = std::forward<V>(value)](const auto& member) { member.set(obj, value); } );
}
// set value of the member named 'name'
template <typename T, typename V>
void set_member_conval(T& obj, const char* name, V&& value) {
    do_for_member_convertible<T, V>(name, [&obj, value = std::forward<V>(value)](const auto& member) { member.set(obj, value); } );
}
// set value of the member by 'id' and 'name'
template <typename T, typename V>
void set_member_conval(T& obj, const char* name, const int id, V&& value) {
    do_for_member_convertible<T, V>(name, id, [&obj, value = std::forward<V>(value)](const auto& member) { member.set(obj, value); } );
}

// type_list is array of types
template <typename... Args>
struct type_list {
    template <std::size_t N>
    using type = std::tuple_element_t<N, std::tuple<Args...>>;
    using indices = std::index_sequence_for<Args...>;
    static const size_t size = sizeof...(Args);
};
template <typename T> struct constructor_args { using types = type_list<>; };
template <typename T> using constructor_arguments = typename constructor_args<T>::types;
// check if struct T has non-default ctor registered
template <typename T> constexpr bool ctor_registered() { return !std::is_same_v<type_list<>, constructor_arguments<T>>; }

/*
namespace meta {
template <> inline auto register_members<some_struct>() {
    return make_members(
        make_member(...),
        ...
    );
}
// you can register members by using their data member pointer: make_member("some_member", &some_struct::some_member)
// or use getters/setters: make_member("some_member", &some_struct::get_some_member, &some_struct::set_some_member)
// and you can add non-const getter: make_member(...).add_nonconst_getter(&some_struct::get_some_member_ref)
// getters and setters can be by-value (T is member type):
// T some_struct::get_some_member() const { return some_member; }
// void some_struct::set_some_member(T value) { some_member = value; }
// or by reference: 
// const T& some_struct::get_some_member() const { return some_member; }
// void some_struct::set_some_member(const T& value) { some_member = value; }
// non-const getter has the following form:
// T& some_struct::get_some_member_ref() { return some_member; }
*/

} // end of namespace meta

#endif // #ifndef __META_HPP