#include "../Source/Plugin/plugin.hpp"

struct VecStore {
  new_expression::OwnedExpression entry_type;
  new_expression::OwnedExpression* begin;
  new_expression::OwnedExpression* end;
  friend bool operator==(VecStore const& lhs, VecStore const& rhs) {
    if(lhs.entry_type != rhs.entry_type) return false;
    if(lhs.end - lhs.begin != rhs.end - rhs.begin) return false;
    for(new_expression::OwnedExpression* lhs_it = lhs.begin, *rhs_it = rhs.begin; lhs_it != lhs.end; ++lhs_it, ++rhs_it) {
      if(*lhs_it != *rhs_it) return false;
    }
    return true;
  }
};
void destroy_from_arena(new_expression::Arena& arena, VecStore& vec) {
  arena.drop(std::move(vec.entry_type));
  for(auto& expr : std::span{vec.begin, vec.end}) {
    arena.drop(std::move(expr));
  }
}
struct VecStoreHasher {
  std::hash<void const*> hasher;
  auto operator()(VecStore const& vec) const noexcept {
    auto ret = hasher(vec.entry_type.data());
    for(auto const& expr : std::span{vec.begin, vec.end}) {
      ret = 31 * ret + hasher(expr.data());
    }
    return ret;
  }
};
struct VecData : new_expression::DataType {
  VecData(new_expression::Arena& arena, std::uint64_t type_index, new_expression::OwnedExpression type):new_expression::DataType(arena, type_index), type_family(std::move(type)) {}
  std::unordered_map<VecStore, new_expression::WeakExpression, VecStoreHasher> memoized; //no ownership in VecStores stored here
public:
  new_expression::OwnedExpression type_family;
  static new_expression::SharedDataTypePointer<VecData> register_on(new_expression::Arena& arena, new_expression::OwnedExpression vec_type) {
    return arena.create_data_type([&](new_expression::Arena& arena, std::uint64_t type_index) {
      return new VecData{
        arena,
        type_index,
        std::move(vec_type)
      };
    });
  }
  VecStore const& read_buffer(new_expression::Buffer const& buffer) {
    return (VecStore const&)buffer;
  }
  VecStore& read_buffer(new_expression::Buffer& buffer) {
    return (VecStore&)buffer;
  }
  VecStore const& read_data(new_expression::Data const& data) {
    return read_buffer(data.buffer);
  }
  new_expression::OwnedExpression make_expression(VecStore vec) {
    if(memoized.contains(vec)) {
      auto ret = arena.copy(memoized.at(vec));
      destroy_from_arena(arena, vec);
      if(vec.begin) free(vec.begin);
      return ret;
    } else {
      auto ret = arena.data(type_index, [&](new_expression::Buffer* buffer){
        new (buffer) VecStore{std::move(vec)};
      });
      memoized.insert(std::make_pair(std::move(vec), (new_expression::WeakExpression)ret));
      return ret;
    }
  }
  void destroy(new_expression::WeakExpression, new_expression::Buffer& buffer) {
    memoized.erase(read_buffer(buffer));
    destroy_from_arena(arena, read_buffer(buffer));
  };
  void debug_print(new_expression::Buffer const& buffer, std::ostream& o) {
    o << "<vector>";
  };
  void pretty_print(new_expression::Buffer const& buffer, std::ostream& o, mdb::function<void(new_expression::WeakExpression)>) {
    o << "<vector>";
  };
  bool all_subexpressions(new_expression::Buffer const&, mdb::function<bool(new_expression::WeakExpression)>) {
    std::terminate();
  };
  new_expression::OwnedExpression modify_subexpressions(new_expression::Buffer const&, new_expression::WeakExpression me, mdb::function<new_expression::OwnedExpression(new_expression::WeakExpression)>) {
    std::terminate();
  };
  new_expression::OwnedExpression type_of(new_expression::Buffer const& buf) {
    auto const& store = read_buffer(buf);
    return arena.apply(
      arena.copy(type_family),
      arena.copy(store.entry_type)
    );
  }
  ~VecData() {
    arena.drop(std::move(type_family));
  }
};
