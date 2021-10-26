#include "string_holder.hpp"
#include <atomic>
#include <cstring>

namespace primitive {
  namespace {
    constexpr auto counter_size = sizeof(std::atomic<std::uint64_t>);
    std::atomic<std::uint64_t>& counter_for(std::byte* data) {
      return *(std::atomic<std::uint64_t>*)data;
    }
    void unref_data(std::byte* data) {
      if(--counter_for(data) == 0) {
        using T = std::atomic<std::uint64_t>;
        counter_for(data).~T();
        free(data);
      }
    }
    void ref_data(std::byte* data) {
      ++counter_for(data);
    }
  }
  StringHolder::StringHolder(std::byte* allocation, const char* m_begin, const char* m_end):allocation(allocation),m_begin(m_begin),m_end(m_end) {}
  StringHolder::StringHolder(std::string_view str) {
    allocation = (std::byte*)malloc(counter_size + str.size() + 1);
    new (allocation) std::atomic<std::uint64_t>{1};
    std::memcpy(allocation + counter_size, str.data(), str.size());
    allocation[counter_size + str.size()] = (std::byte)0; //null terminator
    m_begin = (const char*)(allocation + counter_size);
    m_end = (const char*)(allocation + counter_size + str.size());
  }
  StringHolder::StringHolder(StringHolder const& other):allocation(other.allocation),m_begin(other.m_begin),m_end(other.m_end) {
    if(allocation) ref_data(allocation);
  }
  StringHolder::StringHolder(StringHolder&& other):allocation(other.allocation),m_begin(other.m_begin),m_end(other.m_end) {
    other.allocation = nullptr;
  }
  StringHolder& StringHolder::operator=(StringHolder const& other) {
    auto prior = allocation;
    allocation = other.allocation;
    m_begin = other.m_begin;
    m_end = other.m_end;
    if(allocation) ref_data(allocation);
    if(prior) unref_data(prior);
    return *this;
  }
  StringHolder& StringHolder::operator=(StringHolder&& other) {
    auto prior = allocation;
    allocation = other.allocation;
    m_begin = other.m_begin;
    m_end = other.m_end;
    other.allocation = nullptr;
    if(prior) unref_data(prior);
    return *this;
  }
  StringHolder::~StringHolder() {
    if(allocation) unref_data(allocation);
  }
  std::size_t StringHolder::size() const {
    return m_end - m_begin;
  }
  StringHolder StringHolder::substr(std::uint64_t start, std::uint64_t len) && {
    auto a = allocation;
    allocation = nullptr;
    return StringHolder{
      a,
      m_begin + start,
      m_begin + start + len
    };
  }
  StringHolder StringHolder::substr(std::uint64_t start, std::uint64_t len) const& {
    return StringHolder{*this}.substr(start, len); //copy and then substr
  }
  StringHolder StringHolder::substr(std::uint64_t start) && {
    return std::move(*this).substr(start, size() - start);
  }
  StringHolder StringHolder::substr(std::uint64_t start) const& {
    return substr(start, size() - start);
  }
  std::string_view StringHolder::get_string() const {
    return std::string_view{m_begin, size()};
  }
  bool operator==(StringHolder const& lhs, StringHolder const& rhs) {
    return lhs.get_string() == rhs.get_string();
  }
  std::ostream& operator<<(std::ostream& o, StringHolder const& str) {
    return o << "\"" << str.get_string() << "\"";
  }
}
