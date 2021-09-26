#ifndef STRING_HOLDER_HPP
#define STRING_HOLDER_HPP

#include <string>
#include <cstddef>
#include <iostream>

namespace imported_type {
  class StringHolder {
    std::byte* allocation;
    const char* m_begin;
    const char* m_end;
    StringHolder(std::byte*, const char*, const char*);
  public:
    StringHolder(std::string_view);
    StringHolder(StringHolder const&);
    StringHolder(StringHolder&&);
    StringHolder& operator=(StringHolder const&);
    StringHolder& operator=(StringHolder&&);
    ~StringHolder();
    std::size_t size() const;
    StringHolder substr(std::uint64_t start, std::uint64_t len) &&;
    StringHolder substr(std::uint64_t start, std::uint64_t len) const&;
    StringHolder substr(std::uint64_t start) &&;
    StringHolder substr(std::uint64_t start) const&;
    std::string_view get_string() const;
  };
  bool operator==(StringHolder const&, StringHolder const&);
  std::ostream& operator<<(std::ostream&, StringHolder const&);
}

#endif
