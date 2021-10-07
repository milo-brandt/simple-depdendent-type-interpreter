#ifndef MDB_EVENT_HPP
#define MDB_EVENT_HPP

#include <utility>

namespace mdb {
  template <class... Args>
  class Event{
  public:
    class Handle;
  private:
    struct DataNode { //An element of a doubly linked list (always linked in a circle)
      DataNode* next;
      DataNode* last;
      void disconnect() {
        last->next = next;
        next->last = last;
      }
    };
    struct FunctionNode : DataNode { //a node containing a function
      virtual void call(Args const&...) = 0;
      virtual ~FunctionNode() = default;
    };
    template<class F>
    struct FunctionImpl : FunctionNode { //a specific implementation of a function.
      F function;
      FunctionImpl(F function):function(std::move(function)) {}
      void call(Args const&... args) {
        function(args...);
      }
    };
    template<class Callback>
    void walk_nodes(Callback&& callback) {
      DataNode walker{ //create a new node which crawls around the list
                       //this allows safe deletion of executing nodes, if they desire
        .next = root.next,
        .last = &root
      };
      root.next = &walker;
      walker.next->last = &walker;
      while(walker.next != &root) {
        auto target = (FunctionNode*)walker.next;
        walker.next = target->next;
        target->last = walker.last;
        walker.last->next = target;
        walker.last = target;
        target->next->last = &walker;
        target->next = &walker;
        callback(target);
      }
      walker.disconnect();
    }
    DataNode root;
  public:
    Event():root{.next = &root, .last = &root} {}
    Event(Event const&) = delete;
    Event(Event&& other):root(other.root) {
      //replace other.root with our root in the linked list.
      //point other.root in a loop
      other.root.next->last = &root;
      other.root.last->next = &root;
      other.root.next = &other.root;
      other.root.last = &other.root;
    }
    Event& operator=(Event&& other) {
      Event tmp{std::move(other)};
      this->~Event();
      new (this) Event{std::move(tmp)};
      return *this;
    }
    ~Event() {
      walk_nodes([&](FunctionNode* node) {
        node->disconnect();
        delete node;
      });
    }
    class UniqueHook;
    class Hook {
      FunctionNode* node = nullptr;
      friend Event;
      friend UniqueHook;
      explicit Hook(FunctionNode* node):node(node) {}
    public:
      Hook() = default;
      void disconnect() && { if(node) { node->disconnect(); delete node; node = nullptr; } }
    };
    class UniqueHook {
      FunctionNode* node = nullptr;
      friend Event;
    public:
      explicit UniqueHook(Hook hook):node(hook.node) {}
      UniqueHook(UniqueHook&& o):node(o.node) { o.node = nullptr; }
      UniqueHook& operator=(UniqueHook&& o) {
        std::swap(node, o.node);
      }
      Hook get() const { return Hook{node}; }
      void detach() { node = nullptr; }
      void disconnect() && { if(node) { node->disconnect(); delete node; node = nullptr; } }
      ~UniqueHook() { std::move(*this).disconnect(); }
    };
    /*
      Note: returned hooks have lifetime until either disconnection or until the event is destroyed.
    */
    template<class F>
    Hook listen(F function) {
      auto new_node = new FunctionImpl<F>{std::move(function)};
      new_node->next = &root;
      new_node->last = root.last;
      root.last->next = new_node;
      root.last = new_node;
      return Hook{new_node};
    }
    template<class F>
    UniqueHook listen_unique(F function) {
      auto basic_hook = listen(std::move(function));
      return UniqueHook{basic_hook};
    }

    void operator()(Args const&... args) {
      walk_nodes([&](FunctionNode* node) {
        node->call(args...);
      });
    }
  };
}

#endif /* Event_hpp */
