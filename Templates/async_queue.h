//
//  async_queue.h
//  FactoryGame
//
//  Created by Milo Brandt on 2/14/20.
//  Copyright © 2020 Milo Brandt. All rights reserved.
//

#ifndef async_queue_h
#define async_queue_h

#include <functional> //For std::invoke
#include <atomic>
#include <thread>
#include <optional>
#include <mutex>
#include <condition_variable>

template<class T>
class async_queue{
    struct Node{
        T value;
        Node* next = nullptr; //Owns target
    };
    Node* first = nullptr; //Owns target
    Node* last; //If first is not nullptr, points to end
    std::mutex mutex;
    std::condition_variable nonempty;
public:
    void push(T value){
        Node* toAdd = new Node{std::move(value)};
        std::unique_lock<std::mutex> lock(mutex);
        if(first){
            last->next = toAdd;
            last = toAdd;
        }else{
            first = toAdd;
            last = toAdd;
            lock.unlock();
            nonempty.notify_all();
        }
    }
    template<class Callback, class = std::enable_if_t<std::is_invocable_v<Callback, T> > >
    void consume(Callback&& callback){
        Node* readFirst;
        {
            std::lock_guard<std::mutex> lock(mutex);
            readFirst = first; //Owns target!
            first = nullptr;
        }
        while(readFirst){
            std::invoke(callback, std::move(readFirst->value));
            Node* toDelete = readFirst;
            readFirst = readFirst->next;
            delete toDelete;
        }
    }
    T popFront(){
        waitForNonEmpty();
        return *tryPopFront();
    }
    std::optional<T> tryPopFront(){
        Node* node;
        {
            std::lock_guard<std::mutex> lock(mutex);
            node = first;
            if(first)
                first = first->next;
        }
        std::optional<T> ret;
        if(node){
            ret = std::move(node->value);
            delete node;
        }
        return ret;
    }
    void waitForNonEmpty(){
        std::unique_lock<std::mutex> lock(mutex);
        nonempty.wait(lock, [this](){ return first != nullptr; });
    }
    bool empty(){
        std::lock_guard<std::mutex> lock(mutex);
        return first == nullptr;
    }
};

#endif /* async_queue_h */
