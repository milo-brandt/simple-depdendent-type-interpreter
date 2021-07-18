#include "resolved_tree.hpp"
#include "../Utility/overloaded.hpp"


namespace compiler::resolution {
  namespace {
    template<class Then>
    decltype(auto) with_stack_name(std::unordered_map<std::string, std::uint64_t>& map, std::uint64_t& stack_size, std::string name, Then&& then) {
      auto [it, ok] = map.insert(std::make_pair(name, stack_size));
      if(ok) {
        ++stack_size;
        decltype(auto) ret = std::forward<Then>(then)();
        --stack_size;
        map.erase(name);
        return ret;
      } else {
        std::swap(it->second, stack_size);
        ++stack_size;
        decltype(auto) ret = std::forward<Then>(then)();
        --stack_size;
        std::swap(it->second, stack_size);
        return ret;
      }
    }
    template<class Then>
    decltype(auto) with_stack_frame(std::uint64_t& stack_size, Then&& then) {
      ++stack_size;
      decltype(auto) ret = std::forward<Then>(then)();
      --stack_size;
      return ret;
    }
    template<class Then>
    decltype(auto) with_optional_stack_name(std::unordered_map<std::string, std::uint64_t>& map, std::uint64_t& stack_size, std::optional<std::string_view> name, Then&& then) {
      if(name) {
        return with_stack_name(map, stack_size, std::string{*name}, std::forward<Then>(then));
      } else {
        return with_stack_frame(stack_size, std::forward<Then>(then));
      }
    }
  }
  mdb::Result<Output, error::Any> resolve(Input input) {
    struct Impl {
      NameContext const& context;
      std::uint64_t embed_arrow;
      std::unordered_map<std::string, std::uint64_t> stack_names;
      std::uint64_t stack_size;
      expression_parser::path::Path absolute_position;
      mdb::Result<Output, error::Any> resolve(expression_parser::output::Tree const& tree, relative::Position position) {
        auto resolve_in = [&](expression_parser::output::Tree const& tree, std::uint64_t step) {
          absolute_position.steps.push_back(step);
          auto ret = resolve(tree, relative::Step{step});
          absolute_position.steps.pop_back();
          return ret;
        };
        return tree.visit(mdb::overloaded{
          [&](expression_parser::output::Apply const& apply) -> mdb::Result<Output, error::Any> {
            return mdb::gather_map([&](Output&& lhs, Output&& rhs) -> Output {
              return Output{
                .tree = located_output::Apply{
                  .lhs = std::move(lhs.tree),
                  .rhs = std::move(rhs.tree),
                  .relative_position = position
                }
              };
            }, [&] {
              return resolve_in(apply.lhs, 0);
            }, [&] {
              return resolve_in(apply.rhs, 1);
            });
          },
          [&](expression_parser::output::Lambda const& lambda) -> mdb::Result<Output, error::Any> {
            auto parse_body = [&] {
              return with_optional_stack_name(stack_names, stack_size, lambda.arg_name, [&] {
                return resolve_in(lambda.body, 0);
              });
            };
            if(lambda.type) {
              return mdb::gather_map([&] (Output&& type, Output&& body) {
                  return Output{
                    .tree = located_output::Lambda{
                      .body = std::move(body.tree),
                      .type = std::move(type.tree),
                      .relative_position = position
                    }
                  };
                },
                [&] { return resolve_in(*lambda.type, 1); },
                parse_body
              );
            } else {
              return mdb::map(parse_body(), [&](Output&& body) {
                return Output{
                  .tree = located_output::Lambda {
                    .body = std::move(body.tree),
                    .type = located_output::Hole{
                      .relative_position = relative::LambdaType{}
                    },
                    .relative_position = position
                  }
                };
              });
            }
          },
          [&](expression_parser::output::Identifier const& identifier) -> mdb::Result<Output, error::Any> {
            std::string id{identifier.id};
            if(stack_names.contains(id)) {
              return Output{
                .tree = located_output::Local{
                  .stack_index = stack_names.at(id),
                  .relative_position = position
                }
              };
            } else if(context.names.contains(id)) {
              return Output{
                .tree = located_output::Embed{
                  .embed_index = context.names.at(id).embed_index, //TODO: ye
                  .relative_position = position
                }
              };
            } else {
              return {mdb::in_place_error, error::UnrecognizedId {
                absolute_position
              }};
            }
          },
          [&](expression_parser::output::Hole const& hole) -> mdb::Result<Output, error::Any> {
            return Output{
              .tree = located_output::Hole{
                .relative_position = position
              }
            };
          },
          [&](expression_parser::output::Arrow const& arrow) -> mdb::Result<Output, error::Any> {
            return mdb::gather_map([&] (Output&& domain, Output&& codomain) {
                return Output{
                  .tree = located_output::Apply{
                    .lhs = located_output::Apply{
                      .lhs = located_output::Embed{ .embed_index = embed_arrow, .relative_position = relative::ArrowExpand::arrow_axiom },
                      .rhs = domain.tree,
                      .relative_position = relative::ArrowExpand::apply_domain
                    },
                    .rhs = located_output::Lambda {
                      .body = std::move(codomain.tree),
                      .type = domain.tree,
                      .relative_position = relative::ArrowExpand::codomain_lambda
                    },
                    .relative_position = position
                  }
                };
              },
              [&] { return resolve_in(arrow.domain, 0); },
              [&] {
                return with_optional_stack_name(stack_names, stack_size, arrow.arg_name, [&] {
                  return resolve_in(arrow.codomain, 1);
                });
              }
            );
          }
        });
      }
    };
    return Impl{input.context, input.embed_arrow}.resolve(input.tree, relative::Root{});
  };
}
