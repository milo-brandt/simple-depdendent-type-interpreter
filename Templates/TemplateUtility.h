//
//  TemplateUtility.h
//  FactoryGame
//
//  Created by Milo Brandt on 10/9/19.
//  Copyright © 2019 Milo Brandt. All rights reserved.
//

#ifndef TemplateUtility_h
#define TemplateUtility_h

#include <tuple>
#include <array>
#include <vector>
#include <variant>
#include <cassert>
#include <utility>

namespace utility{
    struct init_required_t {
      template <class T>
      operator T() const; // Left undefined
    } static const init_required;
    //From cppreference
    namespace detail {
        template <class>
        constexpr bool is_reference_wrapper_v = false;
        template <class U>
        constexpr bool is_reference_wrapper_v<std::reference_wrapper<U>> = true;

        template <class T, class Type, class T1, class... Args>
        constexpr decltype(auto) INVOKE(Type T::* f, T1&& t1, Args&&... args)
        {
            if constexpr (std::is_member_function_pointer_v<decltype(f)>) {
                if constexpr (std::is_base_of_v<T, std::decay_t<T1>>)
                    return (std::forward<T1>(t1).*f)(std::forward<Args>(args)...);
                else if constexpr (is_reference_wrapper_v<std::decay_t<T1>>)
                    return (t1.get().*f)(std::forward<Args>(args)...);
                else
                    return ((*std::forward<T1>(t1)).*f)(std::forward<Args>(args)...);
            } else {
                static_assert(std::is_member_object_pointer_v<decltype(f)>);
                static_assert(sizeof...(args) == 0);
                if constexpr (std::is_base_of_v<T, std::decay_t<T1>>)
                    return std::forward<T1>(t1).*f;
                else if constexpr (is_reference_wrapper_v<std::decay_t<T1>>)
                    return t1.get().*f;
                else
                    return (*std::forward<T1>(t1)).*f;
            }
        }

        template <class F, class... Args>
        constexpr decltype(auto) INVOKE(F&& f, Args&&... args)
        {
            return std::forward<F>(f)(std::forward<Args>(args)...);
        }
    }

    template< class F, class... Args>
    constexpr std::invoke_result_t<F, Args...> invoke(F&& f, Args&&... args)
    noexcept(std::is_nothrow_invocable_v<F, Args...>)
    {
        return detail::INVOKE(std::forward<F>(f), std::forward<Args>(args)...);
    }

    /* Template wrappers */

    template<auto V>
    struct value_flag{
        static constexpr auto value = V;
    };
    template<class T>
    struct class_flag{
        using type = T;
    };
    template<class... Args>
    struct class_list;
    namespace detail{
        template<class A, class B>
        struct flattener;
        template<class... First, class... Rest, class... Out>
        struct flattener<class_list<class_list<First...>, Rest...>, class_list<Out...> > : flattener<class_list<Rest...>, class_list<Out..., First...> >{};
        template<class... Out>
        struct flattener<class_list<>, class_list<Out...> > : class_flag<class_list<Out...> >{};
        template<class A, class B>
        struct uniquer;
        template<class First, class... Rest, class... Out>
        struct uniquer<class_list<First, Rest...>, class_list<Out...> > : std::conditional_t<(std::is_same_v<First, Out> || ...), uniquer<class_list<Rest...>, class_list<Out...> >, uniquer<class_list<Rest...>, class_list<Out..., First> > >{};
        template<class... Out>
        struct uniquer<class_list<>, class_list<Out...> > : class_flag<class_list<Out...> >{};
    };
    template<class... Args>
    struct class_list;
    template<class... Ts>
    using join_class_lists = typename detail::flattener<class_list<Ts...>, class_list<> >::type;

    namespace detail{
        template<class T>
        struct member_fn_helper;
        template<class Ret, class C, class... Args>
        struct member_fn_helper<Ret(C::*)(Args...)>{
            using ret_type = Ret;
            using arg_list = class_list<Args...>;
        };
        template<class Ret, class C, class... Args>
        struct member_fn_helper<Ret(C::*)(Args...) const>{
            using ret_type = Ret;
            using arg_list = class_list<Args...>;
        };
        template<class T>
        struct invoke_signature_finder : member_fn_helper<decltype(&T::operator())>{};
        template<class R, class... Args>
        struct invoke_signature_finder<R(*)(Args...)>{
            using ret_type = R;
            using arg_list = class_list<Args...>;
        };
        template<class R, class T>
        struct invoke_signature_finder<R T::*>{
            using ret_type = R;
            using arg_list = class_list<T>;
        };
        template<class R, class T, class... Args>
        struct invoke_signature_finder<R(T::*)(Args...)>{
            using ret_type = R;
            using arg_list = class_list<T, Args...>;
        };
    };
    template<class T> //ret_type, arg_list members
    using invoke_signature_of = detail::invoke_signature_finder<T>;


    namespace detail{
        template<class T>
        struct ArgListFor;
        template<template<class...> class T, class... Args>
        struct ArgListFor<T<Args...> >{
            using Type = class_list<Args...>;
        };
    };
    template<class T>
    using class_list_from_template_args = typename detail::ArgListFor<T>::Type;

    template<auto Fn>
    struct static_function{ //A default-constructible wrapper for a specific function.
        static_assert(std::is_function_v<Fn>, "Only functions can be used as static functions.");
    };
    template<class Ret, class... Args, Ret(*fn)(Args...)>
    struct static_function<fn>{
        static constexpr auto function = fn;
        Ret operator()(Args... args){
            return fn(args...);
        };
    };


    /* Tuple iteration helpers */

    namespace detail{
        template<class Fn, std::size_t... indices>
        constexpr void forEachUpToImpl(Fn&& f, std::index_sequence<indices...>){
            (utility::invoke(f, value_flag<indices>{}) , ...);
        }

    }
    template<std::size_t num, class Fn>
    constexpr void for_each_up_to(Fn&& f){
        detail::forEachUpToImpl(std::forward<Fn>(f), std::make_index_sequence<num>{});
    }
    template<class Fn, class Tuple>
    constexpr void for_each_in_tuple(Tuple&& tuple, Fn&& f){
        std::apply([&f](auto&&... args){
            (utility::invoke(f, args) , ...);
        }, std::forward<Tuple>(tuple));
    }
    template<class Fn, class Tuple>
    constexpr auto transform_tuple(Tuple&& tuple, Fn&& f){
        return std::apply([&f](auto&&... args){
            return std::make_tuple(utility::invoke(f, args)...);
        }, std::forward<Tuple>(tuple));
    }

    /* Array iteration helpers */

    template<class Fn, class T, std::size_t N>
    constexpr void for_each_in_array(std::array<T, N> const& arr, Fn&& f){
        std::apply([&f](auto&&... args){
            (utility::invoke(f, args) , ...);
        }, arr);
    }
    template<class Fn, class T, std::size_t N, class Ret = std::invoke_result_t<Fn, T> >
    constexpr auto transform_array(std::array<T, N> const& arr, Fn&& f){
        return std::apply([&f](auto&&... args){
            return std::array<Ret, N>(utility::invoke(f, args)...);
        }, arr);
    }
    template<std::size_t N>
    constexpr std::size_t count_in_array(std::array<bool, N> const& arr){
        std::size_t ret = 0;
        for_each_in_array(arr, [&ret](bool b){ ret += b; });
        return ret;
    }
    template<std::size_t N>
    constexpr std::size_t has_unique_true_in_array(std::array<bool, N> const& arr){
        return count_in_array(arr) == 1;
    }
    template<std::size_t N>
    constexpr std::size_t index_of_last_true_value(std::array<bool, N> const& arr){
        std::size_t ret = -1;
        std::size_t pos = 0;
        for_each_in_array(arr, [&ret, &pos](bool b){
            if(b) ret = pos;
            ++pos;
        });
        return ret;
    }
    template<std::size_t N>
    constexpr bool any_in_array(std::array<bool, N> const& arr){
        bool ret = false;
        forEachInArray([&ret](bool b){ ret = ret || b; }, arr);
        return ret;
    }
    template<std::size_t N>
    constexpr bool all_in_array(std::array<bool, N> const& arr){
        bool ret = true;
        forEachInArray([&ret](bool b){ ret = ret && b; }, arr);
        return ret;
    }


    /* Lambda helpers */

    template<class... Ts> struct overloaded : Ts... { using Ts::operator()...; }; //Wonderful trick stolen from Bjarne.
    template<class... Ts> overloaded(Ts...) -> overloaded<Ts...>;

    struct ignore_undefined_t{
        template<class... Args>
        void operator()(Args&&...) const{}; //Lowest priority for matching
    };
    constexpr ignore_undefined_t ignore_undefined;

    template<class... Ts> struct precedence_overloaded;
    template<> struct precedence_overloaded<>{};
    template<class T, class... Ts> struct precedence_overloaded<T, Ts...> : T, precedence_overloaded<Ts...>{
      template<class... Args> requires (std::is_invocable_v<T, Args&&...> || ... || std::is_invocable_v<Ts, Args&&...>)
      decltype(auto) operator()(Args&&... args){
        if constexpr(std::is_invocable_v<T, Args&&...>){
          return T::operator()(std::forward<Args>(args)...);
        }else{
          return precedence_overloaded<Ts...>::operator()(std::forward<Args>(args)...);
        }
      }
    };


    /* Constexpr-friendly range manipulations */

    namespace detail{
        template<class T, std::size_t N, class Callback, std::size_t... indices>
        constexpr std::array<T,N> __generateStaticArray(Callback&& callback, std::index_sequence<indices...>){
            static_assert((std::is_invocable_r_v<T, Callback, value_flag<indices> > && ...), "The callback must be callable with value_flag<n> for 0 < n < N and return the proper type.");
            return {callback(value_flag<indices>{})...};
        }
        template<class Callback, std::size_t... indices>
        constexpr auto __generateStaticTuple(Callback&& callback){
            static_assert((std::is_object_v<std::invoke_result_t<Callback, value_flag<indices> > > && ...), "The callback must be callable with value_flag<n> for 0 < n < N and return an object.");
            return std::make_tuple(callback(value_flag<indices>{})...);
        }
        template<template<auto...> class, class>
        struct __generateClassList;
        template<template<auto...> class Transform, std::size_t... indices>
        struct __generateClassList<Transform, std::index_sequence<indices...> >{
            using Type = class_list<Transform<indices>...>;
        };
    }
    template<class T, std::size_t N, class Callback>
    constexpr std::array<T,N> generate_static_array(Callback&& callback){ //Calls callback with ValueFlag<0>{}, ...
        return detail::__generateStaticArray<T, N>(callback, std::make_index_sequence<N>{});
    }
    template<std::size_t N, class Callback>
    constexpr auto generate_static_tuple(Callback&& callback){ //Calls callback with ValueFlag<0>{}, ...
        return detail::__generateStaticTuple(callback, std::make_index_sequence<N>{});
    }
    template<std::size_t N, template<auto...> class Transform>
    using generate_class_list = typename detail::__generateClassList<Transform, std::make_index_sequence<N> >::Type;

    /* Parameter pack helpers */

    namespace detail{
        template<std::size_t index, class... N>
        struct __IndexExtractor;
        template<class First, class... Rest>
        struct __IndexExtractor<0, First, Rest...>{
            using Type = First;
        };
        template<std::size_t index, class First, class... Rest>
        struct __IndexExtractor<index, First, Rest...> : __IndexExtractor<index - 1, Rest...>{};
        template<std::size_t index, class... N>
        struct __CheckedExtractor{
            static_assert(index < sizeof...(N), "Index must be less than size of parameter pack.");
            using Type = typename __IndexExtractor<index, N...>::Type;
        };




    };
    template<std::size_t index, class... N>
    using nth_parameter_in_pack = typename detail::__CheckedExtractor<index, N...>::Type;
    template<class T, class... N>
    static constexpr bool pack_includes = (std::is_same_v<T, N> || ...);
    template<class T, class... N>
    static constexpr bool pack_includes_unique = has_unique_true_in_array(std::array<bool, sizeof...(N)>{std::is_same_v<T, N>...});
    template<class T, class... N>
    static constexpr std::size_t last_index_in_pack = index_of_last_true_value(std::array<bool, sizeof...(N)>{std::is_same_v<T, N>...});

    /* Calling helpers */

    template<class Fn,class... Args>
    constexpr void call_if_possible(Fn&& f,Args&&... args){
        if constexpr(std::is_invocable_v<Fn, Args...>){
            invoke(std::forward<Fn>(f), std::forward<Args>(args)...);
        }
    }

    template<class T>
    void erase_one(std::vector<T>& vec, T const& val){
        auto it = std::find(vec.begin(), vec.end(), val);
        assert(it != vec.end());
        vec.erase(it);
    }
    template<class T, class Predicate>
    void erase_if(std::vector<T>& vec, Predicate&& p){
      auto it = std::remove_if(vec.begin(), vec.end(), std::forward<Predicate>(p));
      vec.erase(it, vec.end());
    }
    template<class T>
    void transfer_elements(std::vector<T>& target, std::vector<T>&& source){
        target.reserve(target.size() + source.size());
        for(auto& t : source){
            target.push_back(std::move(t));
        }
        source.clear();
    }
    template<class T, class F, class result_type = std::invoke_result_t<F&, T const&> >
    std::vector<result_type> transform_vector(std::vector<T> const& in, F&& func){
        std::vector<result_type> ret;
        ret.reserve(in.size());
        for(auto const& t : in) ret.push_back(func(t));
        return ret;
    }

    /* variant helpers */

    namespace detail{
        template<class ret_type, class visitor_t>
        using func_instantiation = ret_type(*)(visitor_t&&);
        template<class ret_type, class visitor_t, std::size_t... indices> requires requires{ {invoke(std::declval<visitor_t&&>(), value_flag<indices>{}...) } -> std::convertible_to<ret_type>; }
        constexpr func_instantiation<ret_type, visitor_t> get_entry_list(std::index_sequence<>, std::index_sequence<indices...>) {
            return [](visitor_t&& visitor){ return invoke(std::forward<visitor_t>(visitor)(value_flag<indices>{}...)); };
        }
        template<class ret_type, class visitor_t, std::size_t first_max, std::size_t... max, std::size_t... indices>
        constexpr auto get_entry_list(std::index_sequence<first_max, max...>, std::index_sequence<indices...>){
            using next_type = decltype(gen_entry_list<ret_type, visitor_t>(std::index_sequence<max...>{}, std::index_sequence<indices..., 0>{}));
            return generate_static_array<next_type, first_max>([]<std::size_t index>(value_flag<index>){
                get_entry_list<ret_type, visitor_t>(std::index_sequence<max...>{}, std::index_sequence<indices..., index>{});
            });
        }
        template<class arr>
        constexpr decltype(auto) get_nested_index(arr const& a){
            return a;
        }
        template<class arr, class... indices_t>
        constexpr decltype(auto) get_nested_index(arr const& a, std::size_t index, indices_t... indices){
            return get_nested_index(a[index], indices...);
        }
    }
    template<class ret_type, std::size_t... max, class visitor_t, class... indices_t>
    constexpr auto visit_by_index(visitor_t&& visitor, indices_t... indices) requires (sizeof...(max) == sizeof...(indices) && (std::is_convertible_v<indices_t, std::size_t> && ...)){
        static_assert(((max > 0) && ...), "the maximum indices must be positive.");
        assert(((indices < max) && ...));
        constexpr auto alternative_array = detail::get_entry_list<ret_type, visitor_t>(std::index_sequence<max...>{}, std::index_sequence<>{});
        return get_nested_index(alternative_array, indices...)(std::forward<visitor_t>(visitor));
    }
    template<class callback_t, class... Ts>
    constexpr auto variant_map(std::variant<Ts...>&& arg, callback_t&& callback) requires (requires{ invoke(std::forward<callback_t>(callback), std::declval<Ts&&>()); } && ...){
        using ret_type = std::variant<decltype(invoke(std::forward<callback_t>(callback), std::declval<Ts&&>()))...>;
        return visit_by_index<ret_type, sizeof...(Ts)>([&]<std::size_t index>(value_flag<index>){
            return ret_type{std::in_place_index<index>, invoke(std::forward<callback_t>(callback), std::move(std::get<index>(arg)))};
        }, arg.index());
    }
    template<class callback_t, class... Ts>
    constexpr auto variant_map(std::variant<Ts...> const& arg, callback_t&& callback) requires (requires{ invoke(std::forward<callback_t>(callback), std::declval<Ts const&>()); } && ...){
        using ret_type = std::variant<decltype(invoke(std::forward<callback_t>(callback), std::declval<Ts const&>()))...>;
        return visit_by_index<ret_type, sizeof...(Ts)>([&]<std::size_t index>(value_flag<index>){
            return ret_type{std::in_place_index<index>, invoke(std::forward<callback_t>(callback), std::get<index>(arg))};
        }, arg.index());
    }

    template<class T>
    struct final_action{
        T action;
        final_action(T action):action(std::move(action)){}
        final_action(final_action const&) = delete;
        final_action(final_action&&) = delete;
        final_action& operator=(final_action const&) = delete;
        final_action& operator=(final_action&&) = delete;
        ~final_action(){ action(); }
    };
    template<class T>
    final_action(T) -> final_action<T>;

    template<class T, auto ref, auto deref> requires requires(T& a){ invoke(ref, a); invoke(deref, a); }
    class managed_ptr{
        T* ptr;
    public:
        managed_ptr():ptr(nullptr){}
        managed_ptr(std::nullptr_t):ptr(nullptr){}
        managed_ptr(T* ptr):ptr(ptr){ if(ptr) invoke(ref, *ptr); }
        template<class... Args> requires std::is_constructible_v<T, Args&&...>
        managed_ptr(std::in_place_t, Args&&... args):managed_ptr(new T(std::forward<Args>(args)...)){}
        managed_ptr(managed_ptr const& o):ptr(o.ptr){ if(ptr) invoke(ref, *ptr); }
        managed_ptr(managed_ptr&& o):ptr(o.ptr){ o.ptr = nullptr; }
        managed_ptr& operator=(managed_ptr const& o){
            if(ptr) invoke(deref, *ptr);
            ptr = o.ptr;
            if(ptr) invoke(ref, *ptr);
            return *this;
        }
        managed_ptr& operator=(managed_ptr&& o){
            if(ptr) invoke(deref, *ptr);
            ptr = o.ptr;
            o.ptr = nullptr;
            return *this;
        }
        ~managed_ptr(){
            if(ptr) invoke(deref, *ptr);
        }
        T* get() const{
            return ptr;
        }
        std::size_t hash() const{
            return std::hash<T*>()(ptr);
        }
    };
    struct member_hasher{
        template<class T>
        std::size_t operator()(T const& a) const requires requires{ {a.hash()} -> std::convertible_to<std::size_t>; }{
            return a.hash();
        }
    };
    template<class A, class B>
    struct range_from_sentinel_pair{
        std::pair<A, B> inner;
        range_from_sentinel_pair(std::pair<A, B> inner):inner(inner){};
        A const& begin() const{ return inner.first; }
        B const& end() const{ return inner.second; }
    };

    template<class... Args>
    struct class_list{
        template<template<class...> class Head>
        using apply = Head<Args...>;
        template<template<class...> class Map>
        using transform = class_list<Map<Args>...>;
        template<template<class...> class Transform>
        using transform_dependent_type = class_list<typename Transform<Args>::type...>;
        template<class... Added>
        using append = class_list<Args..., Added...>;
        template<class... Added>
        using prepend = class_list<Added..., Args...>;
        using add_const = class_list<Args const...>;
        template<auto fn, class... parameters>
        using call_results = class_list<decltype(utility::invoke(fn, std::declval<Args>(), std::declval<parameters>()...))...>;
        template<auto fn, class... parameters>
        using call_flag_results = class_list<typename decltype(utility::invoke(fn, std::declval<Args>(), std::declval<parameters>()...))::type...>;
        using unique = typename detail::uniquer<class_list<Args...>, class_list<> >::type;
        template<class T>
        static constexpr bool contains = (std::is_same_v<T, Args> || ...);
        template<class T>
        static constexpr std::size_t count = (std::is_same_v<T, Args> + ...);
        template<class T>
        static constexpr bool contains_once = count<T> == 1;
        static constexpr std::size_t size = sizeof...(Args);
        using index_sequence = std::make_index_sequence<size>;
        template<std::size_t index>
        using nth_term = nth_parameter_in_pack<index, Args...>;
    };

    /*template<class T, class friend_type>
    class hidden{
    protected:
        T value;
        template<class... Args>
        hidden(Args&&... args):value(std::forward<Args>(args)...){}
        friend friend_type;
        template<class U>
        friend class std::hash;
    };*/


}
/*
namespace std{
    template<class T, class friend_type>
    struct hash<hidden<T, friend_type> >{
        std::hash<T> hasher;
        auto operator()(hidden<T, friend_type> const& value) const noexcept{
            return hasher(value.value);
        }
    };
}
*/
#endif /* TemplateUtility_h */
