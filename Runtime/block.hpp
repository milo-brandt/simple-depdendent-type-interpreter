#ifndef runtime_block_hpp
#define runtime_block_hpp

#include <vector>
#include <variant>
#include <memory>
#include <cstdint>

namespace runtime {
  /*
  namespace builder {
    struct allocation_spec{
      std::size_t size;
      std::size_t align;
    };

    class local_value { //a value in the procedure
      std::size_t index;
    };
    class tag {
      std::size_t index;
    };
    class local_storage {
      std::size_t index;
    };
    struct load{ local_value ptr; };
    struct store{ local_value what; local_value where; };
    struct call{ local_value f_ptr; };
    struct jump{ tag where; };
    struct br{ local_value condition; tag true_target; tag false_target; };
    class function {
    public:
      function(allocation_spec head);

      //memory manipulation
      local_value load(local_value ptr);
      local_value store(local_value what, local_value ptr);
      local_storage local_alloc(allocation_spec);

      //local operations
      local_value constant(std::size_t);
      local_value add(local_value, local_value);
      local_value sub(local_value, local_value);
      local_value eq(local_value, local_value);

      //flow control
      tag allocate_tag();
      void bind(tag);
      void jump(tag);
      void br(local_value condition, tag true_case, tag false_calse);

      void ret();
    };
  }
  namespace instructions {
    struct local_mov{ std::size_t src; std::size_t dst; };
    struct set_const{ std::size_t value; std::size_t dst; };
    struct store{ std::size_t src; std::size_t dst_ptr; };
    struct load{ std::size_t src_ptr; std::size_t dst; };
    struct add{ std::size_t lhs; std::size_t rhs; std::size_t dst; };
    struct sub{ std::size_t lhs; std::size_t rhs; std::size_t dst; };
    struct eq{ std::size_t lhs; std::size_t rhs; std::size_t dst; };
    struct jump{ std::size_t ip; };
    struct br{ std::size_t cond; std::size_t true_ip; std::size_t false_ip; };
    struct ret_t{}; constexpr ret_t ret;
    using any = std::variant<local_mov, set_const, store, load, add, sub, eq, jump, br, ret_t>;
  };
  using instruction = instructions::any;
  struct block {
    std::vector<instruction> code;
    void simulate(std::vector<std::size_t>& stack, std::size_t base_pointer);
  };*/
  namespace type {
    enum primitive { u1, u64 };
    struct pointer;
    struct union_; //union;
    struct structure; //struct;
    struct function;
    using _variant = std::variant<primitive, pointer, union_, structure, function>;
    using any = std::shared_ptr<_variant>;
    struct pointer {
      any element;
    };
    struct union_ {
      std::vector<any> variants;
    };
    struct structure {
      std::vector<any> members;
    };
    struct function {
      any ret;
      std::vector<any> args;
    };
    inline any make(primitive p) { return std::make_shared<_variant>(p); }
    inline any make_union(std::vector<any> vs){ return std::make_shared<_variant>(union_{std::move(vs)}); }
    inline any make_struct(std::vector<any> vs){ return std::make_shared<_variant>(structure{std::move(vs)}); }
    inline any make_pointer(any element){ return std::make_shared<_variant>(pointer{std::move(element)}); }
    inline any make_function(any ret, std::vector<any> args){ return std::make_shared<_variant>(function{ret, std::move(args)}); }
    namespace detail {
      template<primitive p> struct primitive_info;
      template<> struct primitive_info<u1>{ using native_type = bool; };
      template<> struct primitive_info<u64>{ using native_type = std::uint64_t; };
    };
    template<primitive p> using native_type = typename detail::primitive_info<p>::native_type;

    //using any_primitive = std::variant<native_type<u1>, native_type<u64> >;
  };

  struct local_value {
    std::size_t index;
    local_value(std::size_t index = -1):index(index){}
  };
  class block;
  struct jump_spec {
    std::size_t target_index;
    std::vector<local_value> args;
  };
  namespace instructions {
    namespace body {
      struct load { local_value where; };
      struct store { local_value value; local_value target; };
      struct constant { std::size_t value; };
      struct constant_fn { void* function; type::any ret; std::vector<type::any> args; };

      struct local_mutable { type::any t; }; //gives pointer
      struct get_element_ptr { local_value value; std::size_t index; };
      struct get_index_ptr { local_value value; local_value index; };

      struct bitcast { local_value v; type::any t; };

      struct add { local_value lhs; local_value rhs; };
      struct sub { local_value lhs; local_value rhs; };
      struct cmp { local_value lhs; local_value rhs; };
      struct call { local_value callee; std::vector<local_value> args; };
      using any = std::variant<load, store, constant, add, sub, cmp, local_mutable, get_element_ptr, get_index_ptr, constant_fn, call>;
    };
    namespace terminator {
      struct ret{ local_value value; };
      struct jump{ jump_spec spec; };
      struct br{ local_value condition; jump_spec if_true; jump_spec if_false; };
      using any = std::variant<ret, jump, br>;
    };
  };
  struct block {
    std::vector<type::any> args;
    std::vector<instructions::body::any> instructions;
    instructions::terminator::any end;
    std::vector<block> children;
    //std::size_t simulate(std::size_t* args) const;
    /*
    struct builder {
      local_value get_arg(std::size_t);
      local_value declare_local(allocation_spec);
      local_value constant(std::size_t);
      local_value load(local_value where);
      void store(local_value value, local_value where);
      local_value add(local_value lhs, local_value rhs);
      local_value sub(local_value lhs, local_value rhs);
      local_value cmp(local_value lhs, local_value rhs);
    };*/
  };
  /*class environment {
    struct _impl;
    std::unique_ptr<_impl> data;
  public:
    environment();
    ~runtime();
    using fn_type = std::size_t(*)(std::size_t*);
    fn_type add_function(block const&);
  };*/
}

#endif
