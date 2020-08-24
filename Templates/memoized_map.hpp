#ifndef memoized_map_h
#define memoized_map_h

#include <unordered_map>

template<class in, class out, class hasher, class callback>
class memoized_map{
  std::unordered_map<in, out, hasher> known;
  callback call;
public:
  memoized_map(callback call):call(std::move(call)){}
  out const& operator()(in const& data){
    if(known.contains(data)) return known.at(data);
    else return known[data] = call(*this, data);
  }
  void add_value(in const& data, out const& value){
    known[data] = value;
  }
};
template<class in, class out, class hasher = std::hash<in>, class callback>
auto make_memoized_map(callback call){
  return memoized_map<in, out, hasher, callback>{std::forward<callback>(call)};
}

#endif
