//
//  Vector.h
//  CAM
//
//  Created by Milo Brandt on 3/17/20.
//  Copyright © 2020 Milo Brandt. All rights reserved.
//

#ifndef Vector_h
#define Vector_h

#include <array>
#include <utility>
#include <cmath>
#include <initializer_list>

struct CallbackConstruct_t{};
constexpr CallbackConstruct_t CallbackConstruct;
template<class T, std::size_t d>
class Vector{
    T data[d];
    struct CallbackConstructImpl_t{};
    static constexpr CallbackConstructImpl_t CallbackConstructImpl;
    template<class Callback, std::size_t... indices>
    Vector(CallbackConstructImpl_t, Callback&& callback, std::index_sequence<indices...>):data{callback(indices)...}{}
public:
    Vector():data{}{}
    template<class... Args, class = std::enable_if_t<sizeof...(Args) == d && (std::is_convertible_v<Args, T> && ...)> >
    Vector(Args&&... args):data{(T)std::forward<Args>(args)...}{}
    template<class Callback>
    Vector(CallbackConstruct_t, Callback&& callback):Vector(CallbackConstructImpl, std::forward<Callback>(callback), std::make_index_sequence<d>{}){}
    template<class T2, class = std::enable_if_t<std::is_convertible_v<T2, T> > >
    Vector(Vector<T2, d> const& other):Vector(CallbackConstruct, [&other](std::size_t i) -> T{ return other[i]; }){}
    Vector(Vector const&) = default;
    Vector(Vector&&) = default;
    Vector& operator=(Vector const&) = default;
    Vector& operator=(Vector&&) = default;
    T& operator[](std::size_t n){ assert(n < d); return data[n]; }
    T const& operator[](std::size_t n) const{ assert(n < d); return data[n]; }
    Vector operator-() const{ return {CallbackConstruct, [this](std::size_t i){ return -data[i]; }}; }
    Vector& operator+=(Vector const& other){
        for(std::size_t i = 0; i < d; ++i) data[i] += other.data[i];
        return *this;
    }
    Vector& operator-=(Vector const& other){
        for(std::size_t i = 0; i < d; ++i) data[i] -= other.data[i];
        return *this;
    }
    Vector& operator*=(T const& value){
        for(std::size_t i = 0; i < d; ++i) data[i] *= value;
        return *this;
    }
    Vector& operator/=(T const& value){
        for(std::size_t i = 0; i < d; ++i) data[i] /= value;
        return *this;
    }
    T dot(Vector const& other) const{
        T ret = 0;
        for(std::size_t i = 0; i < d; ++i) ret += data[i] * other.data[i];
        return ret;
    }
    T magnitudeSq() const{
        T ret = 0;
        for(std::size_t i = 0; i < d; ++i) ret += data[i] * data[i];
        return ret;
    }
    T magnitude() const{
        return std::sqrt(magnitudeSq());
    }
    Vector normalized() const{
        return *this / magnitude();
    }
    Vector& normalize(){
        return this /= magnitude();
    }
    template<class U>
    auto normalize(U const& len){
        return len * (Vector<U, d>(*this).normalize());
    }
    template<std::size_t dp = d, class = std::enable_if_t<dp == 2> >
    T angle() const{
        return std::atan2(data[1], data[0]);
    }
    template<std::size_t dp = d, class = std::enable_if_t<dp == 2> >
    static Vector fromAngle(T angle, T radius = 1){
        return {std::cos(angle) * radius, std::sin(angle) * radius};
    }
    template<std::size_t dp = d, class = std::enable_if_t<dp == 2> >
    Vector rotateCCW() const{ //Positive orientation
        return {-data[1], data[0]};
    }
    template<std::size_t dp = d, class = std::enable_if_t<dp == 2> >
    Vector rotateCW() const{
        return {data[1], -data[0]};
    }
    template<class U>
    auto lerpTo(Vector const& other, U const& t){
        return (1-t)**this + t*other;
    }
};
template<class T>
using Vector2 = Vector<T, 2>;
using Vector2d = Vector2<double>;
using Vector2f = Vector2<float>;
using Vector2i = Vector2<int>;

template<class Callback, std::size_t d, class... Args, class R = decltype(std::declval<Callback>()(std::declval<Args const&>()...))>
Vector<R, d> mapVector(Callback&& callback, Vector<Args, d> const&... argVecs){
    return {CallbackConstruct, [&callback, argVecs = std::make_tuple(std::forward<Args>(argVecs) ...)](std::size_t i){
        return std::apply([&callback, i](auto&& ... argVecs){
            return callback(argVecs[i]...);
        }, std::move(argVecs));
    }};
}
template<class T1, class T2, std::size_t d, class R = decltype(std::declval<T1>() + std::declval<T2>())>
Vector<R, d> operator+(Vector<T1, d> const& a, Vector<T2, d> const& b){
    return {CallbackConstruct, [&a, &b](std::size_t i){ return a[i] + b[i]; }};
}
template<class T1, class T2, std::size_t d, class R = decltype(std::declval<T1>() - std::declval<T2>())>
Vector<R, d> operator-(Vector<T1, d> const& a, Vector<T2, d> const& b){
    return {CallbackConstruct, [&a, &b](std::size_t i){ return a[i] - b[i]; }};
}
template<class T1, class T2, std::size_t d, class R = decltype(std::declval<T1>() * std::declval<T2>())>
Vector<R, d> operator*(T1 const& a, Vector<T2, d> const& b){
    return {CallbackConstruct, [&a, &b](std::size_t i){ return a * b[i]; }};
}
template<class T1, class T2, std::size_t d, class R = decltype(std::declval<T1>() * std::declval<T2>())>
Vector<R, d> operator*(Vector<T1, d> const& a, T2 const& b){
    return {CallbackConstruct, [&a, &b](std::size_t i){ return a[i] * b; }};
}
template<class T1, class T2, std::size_t d, class R = decltype(std::declval<T1>() / std::declval<T2>())>
Vector<R, d> operator/(Vector<T1, d> const& a, T2 const& b){
    return {CallbackConstruct, [&a, &b](std::size_t i){ return a[i] * b; }};
}
template<class T1, class T2, std::size_t d, class = std::enable_if_t<std::is_same_v<bool, decltype(std::declval<T1>() == std::declval<T2>())> > >
bool operator==(Vector<T1, d> const& a, Vector<T2, d> const& b){
    for(int i = 0; i < d; ++i) if(!(a[i] == b[i])) return false;
    return true;
}


#endif /* Vector_h */
