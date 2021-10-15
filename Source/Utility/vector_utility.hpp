#ifndef MDB_VECTOR_UTILITY_HPP
#define MDB_VECTOR_UTILITY_HPP

#include <algorithm>
#include <vector>
#include <span>

namespace mdb {
  template<class T>
  std::span<T> as_span(std::vector<T>& vec) {
    return std::span<T>{vec.begin(), vec.begin() + vec.size()};
  }
  template<class T>
  std::span<T const> as_span(std::vector<T> const& vec) {
    return std::span<T const>{vec.begin(), vec.begin() + vec.size()};
  }
  template<class T, class... Ts> //to replace initializer_list init of move-only types
  std::vector<T> make_vector(T value, Ts... values) {
    std::vector<T> ret;
    ret.reserve(sizeof...(Ts) + 1);
    ret.push_back(std::move(value));
    (ret.push_back(std::move(values)) , ...);
    return ret;
  }
  template<class F, class T, class R = std::invoke_result_t<F, T const&> >
  std::vector<R> map(F&& function, std::vector<T> const& vec) {
    std::vector<R> ret;
    ret.reserve(vec.size());
    for(auto& entry : vec) {
      ret.push_back(function(entry));
    }
    return ret;
  }
  template<class F, class T, class R = std::invoke_result_t<F, T&&> >
  std::vector<R> map(F&& function, std::vector<T>&& vec) {
    std::vector<R> ret;
    ret.reserve(vec.size());
    for(auto& entry : vec) {
      ret.push_back(function(std::move(entry)));
    }
    return ret;
  }
  template<class T, class Predicate>
  void erase_if(std::vector<T>& vec, Predicate&& predicate) {
    auto erase_start = std::remove_if(vec.begin(), vec.end(), std::forward<Predicate>(predicate));
    vec.erase(erase_start, vec.end());
  }
  template<class T, class Run>
  bool erase_from_active_queue(std::vector<T>& vec, Run&& try_complete) {
    //given a vector, call try_complete on each element once and erase any
    //elements for which it returns true. Stable even if invocations
    //of try_complete call vec.push_back. Preserves order.
    //Returns true if something was removed.
    std::size_t checked_index = 0; //indices less than run_index have already been run.
    for(;;++checked_index) { //This loop handles the vector until something is completed.
      if(vec.size() == checked_index) return false;
      if(try_complete(vec[checked_index])) {
        break;
      }
    }
    for(std::size_t next_index = checked_index + 1; next_index < vec.size(); ++next_index) {
      //Indices less than run_index have been run (no completion)
      //Indices greater than or equal to next_index have not.
      //Indices between have values we don't care about (completed or moved from)
      if(!try_complete(vec[next_index])) {
        //If something isn't completed, move it to the next index.
        vec[checked_index] = std::move(vec[next_index]);
        ++checked_index;
      }
    }
    //erase everything except those under checked_index
    vec.erase(vec.begin() + checked_index, vec.end());
    return true;
  }
}

#endif
