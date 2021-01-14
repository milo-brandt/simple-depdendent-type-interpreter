#include "../untyped_repl.hpp"
#include <catch.hpp>
#include <sstream>

using namespace untyped_repl;

TEST_CASE("tests from untyped_repl") {
  full_context ctx;
  auto preamble = load_file("Source/Tests/untyped_repl_test_file.txt");
  std::string_view remaining = preamble;
  while(true) {
    auto is_done = std::get<2>(parse::whitespaces(remaining)).position.empty();
    if(is_done) break;
    auto result = parse_terminated_command()(remaining);
    if(auto* err = std::get_if<0>(&result)) {
      std::stringstream failure;
      failure << "Parsed until " << err->position << "\n";
      failure << "Error: " << err->reason << "\n";
      for(auto& [frame, pos] : err->stack) {
        failure << "While parsing: " << frame << "\n";
        failure << "At: " << pos << "\n";
      }
      FAIL(failure.str());
    } else if(auto* err = std::get_if<1>(&result)) {
      std::stringstream failure;
      failure << "Parsed until " << err->position << "\n";
      failure << "Failure: " << err->reason << "\n";
      for(auto& [frame, pos] : err->stack) {
        failure << "While parsing: " << frame << "\n";
        failure << "At: " << pos << "\n";
      }
      FAIL(failure.str());
    } else {
      auto& success = std::get<2>(result);
      std::visit([&]<class T>(T x){
        if constexpr(std::is_same_v<T, parsed_test>){
          if(auto error = std::move(x).execute(ctx)) {
            FAIL_CHECK(*error);
          } else {
            CHECK(true); //add to counter of success :)
          }
        } else {
          std::move(x).execute(ctx);
        }
      }, std::move(success.value));
      remaining = success.position;
    }
  }
}
