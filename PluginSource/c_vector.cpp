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
struct Witness{};

extern "C" void initialize(pipeline::compile::StandardCompilerContext* context, new_expression::TypedValue* import_start, new_expression::TypedValue* import_end) {
  auto const& [
    native_empty_vec,
    native_cons_vec,
    witness,
    c_vector,
    empty_vec,
    cons_vec,
    c_vector_to_vector,
    c_vec_len,
    c_vec_at
  ] = mdb::map_span<9>(import_start, import_end, [&](auto const& v) -> new_expression::WeakExpression { return v.value; });

  auto vec = VecData::register_on(context->arena, context->arena.copy(c_vector));

  auto rule_builder = plugin::get_rule_builder(
    context,
    context->u64,
    plugin::ignore,
    plugin::expression_handler,
    vec,
    plugin::ExactHandler<Witness>{
      context->arena,
      witness
    }
  );
  auto& arena = context->arena;

  rule_builder(empty_vec, [](new_expression::OwnedExpression type) {
    return VecStore{
      .entry_type = std::move(type),
      .begin = nullptr,
      .end = nullptr
    };
  });
  rule_builder(cons_vec, [&arena](new_expression::OwnedExpression type, new_expression::OwnedExpression head, VecStore const& tail) {
    auto old_extent = tail.end - tail.begin;
    auto new_allocation = (new_expression::OwnedExpression*)malloc(sizeof(new_expression::OwnedExpression) * (1 + old_extent));
    new (new_allocation) new_expression::OwnedExpression{std::move(head)};
    for(std::size_t i = 0; i < old_extent; ++i) {
      new (new_allocation + i) new_expression::OwnedExpression{arena.copy(tail.begin[i])};
    }
    return VecStore{
      .entry_type = std::move(type),
      .begin = new_allocation,
      .end = new_allocation + (1 + old_extent)
    };
  });
  rule_builder(c_vector_to_vector, [&arena, native_empty_vec, native_cons_vec](new_expression::WeakExpression type, VecStore const& vec) {
    auto ret = arena.apply(
      arena.copy(native_empty_vec),
      arena.copy(type)
    );
    auto it = vec.end;
    while(it != vec.begin) {
      --it;
      ret = arena.apply(
        arena.copy(native_cons_vec),
        arena.copy(type),
        arena.copy(*it),
        std::move(ret)
      );
    }
    return ret;
  });
  rule_builder(c_vec_len, [](plugin::Ignore, VecStore const& vec) {
    return (std::uint64_t)(vec.end - vec.begin);
  });
  rule_builder(c_vec_at, [](plugin::Ignore, VecStore const& vec, std::uint64_t index, Witness) -> new_expression::WeakExpression {
    return vec.begin[index];
  });
}
