#include "formatter.hpp"
#include <sstream>

namespace expression::format {
  FormatResult format_expression(tree::Tree const& term, Context& evaluation_context, FormatContext format_context) {
    struct FormatSettings {
      bool parenthesize_application;
      bool parenthesize_arrow;
      std::uint64_t depth;
    };
    using namespace tree;
    auto arrow_pattern_out = match::Apply{
      match::Apply{
        full_match::External{match::Predicate{[arrow = format_context.arrow_external](std::uint64_t ext) { return ext == arrow; }}},
        match::Any{}
      },
      match::Any{}
    };
    struct Detail {
      Context& evaluation_context;
      FormatContext& format_context;
      decltype(arrow_pattern_out) arrow_pattern;
      std::unordered_set<std::uint64_t> included_externals;
      std::unordered_set<std::uint64_t> merge_sets(std::unordered_set<std::uint64_t> lhs, std::unordered_set<std::uint64_t> rhs) {
        for(auto v : rhs) {
          lhs.insert(v);
        }
        return lhs;
      }
      std::unordered_set<std::uint64_t> write(std::stringstream& out, tree::Tree const& term, FormatSettings settings) { //returns list of used arguments
        std::unordered_set<std::uint64_t> ret;
        if(auto match = arrow_pattern.try_match(term)) {
          tree::Tree const& domain = match->lhs.rhs;
          tree::Tree const& codomain = match->rhs;
          auto true_codomain = evaluation_context.reduce_once_at_root(tree::Apply{codomain, tree::Arg{settings.depth}});
          if(settings.parenthesize_arrow) {
            out << "(";
          }
          std::stringstream codomain_out;
          auto codomain_used_args = write(codomain_out, true_codomain, {.parenthesize_application = false, .parenthesize_arrow = false, .depth = settings.depth + 1});
          bool needs_named_arg = codomain_used_args.contains(settings.depth);
          if(needs_named_arg) {
            out << "($" << settings.depth << " : ";
          }
          auto domain_used_args = write(out, domain, {.parenthesize_application = false, .parenthesize_arrow = !needs_named_arg, .depth = settings.depth});
          if(needs_named_arg) {
            out << ")";
          }
          out << " -> ";
          out << codomain_out.str();
          if(settings.parenthesize_arrow) {
            out << ")";
          }
          return merge_sets(std::move(domain_used_args), std::move(codomain_used_args));
        }
        return term.visit(mdb::overloaded{
          [&](tree::Apply const& apply) {
            if(settings.parenthesize_application) {
              out << "(";
            }
            auto lhs_used_args = write(out, apply.lhs, {.parenthesize_application = false, .parenthesize_arrow = true, .depth = settings.depth});
            out << " ";
            auto rhs_used_args = write(out, apply.rhs, {.parenthesize_application = true, .parenthesize_arrow = true, .depth = settings.depth});
            if(settings.parenthesize_application) {
              out << ")";
            }
            return merge_sets(std::move(lhs_used_args), std::move(rhs_used_args));
          },
          [&](tree::External const& external) {
            out << format_context.format_external(external.index);
            included_externals.insert(external.index);
            return std::unordered_set<std::uint64_t>{};
          },
          [&](tree::Arg const& arg) {
            out << "$" << arg.index;
            return std::unordered_set<std::uint64_t>{arg.index};
          }
        });
      }
    };
    Detail detail{evaluation_context, format_context, std::move(arrow_pattern_out)};
    std::stringstream out;
    detail.write(out, term, {.parenthesize_application = false, .parenthesize_arrow = false, .depth = 0});
    return {
      .result = out.str(),
      .included_externals = std::move(detail.included_externals)
    };
  }
}
