#include <termcolor.hpp>
#include <string>
#include <tuple>

#ifndef CLI_FORMAT_HPP
#define CLI_FORMAT_HPP

template<class... Formats>
struct Format {
  std::string_view source;
  std::string_view part;
  std::tuple<Formats...> formats;
  Format(std::string_view source, std::string_view part, Formats... formats):source(source),part(part),formats(formats...) {}
};
template<class Printer>
struct Print {
  Printer printer;
};
template<class Printer> Print(Printer) -> Print<Printer>;
template<class Printer>
std::ostream& operator<<(std::ostream& o, Print<Printer> const& p) {
  p.printer(o);
  return o;
}
namespace detail {
  template<class InnerCallback>
  void string_around(std::ostream& o, std::string_view source, std::string_view part, InnerCallback&& format_inner) {
    auto start_offset = part.begin() - source.begin();
    auto end_offset = part.end() - source.begin();

    auto line_start = 0;
    auto line_count = 0;
    if(start_offset > 0) {
      auto last_line = source.rfind('\n', start_offset - 1);
      line_count = std::count(source.begin(), source.begin() + start_offset, '\n');
      if(last_line != std::string::npos) {
        line_start = last_line + 1;
      }
    }
    auto line_end = source.size();
    {
      auto next_line = source.find('\n', end_offset);
      if(next_line != std::string::npos) {
        line_end = next_line;
      }
    }

    if(start_offset - line_start > 30) {
      o << "..." << source.substr(start_offset - 30, 30);
    } else {
      o << source.substr(line_start, start_offset - line_start);
    }
    format_inner();
    if(line_end - end_offset  > 30) {
      o << source.substr(end_offset, 30) << "...";
    } else {
      o << source.substr(end_offset, line_end - end_offset);
    }
    if(line_count > 0) {
      o << " [line " << (line_count + 1) << "]";
    }
  }
}
template<class... Formats>
std::ostream& operator<<(std::ostream& o, Format<Formats...> const& format) {

  auto start_offset = format.part.begin() - format.source.begin();
  auto end_offset = format.part.end() - format.source.begin();

  auto line_start = 0;
  auto line_count = 0;
  if(start_offset > 0) {
    auto last_line = format.source.rfind('\n', start_offset - 1);
    line_count = std::count(format.source.begin(), format.source.begin() + start_offset, '\n');
    if(last_line != std::string::npos) {
      line_start = last_line + 1;
    }
  }
  auto line_end = format.source.size();
  {
    auto next_line = format.source.find('\n', end_offset);
    if(next_line != std::string::npos) {
      line_end = next_line;
    }
  }

  if(start_offset - line_start > 30) {
    o << "..." << format.source.substr(start_offset - 30, 30);
  } else {
    o << format.source.substr(line_start, start_offset - line_start);
  }
  o << termcolor::colorize;
  std::apply([&](auto const&... formats){ (o << ... << formats); }, format.formats);
  o << format.part << termcolor::reset;
  if(line_end - end_offset  > 30) {
    o << format.source.substr(end_offset, 30) << "...";
  } else {
    o << format.source.substr(end_offset, line_end - end_offset);
  }
  if(line_count > 0) {
    o << " [line " << (line_count + 1) << "]";
  }
  return o;
}
template<class... Formats>
Format<Formats...> format_substring(std::string_view substring, std::string_view full_string, Formats... formats) {
  return Format<Formats...>{full_string, substring, std::move(formats)...};
}
inline auto format_error(std::string_view substring, std::string_view full) {
  return format_substring(substring, full, termcolor::red, termcolor::bold, termcolor::underline);
}
inline auto format_info(std::string_view substring, std::string_view full) {
  return format_substring(substring, full, termcolor::green, termcolor::bold);
}
inline auto format_info_pair(std::string_view substring_1, std::string_view substring_2, std::string_view full) {
  //TODO: I don't know, but not this for sure
  std::string_view joined{substring_1.data(), (std::uint64_t)(&*substring_2.end() - substring_1.data())};
  std::string_view between{&*substring_1.end(), (std::uint64_t)(substring_2.data() - &*substring_1.end())};
  return Print {
    [=](std::ostream& o) {
      detail::string_around(o, full, joined, [&] {
        o << termcolor::bold << termcolor::green << substring_1 << termcolor::reset
          << between
          << termcolor::bold << termcolor::cyan << substring_2 << termcolor::reset;
      });
    }
  };
}

#endif
