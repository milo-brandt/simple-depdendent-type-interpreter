#include <memory>

template<class Ptr>
auto indirectFunction(Ptr&& ptr){
    return [ptr = std::forward<Ptr>(ptr)](auto&&... args){
        return std::invoke(*ptr, std::forward<decltype(args)>(args)...);
    };
}
template<class Val>
auto sharedFunction(Val&& val){
    return indirectFunction(std::make_shared<std::remove_reference_t<Val> >(std::forward<Val>(val)));
}
/*
template<class Ptr, class Member>
auto indirectFunction(Ptr&& ptr, Member&& member){
    return [ptr = std::forward<Ptr>(ptr), member = std::forward<Member>(member)](auto&&... args){
        return std::invoke(member, *ptr, std::forward<decltype(args)>(args)...);
    };
}
template<class T>
auto weakIndirectFunction(std::weak_ptr<T> ptr){
    return [ptr = std::move(ptr)](auto&&... args){
        if(auto locked = ptr.lock()){
            std::invoke(*locked, std::forward<decltype(args)>(args)...);
            return EventAction::NORMAL;
        }else{
            return EventAction::DISCONNECT;
        }
    };
}
template<class T, class Member>
auto weakIndirectFunction(std::weak_ptr<T> ptr, Member&& member){
    return [ptr = std::move(ptr), member = std::forward<Member>(member)](auto&&... args){
        if(auto locked = ptr.lock()){
            std::invoke(member, *locked, std::forward<decltype(args)>(args)...);
            return EventAction::NORMAL;
        }else{
            return EventAction::DISCONNECT;
        }
    };
}

template<class T>
auto weakIndirectFunction(std::shared_ptr<T> ptr){
    return weakIndirectFunction(std::weak_ptr<T>(ptr));
}
template<class T, class Member>
auto weakIndirectFunction(std::shared_ptr<T> ptr, Member&& member){
    return weakIndirectFunction(std::weak_ptr<T>(ptr), std::forward<Member>(member));
}
*/
