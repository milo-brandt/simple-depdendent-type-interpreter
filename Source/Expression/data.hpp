#ifndef EXPRESSION_DATA_HPP
#define EXPRESSION_DATA_HPP

#include <iostream>
#include <type_traits>
#include "../Utility/function.hpp"
#include <vector>
#include <optional>
#include <unordered_map>

namespace expression::tree {
  class Expression;
}

namespace expression::data {
  using Buffer = std::aligned_storage_t<24, alignof(void*)>;
  class DataType {
  public:
    virtual bool compare(Buffer const&, Buffer const&) const = 0;
    virtual void copy(Buffer const&, Buffer&) const = 0;
    virtual void move_destroy(Buffer&, Buffer&) const = 0;
    virtual void destroy(Buffer&) const = 0;
    virtual void debug_print(Buffer const&, std::ostream&) const = 0;
    virtual void pretty_print(Buffer const&, std::ostream&, mdb::function<void(tree::Expression)>) const = 0;
    virtual tree::Expression substitute(Buffer const&, std::vector<tree::Expression> const&) const = 0;
    virtual void visit_children(Buffer const&, mdb::function<void(tree::Expression const&)>) const = 0;
    virtual tree::Expression type_of(Buffer const&) const = 0;
    virtual ~DataType() = default;
  };
  std::uint64_t register_type(std::unique_ptr<DataType>);
  template<class T>
  class CType;
  class Data;
  bool operator==(Data const&, Data const&);
  std::ostream& operator<<(std::ostream&, Data const&);
  class Data {
  public:
    std::uint64_t type_index;
    Buffer storage;
    explicit Data(std::uint64_t type_index):type_index(type_index) {}
    template<class T> friend class CType;
    friend bool operator==(Data const&, Data const&);
    friend std::ostream& operator<<(std::ostream&, Data const&);
  public:
    Data(Data const&);
    Data(Data&&);
    Data& operator=(Data const&);
    Data& operator=(Data&&);
    ~Data();
    std::uint64_t get_type_index() const { return type_index; }
    void pretty_print(std::ostream& o, mdb::function<void(tree::Expression)> format_data) const;
    tree::Expression substitute(std::vector<tree::Expression> const&) const;
    void visit_children(mdb::function<void(tree::Expression const&)>) const;
    tree::Expression type_of() const;
  };
}

#endif
