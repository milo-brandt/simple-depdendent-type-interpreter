#ifndef MDB_HANDLED_VECTOR
#define MDB_HANDLED_VECTOR

#include <memory>
#include <span>

namespace mdb {
  //Handler must have two members:
  //1. handler.move_destroy(std::span<T> new_span, std::span<T> old_span);
  //  This method is called with a newly allocated (but not constructed) span
  //  of Ts (new_span), along with an existing span of valid Ts (old_span).
  //  It must move all elements from old_span to new_span.
  //2. handler.destroy(std::span<T> old_span);
  //  This method simply destroys the targetted span.
  template<class T, class Handler>
  class HandledVector {
    Handler p_handler;
    T* p_begin = nullptr;
    T* p_end = nullptr;
    T* p_capacity = nullptr;
    void initialize(std::size_t capacity) { //capacity must be > 0
      p_begin = (T*)malloc(sizeof(T) * capacity);
      p_end = p_begin;
      p_capacity = p_begin + capacity;
    }
    void realloc(std::size_t new_capacity) { //capacity must be positive and bigger than length
      auto new_begin = (T*)malloc(sizeof(T) * new_capacity);
      auto new_end = new_begin + (p_end - p_begin);
      if(p_begin) {
        p_handler.move_destroy(
          std::span<T>{new_begin, new_end},
          std::span<T>{p_begin, p_end}
        );
        free(p_begin);
      }
      p_begin = new_begin;
      p_end = new_end;
      p_capacity = new_begin + new_capacity;
    }
    void dealloc() {
      if(!p_begin) return;
      p_handler.destroy(std::span<T>{p_begin, p_end});
      free(p_begin);
      p_begin = nullptr;
    }
  public:
    HandledVector(Handler handler):p_handler(std::move(handler)) {}
    HandledVector(HandledVector&& other):p_begin(other.p_begin),p_end(other.p_end),p_capacity(other.p_capacity) {
      other.p_begin = nullptr;
    }
    HandledVector(HandledVector const&) = delete;
    HandledVector& operator=(HandledVector&& other) {
      std::swap(p_begin, other.p_begin);
      std::swap(p_end, other.p_end);
      std::swap(p_capacity, other.p_capacity);
      return *this;
    }
    HandledVector& operator=(HandledVector const& other) = delete;
    ~HandledVector() {
      dealloc();
    }
    T& operator[](std::size_t index) { return p_begin[index]; }
    T const& operator[](std::size_t index) const { return p_begin[index]; }
    std::size_t size() const {
      return p_end - p_begin;
    }
    std::size_t capacity() const {
      return p_capacity - p_begin;
    }
    std::size_t push_back(T value) { //returns index
      auto next_index = size();
      if(p_capacity == p_end) {
        realloc(next_index * 3 / 2 + 16); //resize factor 1.5
      }
      new (p_end) T{std::move(value)};
      ++p_end;
      return next_index;
    }
    T* begin() {
      return p_begin;
    }
    T* end() {
      return p_end;
    }
    T const* begin() const {
      return p_begin;
    }
    T const* end() const {
      return p_end;
    }
  };
}

#endif
