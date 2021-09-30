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
#ifdef COMPILE_FOR_EMSCRIPTEN
inline auto format_error(std::string_view substring, std::string_view full) {
  return Print {
    [=](std::ostream& o) {
      detail::string_around(o, full, substring, [&] {
        o << "<strong><u><span style=\"color:red;\">" << substring << "</span></u></strong>";
      });
    }
  };
}
inline auto format_info(std::string_view substring, std::string_view full) {
  return Print {
    [=](std::ostream& o) {
      detail::string_around(o, full, substring, [&] {
        o << "<strong><span style=\"color:green;\">" << substring << "</span></strong>";
      });
    }
  };
}
inline auto format_info_pair(std::string_view substring_1, std::string_view substring_2, std::string_view full) {
  //TODO: I don't know, but not this for sure
  std::string_view joined{substring_1.data(), (std::size_t)(&*substring_2.end() - substring_1.data())};
  std::string_view between{&*substring_1.end(), (std::size_t)(substring_2.data() - &*substring_1.end())};
  return Print {
    [=](std::ostream& o) {
      detail::string_around(o, full, joined, [&] {
        o << "<strong><span style=\"color:cyan;\">" << substring_1 << "</span></strong>"
          << between
          << "<strong><span style=\"color:green;\">" << substring_2 << "</span></strong>";
      });
    }
  };
}
template<class T>
inline auto red_string(T&& inner) { //should only be used in single expression; doesn't capture by value
  return Print {
    [=](std::ostream& o) {
      o << "<span style=\"color:red;\">" << std::forward<T>(inner) << "</span>";
    }
  };
}
template<class T>
inline auto yellow_string(T&& inner) { //should only be used in single expression; doesn't capture by value
  return Print {
    [=](std::ostream& o) {
      o << "<span style=\"color:yellow;\">" << std::forward<T>(inner) << "</span>";
    }
  };
}
template<class T>
inline auto grey_string(T&& inner) { //should only be used in single expression; doesn't capture by value
  return Print {
    [=](std::ostream& o) {
      o << "<span style=\"color:grey;\">" << std::forward<T>(inner) << "</span>";
    }
  };
}
#else
inline auto format_error(std::string_view substring, std::string_view full) {
  return Print {
    [=](std::ostream& o) {
      detail::string_around(o, full, substring, [&] {
        o << termcolor::colorize << termcolor::red << termcolor::bold << termcolor::underline << substring << termcolor::reset;
      });
    }
  };
}
inline auto format_info(std::string_view substring, std::string_view full) {
  return Print {
    [=](std::ostream& o) {
      detail::string_around(o, full, substring, [&] {
        o << termcolor::colorize << termcolor::bold << termcolor::green << substring << termcolor::reset;
      });
    }
  };
}
inline auto format_info_pair(std::string_view substring_1, std::string_view substring_2, std::string_view full) {
  //TODO: I don't know, but not this for sure
  std::string_view joined{substring_1.data(), (std::size_t)(&*substring_2.end() - substring_1.data())};
  std::string_view between{&*substring_1.end(), (std::size_t)(substring_2.data() - &*substring_1.end())};
  return Print {
    [=](std::ostream& o) {
      detail::string_around(o, full, joined, [&] {
        o << termcolor::colorize << termcolor::bold << termcolor::green << substring_1 << termcolor::reset
          << between
          << termcolor::colorize << termcolor::bold << termcolor::cyan << substring_2 << termcolor::reset;
      });
    }
  };
}
template<class T>
inline auto red_string(T&& inner) { //should only be used in single expression; doesn't capture by value
  return Print {
    [=](std::ostream& o) {
      o << termcolor::colorize << termcolor::red << std::forward<T>(inner) << termcolor::reset;
    }
  };
}
template<class T>
inline auto yellow_string(T&& inner) { //should only be used in single expression; doesn't capture by value
  return Print {
    [=](std::ostream& o) {
      o << termcolor::colorize << termcolor::yellow << std::forward<T>(inner) << termcolor::reset;
    }
  };
}
template<class T>
inline auto grey_string(T&& inner) { //should only be used in single expression; doesn't capture by value
  return Print {
    [=](std::ostream& o) {
      o << termcolor::colorize << termcolor::bright_grey << std::forward<T>(inner) << termcolor::reset;
    }
  };
}
#endif

#endif
