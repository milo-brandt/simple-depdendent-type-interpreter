#ifndef EXPRESSION_PATTERN_HPP
#define EXPRESSION_PATTERN_HPP

#include "expression.hpp"
#include <functional>
#include <unordered_map>
#include <optional>
#include <ranges>

namespace expressions {
  namespace patterns {
    struct match_all_t{};
    constexpr match_all_t match_all{};
    template<class predicate>
    struct destructure;
    template<class predicate>
    using pattern_data = std::variant<match_all_t, destructure<predicate> >;
    template<class predicate>
    using pattern = std::shared_ptr<pattern_data<predicate> >;
    template<class predicate>
    struct destructure {
      predicate head_expectation;
      std::vector<pattern<predicate> > args;
    };
    template<class base_kind, reduction_rule<base_kind> reducer_t, class predicate> requires requires(predicate const& p, base_kind b) {
      {p(b)} -> std::convertible_to<bool>;
    }
    std::optional<std::vector<expression<base_kind> > > match_expression(reducer_t const& reducer, expression<base_kind> expr, pattern<base_kind> pattern) {
      return std::visit(utility::overloaded{
        [&](match_all_t) -> std::optional<std::vector<expression<base_kind> > > {
          return std::vector<expression<base_kind> >{expr};
        },
        [&](destructure<predicate> const& d) -> std::optional<std::vector<expression<base_kind> > > {
          auto expr = reduce_head_with_reducer(reducer, expr);
          if(expr.index() == 1)
            return std::nullopt; //it held a lambda!
          auto& app = std::get<0>(expr);
          if(app.head.index() == 0)
            return std::nullopt; //head was an argument.
          if(app.rev_args.size() != d.args.size())
            return std::nullopt; //mismatched argument count.
          auto& head = std::get<1>(app.head);
          if(!d.head_expectation(std::move(head)))
            return std::nullopt; //head failed test.
          std::vector<expression<base_kind> > ret;
          for(std::size_t i = 0; i < d.args.size(); ++i) {
            auto result = match_expression(reducer, app.rev_args[app.rev_args.size() - 1 - i], d.args[i]);
            if(!result)
              return std::nullopt;
            ret.insert(ret.back(), result->begin(), result->end());
          }
          return ret;
        }
      }, *pattern);
    }
  };
};

#endif
