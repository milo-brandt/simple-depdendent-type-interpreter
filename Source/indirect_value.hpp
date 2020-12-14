//
//  indirect_value.h
//  CAM
//
//  Created by Milo Brandt on 3/22/20.
//  Copyright © 2020 Milo Brandt. All rights reserved.
//

#ifndef indirect_value_h
#define indirect_value_h

#include <memory>
#include <utility>

template<class T>
class indirect_value{
    std::unique_ptr<T> value;
public:
    template<class... Args>
    indirect_value(Args&&... args):value(new T(std::forward<Args>(args)...)){}
    indirect_value(indirect_value const& other):indirect_value(*other){}
    indirect_value(indirect_value&& other) = default;
    indirect_value& operator=(T&& data){ value = std::make_unique<T>(std::move(data)); return *this; }
    indirect_value& operator=(T const& data){ value = std::make_unique<T>(data); return *this; }
    indirect_value& operator=(indirect_value const& other){ return *this = *other; }
    indirect_value& operator=(indirect_value&&) = default;
    T& operator*() &{ return *value; }
    T const& operator*() const&{ return *value; }
    T&& operator*() &&{ return std::move(*value); }
    T* get(){ return value.get(); }
    T const* get() const{ return value.get(); }
    T* operator->(){ return get(); }
    T const* operator->() const{ return get(); }
};
template<class T>
indirect_value(T const&) -> indirect_value<T>;
template<class T>
indirect_value(T&&) -> indirect_value<T>;

template<class T>
class indirect_optional{
    std::unique_ptr<T> value;
public:
    indirect_optional() = default;
    indirect_optional(std::nullopt_t){}
    template<class... Args>
    indirect_optional(std::in_place_t, Args&&... args):value(new T(std::forward<Args>(args)...)){}
    indirect_optional(T const& value):indirect_optional(std::in_place, value){}
    indirect_optional(T&& value):indirect_optional(std::in_place, value){}
    indirect_optional(indirect_optional const& other){
        if(other) *this = *other;
    }
    indirect_optional(indirect_optional&& other) = default;
    indirect_optional& operator=(T&& data){ value = std::make_unique<T>(std::move(data)); return *this; }
    indirect_optional& operator=(T const& data){ value = std::make_unique<T>(data); return *this; }
    indirect_optional& operator=(indirect_optional const& other){
        if(other) return *this = *other;
        else{
            value = nullptr;
            return *this;
        }
    }
    indirect_optional& operator=(indirect_optional&&) = default;
    T& operator*() &{ return *value; }
    T const& operator*() const&{ return *value; }
    T&& operator*() &&{ return std::move(*value); }
    T* get(){ return value.get(); }
    T const* get() const{ return value.get(); }
    T* operator->(){ return get(); }
    T const* operator->() const{ return get(); }
    bool has_value() const{ return value != nullptr; }
    operator bool() const{ return value != nullptr; }
};
template<class T>
indirect_optional(T const&) -> indirect_optional<T>;
template<class T>
indirect_optional(T&&) -> indirect_optional<T>;


#endif /* indirect_value_h */
