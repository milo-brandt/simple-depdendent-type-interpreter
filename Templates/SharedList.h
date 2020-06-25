//
//  List.h
//  HandyTemplates
//
//  Created by Milo Brandt on 4/15/20.
//  Copyright © 2020 Milo Brandt. All rights reserved.
//

#ifndef List_h
#define List_h

#include <memory>
#include <optional>

template<class T>
class SharedList{
    struct Joint{
        std::shared_ptr<Joint> next;
        std::shared_ptr<Joint>* last; //Not null if not ejected.
        std::optional<T> data;
        
        Joint(Joint const&) = delete; //Occupy actual memory - not to be moved.
        Joint(Joint&&) = delete;
        Joint& operator=(Joint const&) = delete;
        Joint& operator=(Joint&&) = delete;

        Joint(std::shared_ptr<Joint>* last):last(last){}
        template<class... Args>
        Joint(std::in_place_t, Args&&... args):data{std::in_place, std::forward<Args>(args)...}{};
        void eject(){
            if(last){
                next->last = last;
                *last = std::move(next);
                last = nullptr;
            }
        }
    };
    std::shared_ptr<Joint> _first = std::make_shared<Joint>(&_first);
    std::shared_ptr<Joint> _end = _first;
public:
    SharedList() = default;
    SharedList(SharedList const&) = delete;
    SharedList(SharedList&& other):_first(std::move(other._first)),_end(std::move(other._end)){
        _first->last = &_first;
    }
    SharedList& operator=(SharedList const&) = delete;
    SharedList& operator=(SharedList&& other){
        this->~SharedList();
        new (this) SharedList(std::move(other));
        return *this;
    }
    ~SharedList(){
        std::shared_ptr<Joint> position = std::move(_first);
        while(position){
            position->last = nullptr;
            position = std::move(position->next);
        }
        _end = nullptr;
    }
    class Iterator{
        friend class SharedList;
        std::shared_ptr<Joint> internal;
        Iterator(std::shared_ptr<Joint> const& internal):internal(internal){}
        Iterator(std::shared_ptr<Joint>&& internal):internal(std::move(internal)){}
    public:
        Iterator() = default;
        bool valid() const{
            return internal != nullptr;
        }
        bool connected() const{
            return internal != nullptr && internal->last != nullptr;
        }
        void eject(){
            if(internal){
                internal->eject();
                internal = nullptr;
            }
        }
        T& getValue() const{
            assert(internal);
            return *internal->data;
        }
        T& operator*() const{
            return getValue();
        }
        T* operator->() const{
            return &getValue();
        }
        bool operator==(Iterator const& other) const{
            return other.internal == internal;
        }
        bool operator!=(Iterator const& other) const{
            return other.internal != internal;
        }
        Iterator& operator++(){
            internal = internal->next;
            return *this;
        }
        Iterator operator++(int){
            auto ret = *this;
            ++(*this);
            return ret;
        }
    };
    template<class... Args>
    Iterator emplaceFront(Args&&... args){
        auto joint = std::make_shared<Joint>(std::in_place, std::forward<Args>(args)...);
        _first->last = &(joint->next = _first);
        joint->last = &(_first = joint);
        return {std::move(joint)};
    }
    template<class... Args>
    Iterator emplaceBack(Args&&... args){
        auto joint = std::make_shared<Joint>(std::in_place, std::forward<Args>(args)...);
        joint->last = &(*_end->last = joint);
        _end->last = &(joint->next = _end);
        return {std::move(joint)};
    }
    Iterator pushFront(T const& data){ return emplaceFront(data); }
    Iterator pushFront(T&& data){ return emplaceFront(std::move(data)); }
    Iterator pushBack(T const& data){ return emplaceBack(data); }
    Iterator pushBack(T&& data){ return emplaceBack(std::move(data)); }
    Iterator begin(){ return {_first}; }
    Iterator end(){ return {_end}; }
};

#endif /* List_h */
