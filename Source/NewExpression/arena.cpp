#include "arena.hpp"
#include "../Utility/handled_vector.hpp"
#include <vector>
#include <cstring>
#include <unordered_set>
#include <unordered_map>
#include <iostream>

namespace new_expression {
  namespace {
    struct ArenaEntry;
    struct Free { //element of free list
      ArenaEntry* next_free_entry;
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
      std::hash<void const*> h;
      auto operator()(std::pair<WeakExpression, WeakExpression> const& apply) const noexcept {
        return h(apply.first.data()) + h(apply.second.data());
      }
    };
    struct ArenaPool {
      std::vector<std::byte*> allocations;
      std::size_t total_size = 0;
      ArenaEntry* next_free_entry = nullptr;
      ArenaEntry* uninit_begin = nullptr;
      ArenaEntry* uninit_end = nullptr;
      void allocate_new_block() {
        std::size_t next_allocation_size = 256 + total_size * 3 / 2;
        total_size += next_allocation_size;
        auto new_alloc = (std::byte*)malloc(sizeof(std::size_t) + sizeof(ArenaEntry) * next_allocation_size);
        if(!new_alloc) std::terminate(); //oh no
        new (new_alloc) std::size_t{next_allocation_size};
        uninit_begin = (ArenaEntry*)(new_alloc + sizeof(std::size_t));
        uninit_end = uninit_begin + next_allocation_size;
        allocations.push_back(new_alloc);
      }
      ArenaEntry* get_next_writable() {
        if(next_free_entry) { //write into a previous occupied, but now free slot
          auto ret = next_free_entry;
          next_free_entry = next_free_entry->data.free_list_entry.next_free_entry;
          if(++ret->reference_count != 1) std::terminate();
          return ret;
        } else { //write into an allocated but never initialized slot
          if(uninit_begin == uninit_end) {
            allocate_new_block();
          }
          auto ret = uninit_begin;
          ++uninit_begin;
          ret->reference_count = 1;
          return ret;
        }
      }
      bool next_write_allocates() const {
        return next_free_entry == nullptr;
      }
      bool empty() const {
        for(std::byte* allocation : allocations) {
          auto size = *(std::size_t*)allocation;
          auto begin = (ArenaEntry*)(allocation + sizeof(std::size_t));
          for(std::size_t i = 0; i < size; ++i, ++begin) {
            if(begin == uninit_begin) break; //avoid unitialized region
            if(begin->discriminator != discriminator_free) return false;
          }
        }
        return true;
      }
      template<class Callback>
      void for_each(Callback&& callback) {
        for(std::byte* allocation : allocations) {
          auto size = *(std::size_t*)allocation;
          auto begin = (ArenaEntry*)(allocation + sizeof(std::size_t));
          for(std::size_t i = 0; i < size; ++i, ++begin) {
            if(begin == uninit_begin) break; //avoid unitialized region
            if(begin->discriminator != discriminator_free) {
              callback(*begin);
            }
          }
        }
      }
      void erase(ArenaEntry* ptr) {
        if(ptr->reference_count != 0) std::terminate();
        ptr->discriminator = discriminator_free;
        ptr->data.free_list_entry.next_free_entry = next_free_entry;
        next_free_entry = ptr;
      }
      ~ArenaPool() {
        for(auto ptr : allocations) {
          free(ptr);
        }
      }
    };
  }
  struct Arena::Impl {
    std::vector<std::unique_ptr<DataType> > data_types;
    std::unordered_set<ArenaEntry*> orphan_entries;
    mdb::Event<std::span<WeakExpression> > terms_erased;
    std::unordered_map<std::pair<WeakExpression, WeakExpression>, WeakExpression, PairHasher> existing_apply;
    std::unordered_map<std::uint64_t, WeakExpression> existing_argument;
    std::unordered_map<std::uint64_t, WeakExpression> existing_conglomerate;
    ArenaPool pool;

    std::size_t free_list_head = (std::size_t)-1;
    void ref_entry(ArenaEntry*);
    void deref_entry(ArenaEntry*);
    void destroy_entry(ArenaEntry*);
    bool clear_orphans(); //returns true if space was cleared.
    ArenaEntry* get_next_writable() {
      if(pool.next_write_allocates() && orphan_entries.size() > 256) {
        clear_orphans();
      }
      return pool.get_next_writable();
    }

    std::uint64_t next_type_index() {
      return data_types.size();
    }
    void add_type(std::unique_ptr<DataType>&& type) {
      data_types.push_back(std::move(type));
    }
    OwnedExpression copy(OwnedExpression const& input) {
      ref_entry((ArenaEntry*)input.data());
      return OwnedExpression{
        input.data()
      };
    }
    OwnedExpression copy(WeakExpression const& input) {
      ref_entry((ArenaEntry*)input.data());
      return OwnedExpression{
        input.data()
      };
    }
    void drop(OwnedExpression&& input) {
      deref_entry((ArenaEntry*)input.data());
    }
    OwnedExpression apply(OwnedExpression lhs, OwnedExpression rhs) {
      auto key = std::make_pair(WeakExpression{lhs}, WeakExpression{rhs});
      if(existing_apply.contains(key)) {
        drop(std::move(lhs));
        drop(std::move(rhs));
        return copy(existing_apply.at(key));
      } else {
        auto ptr = get_next_writable();
        ptr->discriminator = discriminator_apply;
        ptr->data.apply = Apply{
          .lhs = lhs,
          .rhs = rhs
        };
        //hold on to references to lhs and rhs
        existing_apply.insert(std::make_pair(key, WeakExpression{ptr}));
        return OwnedExpression{ptr};
      }
    }
    OwnedExpression axiom() {
      auto ptr = get_next_writable();
      ptr->discriminator = discriminator_axiom;
      ptr->data.axiom = Axiom{};
      return OwnedExpression{ptr};
    }
    OwnedExpression declaration() {
      auto ptr = get_next_writable();
      ptr->discriminator = discriminator_declaration;
      ptr->data.declaration = Declaration{};
      return OwnedExpression{ptr};
    }
    OwnedExpression argument(std::uint64_t arg_index) {
      if(existing_argument.contains(arg_index)) {
        return copy(existing_argument.at(arg_index));
      } else {
        auto ptr = get_next_writable();
        ptr->discriminator = discriminator_argument;
        ptr->data.argument = Argument{arg_index};
        existing_argument.insert(std::make_pair(arg_index, WeakExpression{ptr}));
        return OwnedExpression{ptr};
      }
    }
    OwnedExpression conglomerate(std::uint64_t conglomerate_index) {
      if(existing_conglomerate.contains(conglomerate_index)) {
        return copy(existing_conglomerate.at(conglomerate_index));
      } else {
        auto ptr = get_next_writable();
        ptr->discriminator = discriminator_conglomerate;
        ptr->data.conglomerate = Conglomerate{conglomerate_index};
        existing_conglomerate.insert(std::make_pair(conglomerate_index, WeakExpression{ptr}));
        return OwnedExpression{ptr};
      }
    }
    std::pair<OwnedExpression, Buffer*> allocate_data(std::uint64_t type_index) {
      data_types[type_index]->ref();
      auto ptr = get_next_writable();
      ptr->discriminator = discriminator_data;
      ptr->data.data = Data{
        .type_index = type_index
      };
      return std::make_pair(OwnedExpression{ptr}, &ptr->data.data.buffer);
    }
    std::pair<std::size_t, void const*> visit_data(WeakExpression expr) const {
      auto ptr = (ArenaEntry*)expr.data();
      return {ptr->discriminator, &ptr->data};
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
      for(auto const& ptr : data_types) {
        if(ptr != nullptr) {
          return false;
        }
      }
      return pool.empty();
    }
  };
  void Arena::Impl::ref_entry(ArenaEntry* ptr) {
    if(ptr->discriminator >= discriminator_free) std::terminate();
    if(++ptr->reference_count == 1) {
      orphan_entries.erase(ptr);
    }
  }
  void Arena::Impl::deref_entry(ArenaEntry* ptr) {
    if(ptr->reference_count == 0) std::terminate(); //!!! can't deref empty thing (debugging purposes only)
    if(--ptr->reference_count == 0) {
      orphan_entries.insert(ptr);
    }
  }
  void Arena::Impl::destroy_entry(ArenaEntry* ptr) {
   if(ptr->reference_count > 0) std::terminate();
   switch(ptr->discriminator) {
     case discriminator_apply:
       deref_entry((ArenaEntry*)ptr->data.apply.lhs.data());
       deref_entry((ArenaEntry*)ptr->data.apply.rhs.data());
       existing_apply.erase(std::make_pair(
         WeakExpression{ptr->data.apply.lhs},
         WeakExpression{ptr->data.apply.rhs}
       ));
       break;
     case discriminator_argument:
       existing_argument.erase(ptr->data.argument.index);
       break;
     case discriminator_conglomerate:
       existing_conglomerate.erase(ptr->data.argument.index);
       break;
     case discriminator_data:
       data_types[ptr->data.data.type_index]->destroy(WeakExpression{ptr}, ptr->data.data.buffer);
       data_types[ptr->data.data.type_index]->deref();
       break;
     default:
       if(ptr->discriminator >= discriminator_free) std::terminate();
   }
   pool.erase(ptr);
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
     destroy_entry((ArenaEntry*)entry.data());
     orphan_entries.erase((ArenaEntry*)entry.data());
   }
   terms_erased(std::span{array.data});
   return true;
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
    auto& entry = *(ArenaEntry*)expr.data();
    if(entry.discriminator == discriminator_apply) {
      return &entry.data.apply;
    } else {
      return nullptr;
    }
  }
  Apply const& Arena::get_apply(WeakExpression expr) const {
    auto& entry = *(ArenaEntry*)expr.data();
    if(entry.discriminator == discriminator_apply) {
      return entry.data.apply;
    } else {
      std::terminate();
    }
  }
  bool Arena::holds_apply(WeakExpression expr) const {
    auto& entry = *(ArenaEntry*)expr.data();
    return entry.discriminator == discriminator_apply;
  }
  Axiom const* Arena::get_if_axiom(WeakExpression expr) const {
    auto& entry = *(ArenaEntry*)expr.data();
    if(entry.discriminator == discriminator_axiom) {
      return &entry.data.axiom;
    } else {
      return nullptr;
    }
  }
  Axiom const& Arena::get_axiom(WeakExpression expr) const {
    auto& entry = *(ArenaEntry*)expr.data();
    if(entry.discriminator == discriminator_axiom) {
      return entry.data.axiom;
    } else {
      std::terminate();
    }
  }
  bool Arena::holds_axiom(WeakExpression expr) const {
    auto& entry = *(ArenaEntry*)expr.data();
    return entry.discriminator == discriminator_axiom;
  }
  Declaration const* Arena::get_if_declaration(WeakExpression expr) const {
    auto& entry = *(ArenaEntry*)expr.data();
    if(entry.discriminator == discriminator_declaration) {
      return &entry.data.declaration;
    } else {
      return nullptr;
    }
  }
  Declaration const& Arena::get_declaration(WeakExpression expr) const {
    auto& entry = *(ArenaEntry*)expr.data();
    if(entry.discriminator == discriminator_declaration) {
      return entry.data.declaration;
    } else {
      std::terminate();
    }
  }
  bool Arena::holds_declaration(WeakExpression expr) const {
    auto& entry = *(ArenaEntry*)expr.data();
    return entry.discriminator == discriminator_declaration;
  }
  Data const* Arena::get_if_data(WeakExpression expr) const {
    auto& entry = *(ArenaEntry*)expr.data();
    if(entry.discriminator == discriminator_data) {
      return &entry.data.data;
    } else {
      return nullptr;
    }
  }
  Data const& Arena::get_data(WeakExpression expr) const {
    auto& entry = *(ArenaEntry*)expr.data();
    if(entry.discriminator == discriminator_data) {
      return entry.data.data;
    } else {
      std::terminate();
    }
  }
  bool Arena::holds_data(WeakExpression expr) const {
    auto& entry = *(ArenaEntry*)expr.data();
    return entry.discriminator == discriminator_data;
  }
  Argument const* Arena::get_if_argument(WeakExpression expr) const {
    auto& entry = *(ArenaEntry*)expr.data();
    if(entry.discriminator == discriminator_argument) {
      return &entry.data.argument;
    } else {
      return nullptr;
    }
  }
  Argument const& Arena::get_argument(WeakExpression expr) const {
    auto& entry = *(ArenaEntry*)expr.data();
    if(entry.discriminator == discriminator_argument) {
      return entry.data.argument;
    } else {
      std::terminate();
    }
  }
  bool Arena::holds_argument(WeakExpression expr) const {
    auto& entry = *(ArenaEntry*)expr.data();
    return entry.discriminator == discriminator_argument;
  }
  Conglomerate const* Arena::get_if_conglomerate(WeakExpression expr) const {
    auto& entry = *(ArenaEntry*)expr.data();
    if(entry.discriminator == discriminator_conglomerate) {
      return &entry.data.conglomerate;
    } else {
      return nullptr;
    }
  }
  Conglomerate const& Arena::get_conglomerate(WeakExpression expr) const {
    auto& entry = *(ArenaEntry*)expr.data();
    if(entry.discriminator == discriminator_conglomerate) {
      return entry.data.conglomerate;
    } else {
      std::terminate();
    }
  }
  bool Arena::holds_conglomerate(WeakExpression expr) const {
    auto& entry = *(ArenaEntry*)expr.data();
    return entry.discriminator == discriminator_conglomerate;
  }
  void Arena::clear_orphaned_expressions() { return impl->clear_orphaned_expressions(); }
  bool Arena::empty() const { return impl->empty(); }
  void Arena::drop(OwnedExpression&& expr) { return impl->drop(std::move(expr)); }
  void Arena::deref_weak(WeakExpression expr) { impl->deref_entry((ArenaEntry*)expr.data()); }
  void Arena::debug_dump() const {
    impl->pool.for_each([](ArenaEntry& entry) {
      std::cout << &entry << " [RefCt " << entry.reference_count << "]: ";
      switch(entry.discriminator) {
        case 0: std::cout << "Apply " << entry.data.apply.lhs.data() << " , " << entry.data.apply.rhs.data() << "\n"; break;
        case 1: std::cout << "Axiom\n"; break;
        case 2: std::cout << "Declaration\n"; break;
        case 3: std::cout << "Data\n"; break;
        case 4: std::cout << "Argument " << entry.data.argument.index << "\n"; break;
        case 5: std::cout << "Conglomerate " << entry.data.conglomerate.index << "\n"; break;
        case 6: std::cout << "Free List " << entry.data.free_list_entry.next_free_entry << "\n"; break;
        default: std::cout << "Corrupted entry!\n"; break;
      }
    });
    std::size_t bad_data_type_count = 0;
    for(auto const& ptr : impl->data_types) {
      if(ptr != nullptr) ++bad_data_type_count;
    }
    if(bad_data_type_count > 0) {
      std::cout << "Data types not destroyed: " << bad_data_type_count << "\n";
    }
  }
  WeakExpression Arena::weak_expression_of_data(Data const& data) {
    return WeakExpression{ //move the pointer backwards to the entry
      ((std::byte const*)&data) - offsetof(ArenaEntry, data)
    };
  }
  DataType* Arena::data_type_at_index(std::uint64_t type_index) {
    return impl->data_types[type_index].get();
  }
  void DataType::deref() {
    if(--ref_count == 0) {
      if(arena.impl->data_types[type_index].get() != this) std::terminate(); //???
      arena.impl->data_types[type_index] = nullptr;
    }
  }
}
