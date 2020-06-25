/*
 For debugging and testing issues of lifetimes
 */

#ifndef LifetimeTracker_hpp
#define LifetimeTracker_hpp

struct LifetimeTracker{
    LifetimeTracker();
    LifetimeTracker(LifetimeTracker const&);
    LifetimeTracker(LifetimeTracker&&);
    LifetimeTracker& operator=(LifetimeTracker const&);
    LifetimeTracker& operator=(LifetimeTracker&&);
    ~LifetimeTracker();
};

#endif /* LifetimeTracker_hpp */
