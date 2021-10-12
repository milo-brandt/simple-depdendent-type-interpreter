#include "arena.hpp"
#include "../Utility/handled_vector.hpp"
#include <vector>
#include <cstring>
#include <unordered_set>
#include <unordered_map>
#include <iostream>

namespace new_expression {

  UniqueExpression::UniqueExpression(UniqueExpression&& other):p_arena(other.p_arena), p_expression(std::move(other.p_expression)) {
    other.p_arena = nullptr;
  }
  UniqueExpression& UniqueExpression::operator=(UniqueExpression&& other) {
    std::swap(other.p_arena, p_arena);
    std::swap(other.p_expression, p_expression);
    return *this;
  }
  UniqueExpression::~UniqueExpression() {
    if(p_arena) {
      p_arena->drop(std::move(p_expression));
    }
  }

  namespace {
    struct Free { //element of free list
      std::size_t next_free_entry;
    };
    union ArenaData {
      Apply apply;
      Axiom axiom;
      Declaration declaration;
      Data data;
      Argument argument;
      Conglomerate conglomerate;
      Free free_list_entry;
    };
    static constexpr std::uint64_t discriminator_apply = 0;
    static constexpr std::uint64_t discriminator_axiom = 1;
    static constexpr std::uint64_t discriminator_declaration = 2;
    static constexpr std::uint64_t discriminator_data = 3;
    static constexpr std::uint64_t discriminator_argument = 4;
    static constexpr std::uint64_t discriminator_conglomerate = 5;
    static constexpr std::uint64_t discriminator_free = 6;

    struct ArenaEntry {
      std::uint64_t discriminator;
      std::uint64_t reference_count;
      ArenaData data;
    };
    struct PairHasher {
      std::hash<std::size_t> h;
      auto operator()(std::pair<WeakExpression, WeakExpression> const& apply) const noexcept {
        return h(apply.first.index()) + h(apply.second.index());
      }
    };
  }
  struct Arena::Impl {
    struct Handler {
      Impl* impl;
      void move_destroy(std::span<ArenaEntry> new_span, std::span<ArenaEntry> old_span);
      void destroy(std::span<ArenaEntry> span);
    };
    std::vector<std::unique_ptr<DataType> > data_types;
    std::unordered_set<std::size_t> orphan_entries;
    mdb::Event<std::span<WeakExpression> > terms_erased;
    std::unordered_map<std::pair<WeakExpression, WeakExpression>, WeakExpression, PairHasher> existing_apply;
    std::unordered_map<std::uint64_t, WeakExpression> existing_argument;
    std::unordered_map<std::uint64_t, WeakExpression> existing_conglomerate;
    mdb::HandledVector<ArenaEntry, Handler> entries;

    Impl():entries(Handler{this}) {}

    std::size_t free_list_head = (std::size_t)-1;
    std::size_t next_writable_entry(); //returns entry with ref_count of 1, safe to overwrite union
    void ref_entry(std::size_t index);
    void deref_entry(std::size_t index);
    void destroy_entry(std::size_t index);
    bool clear_orphans(); //returns true if space was cleared.

    std::uint64_t next_type_index() {
      return data_types.size();
    }
    void add_type(std::unique_ptr<DataType>&& type) {
      data_types.push_back(std::move(type));
    }
    OwnedExpression copy(OwnedExpression const& input) {
      ref_entry(input.index());
      return OwnedExpression{
        input.index()
      };
    }
    OwnedExpression copy(WeakExpression const& input) {
      ref_entry(input.index());
      return OwnedExpression{
        input.index()
      };
    }
    void drop(OwnedExpression&& input) {
      deref_entry(input.index());
    }
    OwnedExpression apply(OwnedExpression lhs, OwnedExpression rhs) {
      auto key = std::make_pair(WeakExpression{lhs}, WeakExpression{rhs});
      if(existing_apply.contains(key)) {
        drop(std::move(lhs));
        drop(std::move(rhs));
        return copy(existing_apply.at(key));
      } else {
        auto index = next_writable_entry();
        entries[index].discriminator = discriminator_apply;
        entries[index].data.apply = Apply{
          .lhs = std::move(lhs),
          .rhs = std::move(rhs)
        };
        existing_apply.insert(std::make_pair(key, WeakExpression{index}));
        return OwnedExpression{index};
      }
    }
    OwnedExpression axiom() {
      auto index = next_writable_entry();
      entries[index].discriminator = discriminator_axiom;
      entries[index].data.axiom = Axiom{};
      return OwnedExpression{index};
    }
    OwnedExpression declaration() {
      auto index = next_writable_entry();
      entries[index].discriminator = discriminator_declaration;
      entries[index].data.axiom = Axiom{};
      return OwnedExpression{index};
    }
    OwnedExpression argument(std::uint64_t arg_index) {
      if(existing_argument.contains(arg_index)) {
        return copy(existing_argument.at(arg_index));
      } else {
        auto index = next_writable_entry();
        entries[index].discriminator = discriminator_argument;
        entries[index].data.argument = Argument{arg_index};
        existing_argument.insert(std::make_pair(arg_index, WeakExpression{index}));
        return OwnedExpression{index};
      }
      auto index = next_writable_entry();
      entries[index].discriminator = discriminator_argument;
      entries[index].data.axiom = Axiom{};
      return OwnedExpression{index};
    }
    OwnedExpression conglomerate(std::uint64_t conglomerate_index) {
      if(existing_conglomerate.contains(conglomerate_index)) {
        return copy(existing_conglomerate.at(conglomerate_index));
      } else {
        auto index = next_writable_entry();
        entries[index].discriminator = discriminator_conglomerate;
        entries[index].data.conglomerate = Conglomerate{conglomerate_index};
        existing_conglomerate.insert(std::make_pair(conglomerate_index, WeakExpression{index}));
        return OwnedExpression{index};
      }
      auto index = next_writable_entry();
      entries[index].discriminator = discriminator_conglomerate;
      entries[index].data.axiom = Axiom{};
      return OwnedExpression{index};
    }
    std::pair<OwnedExpression, Buffer*> allocate_data(std::uint64_t type_index) {
      auto index = next_writable_entry();
      entries[index].discriminator = discriminator_data;
      entries[index].data.data = Data{
        .type_index = type_index
      };
      return std::make_pair(OwnedExpression{index}, &entries[index].data.data.buffer);
    }
    std::pair<std::size_t, void const*> visit_data(WeakExpression expr) const {
      auto& entry = entries[expr.index()];
      return {entry.discriminator, &entry.data};
    }
    void clear_orphaned_expressions() { //clears every expression it can.
      std::vector<WeakExpression> cleared_list;
      while(!orphan_entries.empty()) {
        auto target = *orphan_entries.begin();
        cleared_list.push_back(WeakExpression{target});
        destroy_entry(target);
        orphan_entries.erase(target);
      }
      terms_erased(std::span{cleared_list.data(), cleared_list.data() + cleared_list.size()});
    }
    bool empty() const {
      for(auto const& entry : entries) {
        if(entry.discriminator != discriminator_free)
          return false;
      }
      return true;
    }
  };
  std::size_t Arena::Impl::next_writable_entry() {
    auto ret = [&] {
      if(free_list_head == -1) {
        if(!clear_orphans()) {
          auto index = entries.push_back(ArenaEntry{
            .discriminator = discriminator_free,
            .reference_count = 1
          });
          return index;
        }
      }
      auto index = free_list_head;
      free_list_head = entries[free_list_head].data.free_list_entry.next_free_entry;
      ++entries[index].reference_count;
      return index;
    }();
    if(entries[ret].reference_count != 1) std::terminate();
    if(entries[ret].discriminator != discriminator_free) std::terminate();
    return ret;
  }
  void Arena::Impl::ref_entry(std::size_t index) {
    auto& entry = entries[index];
    if(entry.discriminator >= discriminator_free) std::terminate();
    if(++entry.reference_count == 1) {
      orphan_entries.erase(index);
    }
  }
  void Arena::Impl::deref_entry(std::size_t index) {
    auto& entry = entries[index];
    if(entry.reference_count == 0) std::terminate(); //!!! can't deref empty thing (debugging purposes only)
    if(--entry.reference_count == 0) {
      orphan_entries.insert(index);
    }
  }
  void Arena::Impl::destroy_entry(std::size_t index) {
   auto& entry = entries[index];
   if(entry.reference_count > 0) std::terminate();
   switch(entry.discriminator) {
     case discriminator_apply:
       deref_entry(entry.data.apply.lhs.index());
       deref_entry(entry.data.apply.rhs.index());
       existing_apply.erase(std::make_pair(
         WeakExpression{entry.data.apply.lhs},
         WeakExpression{entry.data.apply.rhs}
       ));
       break;
     case discriminator_argument:
       existing_argument.erase(entry.data.argument.index);
       break;
     case discriminator_conglomerate:
       existing_conglomerate.erase(entry.data.argument.index);
       break;
     case discriminator_data:
       data_types[entry.data.data.type_index]->destroy(WeakExpression{index}, entry.data.data.buffer);
       break;
     default:
       if(entry.discriminator >= discriminator_free) std::terminate();
   }
   entry.discriminator = discriminator_free;
   entry.data.free_list_entry.next_free_entry = free_list_head;
   free_list_head = index;
 }
 bool Arena::Impl::clear_orphans() {
   //for now, just try to clear 256 orphans on each call
   if(orphan_entries.size() < 256) return false;
   union WeakExpressionArray {
     int dummy;
     WeakExpression data[256];
   };
   WeakExpressionArray array{ .dummy = 0 };
   std::size_t count = 0;
   for(auto entry : orphan_entries) {
     array.data[count++] = WeakExpression{entry};
     if(count == 256) break;
   }
   for(auto entry : array.data) {
     destroy_entry(entry.index());
     orphan_entries.erase(entry.index());
   }
   terms_erased(std::span{array.data});
   return true;
 }
 void Arena::Impl::Handler::move_destroy(std::span<ArenaEntry> new_span, std::span<ArenaEntry> old_span) {
    memcpy(new_span.data(), old_span.data(), old_span.size_bytes()); //move all the trivial data
    for(
      auto old_entry = old_span.begin(), new_entry = new_span.begin();
      old_entry != old_span.end();
      ++old_entry, ++new_entry
    ) { //move the non-trivial data
      if(old_entry->discriminator == discriminator_data) {
        impl->data_types[old_entry->data.data.type_index]->move_destroy(new_entry->data.data.buffer, old_entry->data.data.buffer);
      }
    }
  }
  void Arena::Impl::Handler::destroy(std::span<ArenaEntry> span) {
    std::size_t index = 0;
    for(auto& entry : span) {
      if(entry.discriminator == discriminator_data) {
        impl->data_types[entry.data.data.type_index]->destroy(WeakExpression{index}, entry.data.data.buffer);
      }
      ++index;
    }
  }


  std::uint64_t Arena::next_type_index() { return impl->next_type_index(); }
  void Arena::add_type(std::unique_ptr<DataType>&& data) { return impl->add_type(std::move(data)); }
  std::pair<OwnedExpression, Buffer*> Arena::allocate_data(std::uint64_t type_index) { return impl->allocate_data(type_index); }
  std::pair<std::size_t, void const*> Arena::visit_data(WeakExpression expr) const { return impl->visit_data(expr); }

  OwnedExpression Arena::apply(OwnedExpression lhs, OwnedExpression rhs) { return impl->apply(std::move(lhs), std::move(rhs)); }
  OwnedExpression Arena::axiom() { return impl->axiom(); }
  OwnedExpression Arena::declaration() { return impl->declaration(); }
  OwnedExpression Arena::argument(std::uint64_t index) { return impl->argument(index); }
  OwnedExpression Arena::conglomerate(std::uint64_t index) { return impl->conglomerate(index); }

  Arena::Arena():impl(std::make_unique<Impl>()) {}
  Arena::Arena(Arena&&) = default;
  Arena& Arena::operator=(Arena&&) = default;
  Arena::~Arena() = default;
  OwnedExpression Arena::copy(OwnedExpression const& expr) { return impl->copy(expr); }
  OwnedExpression Arena::copy(WeakExpression const& expr) { return impl->copy(expr); }
  mdb::Event<std::span<WeakExpression> >& Arena::terms_erased() { return impl->terms_erased; }
  //Boilerplate code for the getters.
  Apply const* Arena::get_if_apply(WeakExpression expr) const {
    auto& entry = impl->entries[expr.index()];
    if(entry.discriminator == discriminator_apply) {
      return &entry.data.apply;
    } else {
      return nullptr;
    }
  }
  Apply const& Arena::get_apply(WeakExpression expr) const {
    auto& entry = impl->entries[expr.index()];
    if(entry.discriminator == discriminator_apply) {
      return entry.data.apply;
    } else {
      std::terminate();
    }
  }
  bool Arena::holds_apply(WeakExpression expr) const {
    return impl->entries[expr.index()].discriminator == discriminator_apply;
  }
  Axiom const* Arena::get_if_axiom(WeakExpression expr) const {
    auto& entry = impl->entries[expr.index()];
    if(entry.discriminator == discriminator_axiom) {
      return &entry.data.axiom;
    } else {
      return nullptr;
    }
  }
  Axiom const& Arena::get_axiom(WeakExpression expr) const {
    auto& entry = impl->entries[expr.index()];
    if(entry.discriminator == discriminator_axiom) {
      return entry.data.axiom;
    } else {
      std::terminate();
    }
  }
  bool Arena::holds_axiom(WeakExpression expr) const {
    return impl->entries[expr.index()].discriminator == discriminator_axiom;
  }
  Declaration const* Arena::get_if_declaration(WeakExpression expr) const {
    auto& entry = impl->entries[expr.index()];
    if(entry.discriminator == discriminator_declaration) {
      return &entry.data.declaration;
    } else {
      return nullptr;
    }
  }
  Declaration const& Arena::get_declaration(WeakExpression expr) const {
    auto& entry = impl->entries[expr.index()];
    if(entry.discriminator == discriminator_declaration) {
      return entry.data.declaration;
    } else {
      std::terminate();
    }
  }
  bool Arena::holds_declaration(WeakExpression expr) const {
    return impl->entries[expr.index()].discriminator == discriminator_declaration;
  }
  Data const* Arena::get_if_data(WeakExpression expr) const {
    auto& entry = impl->entries[expr.index()];
    if(entry.discriminator == discriminator_data) {
      return &entry.data.data;
    } else {
      return nullptr;
    }
  }
  Data const& Arena::get_data(WeakExpression expr) const {
    auto& entry = impl->entries[expr.index()];
    if(entry.discriminator == discriminator_data) {
      return entry.data.data;
    } else {
      std::terminate();
    }
  }
  bool Arena::holds_data(WeakExpression expr) const {
    return impl->entries[expr.index()].discriminator == discriminator_data;
  }
  Argument const* Arena::get_if_argument(WeakExpression expr) const {
    auto& entry = impl->entries[expr.index()];
    if(entry.discriminator == discriminator_argument) {
      return &entry.data.argument;
    } else {
      return nullptr;
    }
  }
  Argument const& Arena::get_argument(WeakExpression expr) const {
    auto& entry = impl->entries[expr.index()];
    if(entry.discriminator == discriminator_argument) {
      return entry.data.argument;
    } else {
      std::terminate();
    }
  }
  bool Arena::holds_argument(WeakExpression expr) const {
    return impl->entries[expr.index()].discriminator == discriminator_argument;
  }
  Conglomerate const* Arena::get_if_conglomerate(WeakExpression expr) const {
    auto& entry = impl->entries[expr.index()];
    if(entry.discriminator == discriminator_conglomerate) {
      return &entry.data.conglomerate;
    } else {
      return nullptr;
    }
  }
  Conglomerate const& Arena::get_conglomerate(WeakExpression expr) const {
    auto& entry = impl->entries[expr.index()];
    if(entry.discriminator == discriminator_conglomerate) {
      return entry.data.conglomerate;
    } else {
      std::terminate();
    }
  }
  bool Arena::holds_conglomerate(WeakExpression expr) const {
    return impl->entries[expr.index()].discriminator == discriminator_conglomerate;
  }
  void Arena::clear_orphaned_expressions() { return impl->clear_orphaned_expressions(); }
  bool Arena::empty() const { return impl->empty(); }
  void Arena::drop(OwnedExpression&& expr) { return impl->drop(std::move(expr)); }
  void Arena::debug_dump() const {
    std::size_t index = 0;
    for(auto& entry : impl->entries) {
      std::cout << index++ << " [RefCt " << entry.reference_count << "]: ";
      switch(entry.discriminator) {
        case 0: std::cout << "Apply " << entry.data.apply.lhs.index() << " , " << entry.data.apply.rhs.index() << "\n"; break;
        case 1: std::cout << "Axiom\n"; break;
        case 2: std::cout << "Declaration\n"; break;
        case 3: std::cout << "Data\n"; break;
        case 4: std::cout << "Argument " << entry.data.argument.index << "\n"; break;
        case 5: std::cout << "Conglomerate " << entry.data.conglomerate.index << "\n"; break;
        case 6: std::cout << "Free List " << entry.data.free_list_entry.next_free_entry << "\n"; break;
        default: std::terminate();
      }
    }
  }
}
