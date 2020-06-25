#ifndef IndirectVariant_h
#define IndirectVariant_h

#include <array>
#include <memory>
#include <utility>
#include "TemplateUtility.h"
#include <functional>
#include <cassert>

namespace detail{
    template<class T>
    struct simple_caller{
        T operator()(T const&) const;
        T operator()(T&&) const;
    };
    template<class... Types>
    using call_model = utility::overloaded<simple_caller<Types>...>;
    template<class T, class... Types>
    using call_result = std::invoke_result_t<call_model<Types...>, T>;
};

struct callback_construct_t{};
constexpr callback_construct_t callback_construct;
struct tuple_construct_t{};
constexpr tuple_construct_t tuple_construct;

template<class... Types>
class heap_variant{
    static constexpr std::size_t type_count = sizeof...(Types);
    template<std::size_t index> requires (index < type_count)
    using nth_type = utility::nth_parameter_in_pack<index, Types...>;
    template<class Type>
    static constexpr bool appears_once = utility::pack_includes_unique<Type, Types...>;
    template<class Type> requires appears_once<Type>
    static constexpr std::size_t index_of = utility::last_index_in_pack<Type, Types...>;

    heap_variant(std::size_t index):index(index){}
    template<std::size_t i>
    class derived;
    template<class Ret, class F>
    Ret internal_visit(F&& f){
        static constexpr auto call_table = utility::generate_static_array<Ret(*)(heap_variant<Types...>*, F&), sizeof...(Types)>([]<std::size_t i>(utility::value_flag<i>){
            return [](heap_variant<Types...>* ptr, F& f){
                auto base = (derived<i>*)ptr;
                return f(base);
            };
        });
        return call_table[index](this, f);
    }
    template<class Ret, class F>
    Ret internal_visit(F&& f) const{
        static constexpr auto call_table = utility::generate_static_array<Ret(*)(heap_variant<Types...> const*, F&), sizeof...(Types)>([]<std::size_t i>(utility::value_flag<i>){
            return [](heap_variant<Types...> const* ptr, F& f){
                auto base = (derived<i> const*)ptr;
                return f(base);
            };
        });
        return call_table[index](this, f);
    }
public:

    std::size_t const index;

    heap_variant() = delete; //may only be constructed through base class.
    heap_variant(heap_variant&&) = delete;
    heap_variant(heap_variant const&) = delete;
    heap_variant& operator=(heap_variant&&) = delete;
    heap_variant& operator=(heap_variant const&) = delete;
    template<std::size_t index, class... Args> requires (0 <= index && index < type_count/* && std::is_constructible_v<nth_type<index>, Args&&...>*/)
    static heap_variant* make(std::in_place_index_t<index>, Args&&... args){
        return new derived<index>(std::forward<Args>(args)...);
    }
    template<class T, class... Args> requires (appears_once<T>/* && std::is_constructible_v<T, Args&&...>*/)
    static heap_variant* make(std::in_place_type_t<T>, Args&&... args){
        return make(std::in_place_index<index_of<T> >, std::forward<Args>(args)...);
    }
    template<class X, class T = detail::call_result<X&&, Types...> >
    static heap_variant* make(X&& x){
        return make(std::in_place_type<T>, std::forward<X>(x));
    }
    ~heap_variant(){
        internal_visit<void>([]<std::size_t i>(derived<i>* ptr){ ptr->_destroy(); });
    }
    template<class Visitor, class Ret = std::common_type_t<std::invoke_result_t<Visitor, Types&>... > >
    Ret visit_upon(Visitor&& visitor){
        return internal_visit<Ret>([&visitor]<std::size_t i>(derived<i>* d) -> Ret{
            return visitor(d->u.value);
        });
    }
    template<class Visitor, class Ret = std::common_type_t<std::invoke_result_t<Visitor, Types const&>... > >
    Ret visit_upon(Visitor&& visitor) const{
        return internal_visit<Ret>([&visitor]<std::size_t i>(derived<i> const* d) -> Ret{
            return visitor(d->u.value);
        });
    }
    template<class T> requires appears_once<T>
    bool holds_alternative() const{
        return index == index_of<T>;
    }
    template<class T> requires appears_once<T>
    T& get(){
        assert(holds_alternative<T>());
        return ((derived<index_of<T> >*)this)->u.value;
    }
    template<std::size_t i> requires (0 <= i && i < type_count)
    nth_type<i>& get(){
        assert(index == i);
        return ((derived<i>*)this)->u.value;
    }
    template<class T> requires appears_once<T>
    T const& get() const{
        assert(holds_alternative<T>());
        return ((derived<index_of<T> > const*)this)->u.value;
    }
    template<std::size_t i> requires (0 <= i && i < type_count)
    nth_type<i> const& get() const{
        assert(index == i);
        return ((derived<i>*)this)->u.value;
    }
    heap_variant* clone() const{
        return internal_visit<heap_variant*>([]<std::size_t i>(derived<i> const* d){
            return make(std::in_place_index<i>, d->u.value);
        });
    }
};
template<class... Types>
template<std::size_t i>
class heap_variant<Types...>::derived : public heap_variant{
   using T = nth_type<i>;
   friend heap_variant;
   template<class... Args>
   derived(Args&&... args):heap_variant(i), u{.value{std::forward<Args>(args)...}}{}
   template<class... Ts, std::size_t... is>
   derived(tuple_construct_t, std::tuple<Ts...>&& t, std::index_sequence<is...>):derived(std::get<is>(t)...){}
   template<class... Ts>
   derived(tuple_construct_t tag, std::tuple<Ts...>&& t):derived(tag, std::move(t), std::index_sequence_for<Ts...>{}){}
   template<class arg_getter>
   derived(callback_construct_t, arg_getter&& getter):derived(tuple_construct_t{}, std::forward<arg_getter>(getter)(this)){}
   union managed_init{ T value; ~managed_init(){} };
   managed_init u;
   void _destroy(){ u.value.~T(); }
};

template<class... Types>
class indirect_variant{
    std::unique_ptr<heap_variant<Types...> > val;
public:
    indirect_variant() = default;
    indirect_variant(std::nullptr_t){}
    template<class... Args> requires requires(Args&&... args){ heap_variant<Types...>::make(std::forward<Args>(args)...); }
    indirect_variant(Args&&... args):val(heap_variant<Types...>::make(std::forward<Args>(args)...)){}
    indirect_variant(indirect_variant&&) = default;
    indirect_variant(indirect_variant const& o):val(o.val ? o.val->clone() : nullptr){}
    indirect_variant& operator=(indirect_variant&&) = default;
    indirect_variant& operator=(indirect_variant const& o){
        if(o.val) val.reset(o.val->clone());
        else val.reset();
    }
    heap_variant<Types...>* get(){ return val.get(); }
    heap_variant<Types...> const* get() const{ return val.get(); }
    heap_variant<Types...>& operator*(){ return *get(); }
    heap_variant<Types...> const& operator*() const{ return *get(); }
    heap_variant<Types...>& operator->(){ return *get(); }
    heap_variant<Types...> const& operator->() const{ return *get(); }
    operator bool() const{ return val != nullptr; }
};
template<class... Types>
class shared_variant{
    std::shared_ptr<heap_variant<Types...> > val;
public:
    shared_variant() = default;
    shared_variant(std::nullptr_t){}
    template<class... Args> requires requires(Args&&... args){ heap_variant<Types...>::make(std::forward<Args>(args)...); }
    shared_variant(Args&&... args):val(heap_variant<Types...>::make(std::forward<Args>(args)...)){}
    heap_variant<Types...>* get(){ return val.get(); }
    heap_variant<Types...> const* get() const{ return val.get(); }
    heap_variant<Types...>& operator*(){ return *get(); }
    heap_variant<Types...> const& operator*() const{ return *get(); }
    heap_variant<Types...>& operator->(){ return *get(); }
    heap_variant<Types...> const& operator->() const{ return *get(); }
    operator bool() const{ return val != nullptr; }
};

#endif
