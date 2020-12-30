#ifndef EXPRESSION_FOLDING_UTILITY
#define EXPRESSION_FOLDING_UTILITY

#include <variant>
#include <vector>
#include <optional>

template<class T>
using folder_input_term = std::variant<typename T::term_t, typename T::binop_t, typename T::left_unop_t, typename T::right_unop_t>;
template<class T>
using folder_input_vector = std::vector<folder_input_term<T> >;
template<class F, class T = std::decay_t<F> >
typename T::fold_result fold_flat_expression(F&& folder, folder_input_vector<T> terms) {
  using term_t = typename T::term_t;
  using binop_t = typename T::binop_t;
  using left_unop_t = typename T::left_unop_t;
  using right_unop_t = typename T::right_unop_t;
  using fold_result = typename T::fold_result;
  std::vector<std::variant<std::pair<fold_result, binop_t>, left_unop_t> > stack;
  std::optional<fold_result> top;
  auto collapse_once = [&]() {
    assert(top);
    assert(!stack.empty());
    auto stack_top = std::move(stack.back());
    stack.pop_back();
    if(auto binop = std::get_if<0>(&stack_top)) {
      top = folder.process_binary(std::move(binop->first), std::move(binop->second), std::move(*top));
    } else if(auto unop = std::get_if<1>(&stack_top)) {
      top = folder.process_left_unary(std::move(*unop), std::move(*top));
    }
  };
  auto collapse_while = [&](auto&& predicate) {
    while(!stack.empty() && predicate(stack.back())) collapse_once();
  };
  for(auto& t : terms) {
    if(!top) {
      if(auto term = std::get_if<term_t>(&t)) {
        top = folder.process_term(std::move(*term));
      } else if(auto left_unop = std::get_if<left_unop_t>(&t)) {
        stack.emplace_back(std::in_place_index<1>, std::move(*left_unop));
      } else {
        std::terminate(); //passed list had a binary operator or right unary operator where the left side was not an expression
      }
    } else {
      if(auto op = std::get_if<binop_t>(&t)) {
        collapse_while([&](auto const& top){
          if(auto binop = std::get_if<0>(&top)) {
            return folder.associate_left(binop->second, *op);
          } else if(auto unop = std::get_if<1>(&top)) {
            return folder.associate_left(*unop, *op);
          }
          std::terminate();
        });
        stack.emplace_back(std::in_place_index<0>, std::move(*top), std::move(*op));
        top = std::nullopt;
      } else if(auto op = std::get_if<right_unop_t>(&t)) {
        collapse_while([&](auto const& top){
          if(auto binop = std::get_if<0>(&top)) {
            return folder.associate_left(binop->second, *op);
          } else if(auto unop = std::get_if<1>(&top)) {
            return folder.associate_left(*unop, *op);
          }
          std::terminate();
        });
        top = folder.process_right_unary(std::move(*top), std::move(*op));
      } else {
        std::terminate(); //passed list had a term or left unary operator where the left side was an expression.
      }
    }
  }
  if(!top) {
    std::terminate(); //passed list did not contain a term or had a dangling binary or left unary operator.
  }
  collapse_while([](auto const&){ return true; });
  return std::move(*top);
}



#endif
