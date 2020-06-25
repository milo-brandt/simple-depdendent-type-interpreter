//
//  LifetimeTracker.cpp
//  GenericPlayground2
//
//  Created by Milo Brandt on 3/29/20.
//  Copyright © 2020 Milo Brandt. All rights reserved.
//

#include "LifetimeTracker.hpp"
#include <iostream>

LifetimeTracker::LifetimeTracker(){
    std::cout << "Tracker default constructed at " << this << "\n";
}
LifetimeTracker::LifetimeTracker(LifetimeTracker const& other){
    std::cout << "Tracker copied constructed from " << &other << " to " << this << ".\n";
}
LifetimeTracker::LifetimeTracker(LifetimeTracker&& other){
    std::cout << "Tracker move constructed from " << &other << " to " << this << ".\n";

}
LifetimeTracker& LifetimeTracker::operator=(LifetimeTracker const& other){
    std::cout << "Tracker copied assigned from " << &other << " to " << this << ".\n";
    return *this;
}
LifetimeTracker& LifetimeTracker::operator=(LifetimeTracker&& other){
    std::cout << "Tracker move assigned from " << &other << " to " << this << ".\n";
    return *this;
}
LifetimeTracker::~LifetimeTracker(){
    std::cout << "Tracker destructed at " << this << ".\n";
}
