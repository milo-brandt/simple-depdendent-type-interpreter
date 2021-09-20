#include "data.hpp"
#include <vector>
#include "expression_tree.hpp"

namespace expression::data {
  namespace {
    std::vector<std::unique_ptr<DataType> >& get_type_vector() {
      static auto ret = [] {
        std::vector<std::unique_ptr<DataType> > ret;
        ret.push_back(nullptr); //0 = bad
        return ret;
      }();
      return ret;
    }
    DataType const& get_data_type(std::uint64_t index) {
      return *get_type_vector()[index];
    }
  }
  std::uint64_t register_type(std::unique_ptr<DataType> t) {
    auto ret = get_type_vector().size();
    get_type_vector().push_back(std::move(t));
    return ret;
  }
  Data::Data(Data const& other):type_index(other.type_index) {
    if(type_index) {
      get_data_type(type_index).copy(other.storage, storage);
    }
  }
  Data::Data(Data&& other):type_index(other.type_index) {
    if(type_index) {
      get_data_type(type_index).move_destroy(other.storage, storage);
      other.type_index = 0;
    }
  }
  Data& Data::operator=(Data const& other) {
    if(&other == this) return *this;
    this->~Data();
    new (this) Data{other};
    return *this;
  }
  Data& Data::operator=(Data&& other) {
    if(&other == this) return *this;
    this->~Data();
    new (this) Data{std::move(other)};
    return *this;
  }
  Data::~Data() {
    if(type_index) {
      get_data_type(type_index).destroy(storage);
    }
  }
  void Data::pretty_print(std::ostream& o, mdb::function<void(tree::Expression)> data_print) const {
    get_data_type(type_index).pretty_print(storage, o, std::move(data_print));
  }
  tree::Expression Data::substitute(std::vector<tree::Expression> const& args) const {
    return get_data_type(type_index).substitute(storage, args);
  }
  std::optional<tree::Expression> Data::remap_args(std::unordered_map<std::uint64_t, std::uint64_t> const& arg_map) const {
    return get_data_type(type_index).remap_args(storage, arg_map);
  }
  tree::Expression Data::type_of() const {
    return get_data_type(type_index).type_of(storage);
  }
  std::ostream& operator<<(std::ostream& o, Data const& data) {
    get_data_type(data.type_index).debug_print(data.storage, o);
    return o;
  }
  bool operator==(Data const& lhs, Data const& rhs) {
    return lhs.type_index == rhs.type_index && get_data_type(lhs.type_index).compare(lhs.storage, rhs.storage);
  }

}
