#ifndef MDB_GRID_HPP
#define MDB_GRID_HPP

#include <stdlib.h>
#include <utility>
#include <stdexcept>
#include <type_traits>

namespace mdb {
  template<class T>
  class Grid {
    T* p_data; //stored in columns
    std::size_t p_width;
    std::size_t p_height;
    Grid(T* p_data, std::size_t p_width, std::size_t p_height):p_data(p_data), p_width(p_width), p_height(p_height) {}
  public:
    Grid(std::size_t width, std::size_t height, T const& default_value):p_width(width), p_height(height) {
      p_data = (T*)aligned_alloc(alignof(T), sizeof(T) * p_width * p_height);
      for(std::size_t y = 0; y < p_height; ++y) {
        for(std::size_t x = 0; x < p_width; ++x) {
          new (p_data + x*p_height + y) T{default_value};
        }
      }
    }
    Grid(Grid const& o):p_width(o.p_width),p_height(o.p_height) { //copy constructor should be noexcept
      if(o.p_data) {
        p_data = (T*)aligned_alloc(alignof(T), sizeof(T) * p_width * p_height);
        for(std::size_t i = 0; i < p_width*p_height; ++i) {
          new (p_data + i) T{o.p_data[i]};
        }
      }
    }
    Grid(Grid&& o):p_width(o.p_width),p_height(o.p_height),p_data(o.p_data) {
      o.p_data = nullptr;
    }
    Grid& operator=(Grid const& o) {
      Grid tmp = o;
      this->~Grid();
      new (this) Grid{std::move(tmp)};
      return *this;
    }
    Grid& operator=(Grid&& o) {
      Grid tmp = std::move(o);
      this->~Grid();
      new (this) Grid{std::move(tmp)};
      return *this;
    }
    ~Grid() {
      if(p_data) {
        for(std::size_t i = 0; i < p_width*p_height; ++i) {
          p_data[i].~T();
        }
        free(p_data);
      }
    }
    template<class Callback>
    static Grid fill_from_function(std::size_t p_width, std::size_t p_height, Callback&& callback) { //callback should be noexcept
      auto p_data = (T*)aligned_alloc(alignof(T), sizeof(T) * p_width * p_height);
      for(std::size_t y = 0; y < p_height; ++y) {
        for(std::size_t x = 0; x < p_width; ++x) {
          new (p_data + x*p_height + y) T{callback(x, y)};
        }
      }
      return Grid{p_data, p_width, p_height};
    }
    T& operator()(std::size_t x, std::size_t y) { //use () instead of [] because there's no multivariate [] and it's a pain to write a helper to do [][]
      return p_data[x*p_height + y];
    }
    T const& operator()(std::size_t x, std::size_t y) const {
      return p_data[x*p_height + y];
    }
    T& at(std::size_t x, std::size_t y) {
      if(x >= p_width || y >= p_height) throw std::runtime_error("Index out of bounds.");
      return p_data[x*p_height + y];
    }
    T const& at(std::size_t x, std::size_t y) const {
      if(x >= p_width || y >= p_height) throw std::runtime_error("Index out of bounds.");
      return p_data[x*p_height + y];
    }
    T* data() { return p_data; }
    T const* data() const { return p_data; }
    std::size_t width() const { return p_width; }
    std::size_t height() const { return p_height; }
  };
  template<class Callback, class RetType = std::invoke_result_t<Callback, std::size_t, std::size_t> >
  Grid<RetType> grid_from_function(std::size_t width, std::size_t height, Callback&& callback) {
    return Grid<RetType>::fill_from_function(width, height, std::forward<Callback>(callback));
  }
}

#endif
