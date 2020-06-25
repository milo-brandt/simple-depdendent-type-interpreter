//
//  Cubic.h
//  FactoryGame
//
//  Created by Milo Brandt on 3/28/20.
//  Copyright © 2020 Milo Brandt. All rights reserved.
//

#ifndef Cubic_h
#define Cubic_h

#include <array>

template<class T>
struct Cubic{
    std::array<T, 4> coefficients;
    Cubic(double a0 = 0, double a1 = 0, double a2 = 0, double a3 = 0):coefficients{a0, a1, a2, a3}{}
    T evaluate(T const& input){
        return coefficients[0] + input*(coefficients[1] + input*(coefficients[2] + input*coefficients[3]));
    }
    T evaluateDerivative(T const& input){
        return coefficients[1] + input*(2*coefficients[2] + input*3*coefficients[3]);
    }
    struct ControlPoint{
        T x;
        T value;
        T rate;
    };
    static Cubic fromControls(ControlPoint a, ControlPoint b){ //Precomputed formula in mathematica.
        T tmp = a.x - b.x;
        T denominator = tmp*tmp*tmp;
        return {
            (b.x*(a.x*(-a.x + b.x)*(b.rate*a.x + a.rate*b.x) - b.x*(-3*a.x + b.x)*a.value) + a.x*a.x*(a.x - 3*b.x)*b.value)/denominator,
            (b.rate*a.x*(a.x - b.x)*(a.x + 2*b.x) - b.x*(a.rate*(-2*a.x*a.x + a.x*b.x + b.x*b.x) + 6*a.x*(a.value - b.value)))/denominator,
            (-(a.rate*(a.x - b.x)*(a.x + 2*b.x)) + b.rate*(-2*a.x*a.x + a.x*b.x + b.x*b.x) + 3*(a.x + b.x)*(a.value - b.value))/denominator,
            ((a.rate + b.rate)*(a.x - b.x) - 2*a.value + 2*b.value)/denominator
        };
    }
};

#endif /* Cubic_h */
