#ifndef EXPRESSION_ARENA_HPP
#define EXPRESSION_ARENA_HPP

#include <memory>
#include <type_traits>
#include "../Utility/function.hpp"
#include "../Utility/event.hpp"
#include <span>

/*
  This file is merely a structure for *holding* trees. It
  replaces earlier literal implementations of tree.

  Trees are uniqued.
*/

namespace new_expression {
  class Arena;
  class WeakExpression;
  struct UniqueExpression;
  class OwnedExpression { //an expression that we own, but which is not automatically deref'd
    std::size_t p_index;
    explicit OwnedExpression(std::size_t index):p_index(index) {}
    friend Arena;
    friend UniqueExpression;
  public:
    OwnedExpression(OwnedExpression&&) noexcept = default;
    OwnedExpression(OwnedExpression const&) = delete;
    OwnedExpression& operator=(OwnedExpression&&) noexcept = default;
    OwnedExpression& operator=(OwnedExpression const&) = delete;

    std::size_t index() const { return p_index; }
    friend bool operator==(OwnedExpression const& lhs, OwnedExpression const& rhs) {
      return lhs.p_index == rhs.p_index;
    }
    friend bool operator!=(OwnedExpression const& lhs, OwnedExpression const& rhs) {
      return lhs.p_index != rhs.p_index;
    }
  };
  class WeakExpression {
    std::size_t p_index;
    explicit WeakExpression(std::size_t index):p_index(index) {}
    friend Arena;
  public:
    WeakExpression(OwnedExpression const& expr) noexcept:p_index(expr.index()) {}
    WeakExpression(OwnedExpression&&) = delete;
    std::size_t index() const { return p_index; }
    friend bool operator==(WeakExpression const& lhs, WeakExpression const& rhs) {
      return lhs.p_index == rhs.p_index;
    }
    friend bool operator!=(WeakExpression const& lhs, WeakExpression const& rhs) {
      return lhs.p_index != rhs.p_index;
    }
  };
  struct UniqueExpression { //A RAII wrapper for expressions.
    Arena* p_arena;
    OwnedExpression p_expression;
  public:
    UniqueExpression():p_arena{nullptr}, p_expression{(std::size_t)-1} {}
    UniqueExpression(std::nullptr_t):UniqueExpression() {}
    UniqueExpression(Arena* arena, OwnedExpression expression):p_arena(arena), p_expression(std::move(expression)) {}
    UniqueExpression(UniqueExpression&& other);
    UniqueExpression& operator=(UniqueExpression&&);
    Arena* arena() const { return p_arena; }
    WeakExpression get() const { return p_expression; }
    friend bool operator==(UniqueExpression const& lhs, UniqueExpression const& rhs) {
      return lhs.p_expression == rhs.p_expression;
    }
    friend bool operator!=(UniqueExpression const& lhs, UniqueExpression const& rhs) {
      return lhs.p_expression != rhs.p_expression;
    }
    ~UniqueExpression();
  };
  using Buffer = std::aligned_storage_t<24,8>;
  class DataType {
  public:
    //responsible for uniquing elements of this type.
    virtual void move_destroy(Buffer&, Buffer&) = 0; //move second argument to first. Destroy second argument.
      //allowed to assume a memcpy has already occurred
    virtual void destroy(WeakExpression, Buffer&) = 0;
    virtual void debug_print(Buffer const&, std::ostream&) = 0;
    virtual void pretty_print(Buffer const&, std::ostream&, mdb::function<void(WeakExpression)>) = 0;
    virtual bool all_subexpressions(mdb::function<bool(WeakExpression)>) = 0;
    virtual OwnedExpression modify_subexpressions(WeakExpression, mdb::function<OwnedExpression(WeakExpression)>) = 0;
    ~DataType() = default;
  };
  struct Apply {
    OwnedExpression lhs;
    OwnedExpression rhs;
  };
  struct Axiom {};
  struct Declaration {};
  struct Data {
    std::uint64_t type_index;
    Buffer buffer;
  };
  struct Argument {
    std::uint64_t index;
  };
  struct Conglomerate {
    std::uint64_t index;
  };
  class Arena {
    struct Impl;
    std::unique_ptr<Impl> impl;
    std::uint64_t next_type_index();
    void add_type(std::unique_ptr<DataType>&&);
    std::pair<OwnedExpression, Buffer*> allocate_data(std::uint64_t type_index);
    std::pair<std::size_t, void const*> visit_data(WeakExpression expr) const;
  public:
    Arena();
    Arena(Arena&&);
    Arena& operator=(Arena&&);
    ~Arena();
    [[nodiscard]] OwnedExpression apply(OwnedExpression, OwnedExpression);
    [[nodiscard]] OwnedExpression axiom();
    [[nodiscard]] OwnedExpression declaration();
    [[nodiscard]] OwnedExpression argument(std::uint64_t index);
    [[nodiscard]] OwnedExpression conglomerate(std::uint64_t index);
    template<class Filler>
    [[nodiscard]] OwnedExpression data(std::uint64_t type_index, Filler&& filler) {
      auto [ret, buffer] = allocate_data(type_index);
      filler(buffer);
      return std::move(ret);
    }
    [[nodiscard]] OwnedExpression copy(OwnedExpression const&);
    OwnedExpression copy(OwnedExpression&&) = delete; //issue error for calling this on rvalues
    [[nodiscard]] OwnedExpression copy(WeakExpression const&);
    void drop(OwnedExpression&&);
    template<class Factory>
    auto create_data_type(Factory&& factory) {
      auto new_type = factory(*this, next_type_index());
      using SpecificDataType = std::remove_reference_t<decltype(*new_type)>;
      static_assert(!std::is_const_v<SpecificDataType>, "Data type factory must not return a pointer to a const value.");
      static_assert(std::is_base_of_v<DataType, SpecificDataType>, "Data type factory must return a pointer derived from DataType.");
      auto unique_type = std::unique_ptr<DataType>{std::move(new_type)}; //should work for both raw pointers and unique_ptrs.
      SpecificDataType* ret = (SpecificDataType*)unique_type.get();
      add_type(std::move(unique_type));
      return ret;
    }
    mdb::Event<std::span<WeakExpression> >& terms_erased();
    Apply const* get_if_apply(WeakExpression) const;
    Apply const& get_apply(WeakExpression) const;
    bool holds_apply(WeakExpression) const;
    Axiom const* get_if_axiom(WeakExpression) const;
    Axiom const& get_axiom(WeakExpression) const;
    bool holds_axiom(WeakExpression) const;
    Declaration const* get_if_declaration(WeakExpression) const;
    Declaration const& get_declaration(WeakExpression) const;
    bool holds_declaration(WeakExpression) const;
    Data const* get_if_data(WeakExpression) const;
    Data const& get_data(WeakExpression) const;
    bool holds_data(WeakExpression) const;
    Argument const* get_if_argument(WeakExpression) const;
    Argument const& get_argument(WeakExpression) const;
    bool holds_argument(WeakExpression) const;
    Conglomerate const* get_if_conglomerate(WeakExpression) const;
    Conglomerate const& get_conglomerate(WeakExpression) const;
    bool holds_conglomerate(WeakExpression) const;
    template<class Visitor>
    decltype(auto) visit(WeakExpression expr, Visitor&& visitor) const {
      auto [index, data] = visit_data(expr);
      switch(index) {
        case 0: return std::forward<Visitor>(visitor)(*(Apply const*)data);
        case 1: return std::forward<Visitor>(visitor)(*(Axiom const*)data);
        case 2: return std::forward<Visitor>(visitor)(*(Declaration const*)data);
        case 3: return std::forward<Visitor>(visitor)(*(Data const*)data);
        case 4: return std::forward<Visitor>(visitor)(*(Argument const*)data);
        case 5: return std::forward<Visitor>(visitor)(*(Conglomerate const*)data);
        default: std::terminate();
      }
    }

    void clear_orphaned_expressions(); //primarily for testing.
    bool empty() const; //primarily for testing
    void debug_dump() const;
  };
}

#endif