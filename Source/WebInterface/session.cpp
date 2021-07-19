/*
#include "session.hpp"
#include "../Utility/overloaded.hpp"
#include <functional>
#include <sstream>

namespace Web {
  struct Session::Impl {
    combinator::InterfaceState state;
  };
  Session::Session():impl(std::make_unique<Impl>()) {}
  Session::Session(Session&&) = default;
  Session& Session::operator=(Session&&) = default;
  Session::~Session() = default;
  namespace {
    std::vector<combinator::ReplacementRule> default_rules() {
      using namespace combinator;
      return {
        ReplacementRule{
          .pattern = Pattern{
            Pattern{
              Pattern{PatternLeaf{0}},
              Pattern{PatternLeaf{}}
            },
            Pattern{PatternLeaf{}}
          },
          .replacement = Replacement{
            arg_replacement(0),
            Replacement{
              arg_replacement(0),
              arg_replacement(1)
            }
          }
        }
      };
    }
    combinator::Term get_default() {
      using namespace combinator;
      return Term{
        Term{
          Term{
            Term{
              Term{TermLeaf{0}},
              Term{TermLeaf{0}}
            },
            Term{
              Term{TermLeaf{0}},
              Term{TermLeaf{0}}
            },
          },
          Term{TermLeaf{1}}
        },
        Term{TermLeaf{2}}
      };
    }
    void format_leaf(std::stringstream& out, combinator::TermLeaf const& leaf) {
      static constexpr const char* strings[] = {"double", "f", "x"};
      if(leaf.index < std::extent_v<decltype(strings)>) {
        out << strings[leaf.index];
      } else {
        out << "{" << leaf.index << "}";
      }
    }
  }
  combinator::InterfaceOutput Session::respond(Request::Any request) {
    auto set_value = [&](combinator::Term const& term) {
      auto [output, state] = combinator::process_term(term, default_rules(), &format_leaf);
      impl->state = std::move(state);
      return std::move(output);
    };
    return std::visit(mdb::overloaded{
      [&](Request::Initialize) {
        return set_value(get_default());
      },
      [&](Request::Simplify const& simplify) {
        if(simplify.index >= impl->state.updates.size()) {
          throw std::runtime_error("Simplification index out of range!");
        } else {
          return set_value(impl->state.updates[simplify.index]);
        }
      }
    }, request);
  }
}
*/
