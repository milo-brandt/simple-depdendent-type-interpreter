#include <string_view>



namespace result {
  struct lookup {
    std::string_view name;
  };
  struct add_name {

  };
};


class compilation_unit {
public:
  void try_compile(std::string_view str);
};
