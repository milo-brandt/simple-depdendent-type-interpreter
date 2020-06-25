#ifndef Exceptable_h
#define Exceptable_h

#include <exception>
#include <variant>
#include <functional>
#include <memory>
#include "TemplateUtility.h"

template<class T>
class exceptable{
    std::variant<T, std::exception_ptr> value;
public:
    exceptable(T v):value(std::move(v)){}
    exceptable(std::exception_ptr p):value(std::move(p)){}
    template<class callback, class = std::enable_if_t<std::is_invocable_r_v<T, callback&&> > >
    exceptable(callback&& c):value(std::in_place_index<1>){
        try{
            value = std::invoke(std::forward<callback>(c));
        }catch(...){
            value = std::current_exception();
        }
    }
    bool has_value() const{
        return value.index() == 0;
    }
    operator bool() const{
        return has_value();
    }
    T& operator*(){
        if(value.index() == 0) return std::get<0>(value);
        else std::rethrow_exception(std::get<1>(value));
    }
    T const& operator*() const{
        if(value.index() == 0) return std::get<0>(value);
        else std::rethrow_exception(std::get<1>(value));
    }
    T& get(){ return **this; }
    T const& get() const{ return **this; }
    T* operator->(){
        return &**this;
    }
    T const* operator->() const{
        return &**this;
    }
    std::exception_ptr get_exception() const{
        return std::get<1>(value);
    }
};
template<>
class exceptable<void>{
    std::optional<std::exception_ptr> value;
public:
    exceptable(std::exception_ptr p):value(std::move(p)){}
    template<class callback, class = std::enable_if_t<std::is_invocable_r_v<callback&&> > >
    exceptable(callback&& c):value(std::in_place_index<1>){
        try{
            value = std::invoke(std::forward<callback>(c));
        }catch(...){
            value = std::current_exception();
        }
    }
    bool has_value() const{
        return !value.has_value();
    }
    operator bool() const{
        return has_value();
    }
    void get() const{
        if(value) std::rethrow_exception(*value);
    }
    std::exception_ptr get_exception() const{
        return *value;
    }
};
template<class callback, std::enable_if_t<std::is_invocable_v<callback&&>, int> = 0>
exceptable(callback&&) -> exceptable<std::invoke_result_t<callback&&> >;
template<class T, std::enable_if_t<!std::is_invocable_v<T&&>, int> = 0>
exceptable(T) -> exceptable<T>;

class tuple_exception : public std::exception{
    std::unique_ptr<std::string> str = std::make_unique<std::string>();
public:
    std::vector<std::exception_ptr> components;
    tuple_exception(std::vector<std::exception_ptr> components):components(std::move(components)){}
    const char* what() const noexcept override;
};
template<class... callbacks> requires (std::is_invocable_v<callbacks&&> && ...)
auto make_tuple_or_throw(callbacks&&... calls){
    std::vector<std::exception_ptr> failures;
    auto raw = std::make_tuple(exceptable(std::forward<callbacks>(calls))...);
    utility::for_each_in_tuple(raw, [&failures](auto const& e){ if(!e) failures.push_back(e.get_exception()); });
    if(failures.size() == 0) return utility::transform_tuple(raw, [](auto& e){ return std::move(*e); });
    if(failures.size() == 1) std::rethrow_exception(failures[0]);
    throw tuple_exception(std::move(failures));
}

#endif
