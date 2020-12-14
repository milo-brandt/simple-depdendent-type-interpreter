#include "operator_tree.hpp"
#include "combinatorial_parser.hpp"
#include <concepts>
#include <vector>
#include <optional>

template<class T>
concept expression_tree_parser = requires(T const& v) {
  typename T::unary_t;
  {v.left_unary_op} -> any_parser;
  {v.right_unary_op} -> any_parser;
  {v.binary_op} -> any_parser;
  {v.superposition_op} -> std::convertible_to<std::optional<typename decltype(v.binary_op)::return_type> >;
  requires requires(typename decltype(v.left_unary_op)::return_type left_unary, typename decltype(v.right_unary_op)::return_type right_unary) {
    {v.embed_op(left_unary)} -> std::convertible_to<typename T::unary_t>;
    {v.embed_op(right_unary)} -> std::convertible_to<typename T::unary_t>;
  };
  requires requires(typename decltype(v.left_unary_op)::return_type const& left_unary, typename decltype(v.right_unary_op)::return_type const& right_unary, typename decltype(v.right_unary_op)::return_type const& binary) {
    {v.associate_left(left_unary, right_unary)} -> std::convertible_to<bool>;
    {v.associate_left(binary, right_unary)} -> std::convertible_to<bool>;
    {v.associate_left(left_unary, binary)} -> std::convertible_to<bool>;
    {v.associate_left(binary, binary)} -> std::convertible_to<bool>;
  };
};
template<expression_tree_parser P, class binary_t = typename decltype(std::declval<P>().binary_op)::return_type, class unary_t = typename P::unary_t>
dynamic_parser<operator_tree::tree<binary_t, unary_t> > build_parser(P const& parser) { //satisfies any_parser with output operator_tree::tree<binary_t, unary_t>
  using namespace operator_tree;
  using return_t = tree<binary_t, unary_t>;
  using left_unary_t = typename decltype(parser.left_unary_op)::return_type;
  using right_unary_t = typename decltype(parser.right_unary_op)::return_type;
  using stack_segment = std::variant<std::tuple<return_t, binary_t, std::string_view>, std::tuple<right_unary_t, std::string_view> >;
  struct parser_impl {
    using return_type = return_t;
    P const& parser;
    parse_result<return_t> parse(std::string_view str) const {
      std::vector<stack_segment> stack;
      std::optional<return_t> top;
      bool check_superposition = false;
      auto join_string_views = [](std::string_view a, std::string_view b) {
        auto start = std::min(a.data(), b.data());
        auto end = std::max(a.data() + a.size(), b.data() + b.size());
        return std::string_view{start, std::size_t(end-start)};
      };
      auto collapse_once = [&]() {
        assert(top);
        assert(!stack.empty());
        auto stack_top = std::move(stack.back());
        stack.pop_back();
        auto rhs_span = get_span_of(*top);
        if(stack_top.index() == 0) {
          auto& [lhs, op, op_span] = std::get<0>(stack_top);
          auto lhs_span = get_span_of(lhs);
          top = join_binary<binary_t, unary_t>(op, join_string_views(rhs_span, join_string_views(op_span, lhs_span)), op_span, std::move(lhs), std::move(*top));
        } else if(stack_top.index() == 1) {
          auto& [op, op_span] = std::get<1>(stack_top);
          top = join_unary<binary_t, unary_t>(op, join_string_views(op_span, rhs_span), op_span, std::move(*top));
        }
      };
      auto collapse_while = [&](auto predicate) {
        assert(top);
        while(!stack.empty() && predicate(stack.back())) {
          collapse_once();
        }
      };
      auto setup_superposition = [&]() {
        assert(check_superposition);
        check_superposition = false;
        collapse_while([&](stack_segment const& seg) {
          if(seg.index() == 0) return parser.associate_left(std::get<1>(std::get<0>(seg)), *parser.superposition_op);
          else return parser.associate_left(std::get<0>(std::get<1>(seg)), *parser.superposition_op);
        });
        stack.push_back(stack_segment{std::in_place_index<0>, std::move(*top), *parser.superposition_op, str.substr(0, 0)});
        top = std::nullopt;
      };
      while(true) {
        str = std::get<1>((*whitespace).parse(str)).remaining;
        if(top && !check_superposition) {
          auto unary_result = capture(parser.right_unary_op).parse(str);
          if(unary_result.index() == 1){
            auto& [ret, remaining] = std::get<1>(unary_result);
            auto& [op, op_span] = ret;
            collapse_while([&](stack_segment const& seg) {
              if(seg.index() == 0) return parser.associate_left(std::get<1>(std::get<0>(seg)), op);
              else return parser.associate_left(std::get<0>(std::get<1>(seg)), op);
            });
            auto rhs_span = get_span_of(*top);
            top = join_unary<binary_t, unary_t>(op, join_string_views(rhs_span, op_span), op_span, std::move(*top)); //apply operator
            str = remaining;
            continue;
          }
          auto binary_result = capture(parser.binary_op).parse(str);
          if(binary_result.index() == 1){
            auto& [ret, remaining] = std::get<1>(binary_result);
            auto& [op, op_span] = ret;
            collapse_while([&](stack_segment const& seg) {
              if(seg.index() == 0) return parser.associate_left(std::get<1>(std::get<0>(seg)), op);
              else return parser.associate_left(std::get<0>(std::get<1>(seg)), op);
            });
            stack.push_back(stack_segment{std::in_place_index<0>, std::move(*top), std::move(op), op_span});
            top = std::nullopt;
            str = remaining;
            continue;
          }
          if(!parser.superposition_op) {
            collapse_while([](auto&&){ return true; });
            return parse_results::success<return_t> {
              std::move(*top),
              str
            };
          } else {
            check_superposition = true;
          }
        } else {
          auto unary_result = capture(parser.left_unary_op).parse(str);
          if(unary_result.index() == 1){
            if(check_superposition) setup_superposition();
            auto& [ret, remaining] = std::get<1>(unary_result);
            auto& [op, op_span] = ret;
            stack.push_back(stack_segment{std::in_place_index<1>, std::move(op), op_span});
            str = remaining;
            continue;
          }
          auto id_result = identifier.parse(str);
          if(id_result.index() == 1) {
            if(check_superposition) setup_superposition();
            auto& [ret, remaining] = std::get<1>(id_result);
            top = make_identifier<binary_t, unary_t>(ret);
            str = remaining;
            continue;
          }
          auto paren_result = surrounded(recognizer("("), *this, recognizer(")")).parse(str);
          if(paren_result.index() == 1) {
            if(check_superposition) setup_superposition();
            auto& [ret, remaining] = std::get<1>(paren_result);
            top = ret;
            str = remaining;
            continue;
          }
          if(check_superposition) {
            collapse_while([](auto&&){ return true; });
            return parse_results::success<return_t> {
              std::move(*top),
              str
            };
          }
          return {};
        }
      }
    }
  };
  return parser_impl{parser};
}
