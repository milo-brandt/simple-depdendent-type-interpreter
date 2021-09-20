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

#endif
