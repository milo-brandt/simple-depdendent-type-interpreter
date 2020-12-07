#include <cstdint>
#include <utility>
#include "Templates/TemplateUtility.h"

namespace type_theory {
  struct apply;
  struct lambda;
  struct argument;
  class any_expression {
    std::uint8_t* data;
    struct vtable_t {
      std::size_t primitive_type;
      any_expression(*reduce)(void*);
      void(*destroy)(void*);
    };
    template<class T>
    static vtable_t const* get_vtable_for_type() {
      auto destroyer = [](void* ptr){ ((T*)ptr)->~T(); };
      if constexpr(std::is_same_v<T, apply>) {
        static vtable_t const vtable = {
          .primitive_type = 1,
          .destroy = destroyer
        };
        return &vtable;
      } else if constexpr(std::is_same_v<T, lambda>) {
        static vtable_t const vtable = {
          .primitive_type = 2,
          .destroy = destroyer
        };
        return &vtable;
      } else if constexpr(std::is_same_v<T, argument>) {
        static vtable_t const vtable = {
          .primitive_type = 3,
          .destroy = destroyer
        };
        return &vtable;
      } else {
        static vtable_t const vtable = {
          .primitive_type = 0,
          .reduce = [](void* ptr) -> any_expression{ return ((T*)ptr)->reduce(); },
          .destroy = destroyer
        };
        return &vtable;
      }
    }
    std::size_t& get_ref_count() const { return *(std::size_t*)data; }
    vtable_t const* get_vptr() const { return *(vtable_t**)(data + sizeof(std::size_t)); }
    void* get_data_ptr() const { return data + sizeof(std::size_t) + sizeof(vtable_t*); }
    template<class T>
    T* get_data_ptr_as() const { return (T*)get_data_ptr(); }
  public:
    std::size_t get_type_id() const { return (std::size_t)get_vptr(); }
    template<class T> static std::size_t get_type_id_for_type() { return (std::size_t)get_vtable_for_type<std::remove_cv_t<std::remove_reference_t<T> > >(); }
    any_expression(any_expression const& other):data(other.data) {
      if(data) ++get_ref_count();
    }
    any_expression(any_expression&& other):data(other.data) {
      other.data = nullptr;
    }
    any_expression& operator=(any_expression const& other) {
      any_expression copy{other};
      this->~any_expression();
      new (this) any_expression(std::move(copy));
      return *this;
    }
    any_expression& operator=(any_expression&& other) {
      std::swap(data, other.data);
      return *this;
    }
    template<class T, class base_type = std::remove_cv_t<std::remove_reference_t<T> > >
    any_expression(T&& value):data((std::uint8_t*)malloc(sizeof(std::size_t) + sizeof(vtable_t*) + sizeof(base_type))) {
      vtable_t const* vtable = get_vtable_for_type<base_type>();
      new (data) std::size_t(1); //ref count
      new (data + sizeof(std::size_t)) vtable_t const*(vtable);
      new (data + sizeof(std::size_t) + sizeof(vtable_t*)) base_type(std::forward<T>(value));
    }
    std::size_t get_primitive_type() const { return get_vptr()->primitive_type; }
    any_expression reduce_once() const{
      assert(!get_primitive_type());
      return get_vptr()->reduce(get_data_ptr());
    }
    any_expression reduce_all() const {
      any_expression ret = *this;
      while(!ret.get_primitive_type()) ret = ret.reduce_once();
      return ret;
    }
    template<class callback_t, class ret_type = std::common_type_t<std::invoke_result_t<callback_t, apply const&>, std::invoke_result_t<callback_t, lambda const&>, std::invoke_result_t<callback_t, argument const&> > >
    ret_type reduce_and_visit(callback_t&& callback) const {
      any_expression reduced = reduce_all();
      switch(reduced.get_primitive_type()) {
        case 1: return callback(*reduced.get_data_ptr_as<apply const>());
        case 2: return callback(*reduced.get_data_ptr_as<lambda const>());
        case 3: return callback(*reduced.get_data_ptr_as<argument const>());
        default: std::terminate();
      }
    }
    ~any_expression() {
      if(data && --get_ref_count() == 0) {
        get_vptr()->destroy(get_data_ptr());
        free(data);
      }
    }
  };
};
