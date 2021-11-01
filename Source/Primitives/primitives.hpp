#ifndef ARENA_PRIMITIVES_HPP
#define ARENA_PRIMITIVES_HPP

#include "../NewExpression/arena.hpp"
#include "string_holder.hpp"
#include <unordered_map>

namespace primitive {
  struct U64Data : new_expression::DataType {
    U64Data(new_expression::Arena& arena, std::uint64_t type_index):new_expression::DataType(arena, type_index) {}
    std::unordered_map<std::uint64_t, new_expression::WeakExpression> memoized;
  public:
    static new_expression::SharedDataTypePointer<U64Data> register_on(new_expression::Arena& arena) {
      return arena.create_data_type([](new_expression::Arena& arena, std::uint64_t type_index) {
        return new U64Data{
          arena,
          type_index
        };
      });
    }
    std::uint64_t read_buffer(new_expression::Buffer const& buffer) {
      return (std::uint64_t const&)buffer;
    }
    std::uint64_t read_data(new_expression::Data const& data) {
      return read_buffer(data.buffer);
    }
    new_expression::OwnedExpression make_expression(std::uint64_t value) {
      if(memoized.contains(value)) {
        return arena.copy(memoized.at(value));
      } else {
        auto ret = arena.data(type_index, [&](new_expression::Buffer* buffer){
          new (buffer) std::uint64_t{value};
        });
        memoized.insert(std::make_pair(value, (new_expression::WeakExpression)ret));
        return ret;
      }
    }
    void destroy(new_expression::WeakExpression, new_expression::Buffer& buffer) {
      memoized.erase(read_buffer(buffer));
    };
    void debug_print(new_expression::Buffer const& buffer, std::ostream& o) {
      o << read_buffer(buffer);
    };
    void pretty_print(new_expression::Buffer const& buffer, std::ostream& o, mdb::function<void(new_expression::WeakExpression)>) {
      o << read_buffer(buffer);
    };
    bool all_subexpressions(new_expression::Buffer const&, mdb::function<bool(new_expression::WeakExpression)>) { return true; };
    new_expression::OwnedExpression modify_subexpressions(new_expression::Buffer const&, new_expression::WeakExpression me, mdb::function<new_expression::OwnedExpression(new_expression::WeakExpression)>) { return arena.copy(me); };
  };
  struct StringData : new_expression::DataType {
    StringData(new_expression::Arena& arena, std::uint64_t type_index):new_expression::DataType(arena, type_index) {}
    std::unordered_map<StringHolder, new_expression::WeakExpression, StringHolderHasher> memoized;
  public:
    static new_expression::SharedDataTypePointer<StringData> register_on(new_expression::Arena& arena) {
      return arena.create_data_type([](new_expression::Arena& arena, std::uint64_t type_index) {
        return new StringData{
          arena,
          type_index
        };
      });
    }
    StringHolder const& read_buffer(new_expression::Buffer const& buffer) {
      return (StringHolder const&)buffer;
    }
    StringHolder const& read_data(new_expression::Data const& data) {
      return read_buffer(data.buffer);
    }
    new_expression::OwnedExpression make_expression(StringHolder string) {
      if(memoized.contains(string)) {
        return arena.copy(memoized.at(string));
      } else {
        auto ret = arena.data(type_index, [&](new_expression::Buffer* buffer){
          new (buffer) StringHolder{string};
        });
        memoized.insert(std::make_pair(std::move(string), (new_expression::WeakExpression)ret));
        return ret;
      }
    }
    void destroy(new_expression::WeakExpression, new_expression::Buffer& buffer) {
      memoized.erase(read_buffer(buffer));
    };
    void debug_print(new_expression::Buffer const& buffer, std::ostream& o) {
      o << read_buffer(buffer);
    };
    void pretty_print(new_expression::Buffer const& buffer, std::ostream& o, mdb::function<void(new_expression::WeakExpression)>) {
      o << read_buffer(buffer);
    };
    bool all_subexpressions(new_expression::Buffer const&, mdb::function<bool(new_expression::WeakExpression)>) { return true; };
    new_expression::OwnedExpression modify_subexpressions(new_expression::Buffer const&, new_expression::WeakExpression me, mdb::function<new_expression::OwnedExpression(new_expression::WeakExpression)>) { return arena.copy(me); };
  };
}

#endif
