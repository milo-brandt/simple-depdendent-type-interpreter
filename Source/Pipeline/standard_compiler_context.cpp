#include "standard_compiler_context.hpp"
#include "../Module/execute.hpp"
#include "../Utility/vector_utility.hpp"

namespace {
  expr_module::Core module_primitives_module() {
    using namespace expr_module::step;
    using namespace expr_module::rule_step;
    using namespace expr_module::rule_replacement;
    return {.value_import_size=6,.data_type_import_size=0,.c_replacement_import_size=0,.register_count=31,.output_size=4,.steps={Declare{0},Declare{1},Declare{2},Declare{3},Declare{4},Declare{5},Declare{6},Declare{7},Declare{8},Declare{9},Axiom{10},Embed{4,11},Axiom{12},Declare{13},Declare{14},Declare{15},Declare{16},Declare{17},Declare{18},Declare{19},Declare{20},Axiom{21},Declare{22},Axiom{23},Declare{24},Declare{25},Embed{5,26},Embed{0,27},RegisterType{26,27},Embed{3,27},Embed{0,28},Apply{27,28,27},Apply{27,16,27},RegisterType{25,27},Embed{3,27},Embed{0,28},Apply{27,28,27},Apply{27,6,27},RegisterType{24,27},Embed{1,27},Embed{0,28},Apply{27,28,27},Apply{27,25,27},RegisterType{23,27},Embed{1,27},Embed{3,28},Embed{0,29},Apply{28,29,28},Apply{28,6,28},Apply{27,28,27},Embed{2,28},Embed{0,29},Apply{28,29,28},Embed{0,29},Apply{28,29,28},Embed{3,29},Embed{0,30},Apply{29,30,29},Apply{29,6,29},Apply{28,29,28},Apply{27,28,27},RegisterType{22,27},Embed{0,27},RegisterType{21,27},Embed{0,27},RegisterType{20,27},Embed{0,27},RegisterType{19,27},Embed{3,27},Embed{0,28},Apply{27,28,27},Apply{27,20,27},RegisterType{18,27},Embed{3,27},Embed{0,28},Apply{27,28,27},Apply{27,19,27},RegisterType{17,27},Embed{0,27},RegisterType{16,27},Apply{9,1,27},RegisterType{15,27},Embed{0,27},Apply{14,27,27},RegisterType{14,27},Embed{3,27},Embed{0,28},Apply{27,28,27},Apply{27,6,27},RegisterType{13,27},Embed{1,27},Apply{9,1,28},Apply{27,28,27},Embed{2,28},Embed{0,29},Apply{28,29,28},Embed{3,29},Embed{0,30},Apply{29,30,29},Apply{29,6,29},Apply{28,29,28},Apply{9,1,29},Apply{28,29,28},Apply{27,28,27},RegisterType{0,27},Embed{0,27},RegisterType{6,27},Embed{3,27},Embed{0,28},Apply{27,28,27},Apply{27,6,27},RegisterType{1,27},Apply{9,1,27},RegisterType{2,27},Embed{3,27},Embed{0,28},Apply{27,28,27},Apply{27,6,27},RegisterType{3,27},Apply{9,1,27},Apply{14,27,27},RegisterType{4,27},Embed{0,27},RegisterType{5,27},Embed{1,27},Embed{0,28},Apply{27,28,27},Apply{27,17,27},RegisterType{11,27},Apply{9,3,27},RegisterType{8,27},Apply{4,2,27},RegisterType{7,27},Embed{0,27},RegisterType{10,27},Embed{3,27},Embed{0,28},Apply{27,28,27},Apply{27,6,27},Apply{14,27,27},RegisterType{9,27},Embed{1,27},Apply{11,21,28},Apply{27,28,27},Apply{27,18,27},RegisterType{12,27},Embed{1,27},Apply{27,26,27},Argument{0,28},Apply{8,28,28},Apply{27,28,27},Rule{.head=25,.args_captured=1,.steps={PullArgument{}},.replacement=Substitution{27}},Rule{.head=24,.args_captured=1,.steps={PullArgument{}},.replacement=Substitution{26}},Embed{1,27},Apply{27,5,27},Argument{0,28},Apply{27,28,27},Rule{.head=22,.args_captured=1,.steps={PullArgument{}},.replacement=Substitution{27}},Apply{11,21,27},Apply{14,27,27},Rule{.head=20,.args_captured=0,.steps={},.replacement=Substitution{27}},Embed{0,27},Apply{14,27,27},Rule{.head=19,.args_captured=0,.steps={},.replacement=Substitution{27}},Rule{.head=18,.args_captured=1,.steps={PullArgument{}},.replacement=Substitution{10}},Embed{0,27},Rule{.head=17,.args_captured=1,.steps={PullArgument{}},.replacement=Substitution{27}},Embed{0,27},Apply{14,27,27},Rule{.head=16,.args_captured=0,.steps={},.replacement=Substitution{27}},Embed{0,27},Rule{.head=15,.args_captured=2,.steps={PullArgument{},PullArgument{}},.replacement=Substitution{27}},Embed{1,27},Argument{0,28},Apply{27,28,27},Embed{2,28},Embed{0,29},Apply{28,29,28},Embed{0,29},Apply{28,29,28},Argument{0,29},Apply{28,29,28},Apply{27,28,27},Rule{.head=14,.args_captured=1,.steps={PullArgument{}},.replacement=Substitution{27}},Embed{0,27},Rule{.head=13,.args_captured=1,.steps={PullArgument{}},.replacement=Substitution{27}},Embed{1,27},Argument{1,28},Apply{24,28,28},Apply{27,28,27},Argument{0,28},Argument{1,29},Apply{28,29,28},Apply{27,28,27},Rule{.head=0,.args_captured=2,.steps={PullArgument{},PullArgument{}},.replacement=Substitution{27}},Embed{1,27},Embed{0,28},Apply{27,28,27},Apply{27,13,27},Rule{.head=6,.args_captured=0,.steps={},.replacement=Substitution{27}},Embed{1,27},Apply{27,26,27},Argument{0,28},Apply{15,28,28},Apply{27,28,27},Rule{.head=1,.args_captured=1,.steps={PullArgument{}},.replacement=Substitution{27}},Argument{0,27},Apply{14,27,27},Rule{.head=2,.args_captured=2,.steps={PullArgument{},PullArgument{}},.replacement=Substitution{27}},Apply{14,26,27},Rule{.head=3,.args_captured=1,.steps={PullArgument{}},.replacement=Substitution{27}},Argument{0,27},Apply{0,27,27},Apply{9,27,27},Rule{.head=4,.args_captured=1,.steps={PullArgument{}},.replacement=Substitution{27}},Embed{0,27},Rule{.head=5,.args_captured=0,.steps={},.replacement=Substitution{27}},Embed{1,27},Argument{0,28},Apply{27,28,27},Argument{0,28},Apply{7,28,28},Argument{1,29},Apply{28,29,28},Apply{27,28,27},Rule{.head=8,.args_captured=2,.steps={PullArgument{},PullArgument{}},.replacement=Substitution{27}},Rule{.head=7,.args_captured=3,.steps={PullArgument{},PullArgument{},PullArgument{}},.replacement=Substitution{21}},Embed{3,27},Embed{0,28},Apply{27,28,27},Argument{0,28},Apply{22,28,28},Apply{27,28,27},Rule{.head=9,.args_captured=1,.steps={PullArgument{}},.replacement=Substitution{27}},Export{10},Export{12},Export{21},Export{23}}};
  }
  expr_module::Core empty_vec_module() {
    using namespace expr_module::step;
    using namespace expr_module::rule_step;
    using namespace expr_module::rule_replacement;
    return {.value_import_size=5,.data_type_import_size=0,.c_replacement_import_size=0,.register_count=7,.output_size=1,.steps={Declare{0},Declare{1},Declare{2},Axiom{3},Embed{1,4},Embed{0,5},Apply{4,5,4},Apply{4,2,4},RegisterType{3,4},Embed{3,4},Embed{0,5},Apply{4,5,4},Apply{4,1,4},RegisterType{2,4},Embed{0,4},RegisterType{1,4},Embed{0,4},Apply{0,4,4},RegisterType{0,4},Embed{4,4},Argument{0,5},Apply{4,5,4},Rule{.head=2,.args_captured=1,.steps={PullArgument{}},.replacement=Substitution{4}},Embed{0,4},Apply{0,4,4},Rule{.head=1,.args_captured=0,.steps={},.replacement=Substitution{4}},Embed{1,4},Argument{0,5},Apply{4,5,4},Embed{2,5},Embed{0,6},Apply{5,6,5},Embed{0,6},Apply{5,6,5},Argument{0,6},Apply{5,6,5},Apply{4,5,4},Rule{.head=0,.args_captured=1,.steps={PullArgument{}},.replacement=Substitution{4}},Export{3}}};
  }
  expr_module::Core push_vec_module() {
    using namespace expr_module::step;
    using namespace expr_module::rule_step;
    using namespace expr_module::rule_replacement;
    return {.value_import_size=5,.data_type_import_size=0,.c_replacement_import_size=0,.register_count=22,.output_size=1,.steps={Declare{0},Declare{1},Declare{2},Axiom{3},Declare{4},Declare{5},Declare{6},Declare{7},Declare{8},Declare{9},Declare{10},Declare{11},Declare{12},Declare{13},Declare{14},Declare{15},Declare{16},Declare{17},Apply{15,0,18},RegisterType{17,18},Embed{0,18},RegisterType{16,18},Apply{13,1,18},Apply{10,18,18},RegisterType{15,18},Embed{3,18},Embed{0,19},Apply{18,19,18},Apply{18,12,18},RegisterType{14,18},Embed{3,18},Embed{0,19},Apply{18,19,18},Apply{18,12,18},Apply{10,18,18},RegisterType{13,18},Apply{13,1,18},RegisterType{0,18},Embed{3,18},Embed{0,19},Apply{18,19,18},Apply{18,12,18},RegisterType{1,18},Embed{0,18},RegisterType{12,18},Embed{1,18},Apply{13,1,19},Apply{18,19,18},Embed{2,19},Embed{0,20},Apply{19,20,19},Embed{3,20},Embed{0,21},Apply{20,21,20},Apply{20,12,20},Apply{19,20,19},Apply{13,1,20},Apply{19,20,19},Apply{18,19,18},RegisterType{2,18},Embed{1,18},Embed{0,19},Apply{18,19,18},Apply{18,5,18},RegisterType{3,18},Embed{3,18},Embed{0,19},Apply{18,19,18},Apply{18,12,18},RegisterType{4,18},Embed{3,18},Embed{0,19},Apply{18,19,18},Apply{18,7,18},RegisterType{5,18},Apply{13,1,18},RegisterType{6,18},Embed{0,18},RegisterType{7,18},Embed{1,18},Embed{3,19},Embed{0,20},Apply{19,20,19},Apply{19,12,19},Apply{18,19,18},Embed{2,19},Embed{0,20},Apply{19,20,19},Embed{0,20},Apply{19,20,19},Embed{3,20},Embed{0,21},Apply{20,21,20},Apply{20,12,20},Apply{19,20,19},Apply{18,19,18},RegisterType{8,18},Embed{3,18},Embed{0,19},Apply{18,19,18},Apply{18,12,18},RegisterType{9,18},Apply{13,9,18},RegisterType{11,18},Embed{0,18},Apply{10,18,18},RegisterType{10,18},Embed{4,18},Argument{0,19},Apply{18,19,18},Rule{.head=17,.args_captured=3,.steps={PullArgument{},PullArgument{},PullArgument{}},.replacement=Substitution{18}},Embed{0,18},Rule{.head=16,.args_captured=0,.steps={},.replacement=Substitution{18}},Argument{0,18},Apply{2,18,18},Apply{13,18,18},Rule{.head=15,.args_captured=1,.steps={PullArgument{}},.replacement=Substitution{18}},Embed{0,18},Rule{.head=14,.args_captured=1,.steps={PullArgument{}},.replacement=Substitution{18}},Embed{3,18},Embed{0,19},Apply{18,19,18},Argument{0,19},Apply{8,19,19},Apply{18,19,18},Rule{.head=13,.args_captured=1,.steps={PullArgument{}},.replacement=Substitution{18}},Embed{4,18},Argument{0,19},Apply{18,19,18},Apply{10,18,18},Rule{.head=0,.args_captured=2,.steps={PullArgument{},PullArgument{}},.replacement=Substitution{18}},Embed{1,18},Argument{0,19},Apply{18,19,18},Argument{0,19},Apply{6,19,19},Apply{18,19,18},Rule{.head=1,.args_captured=1,.steps={PullArgument{}},.replacement=Substitution{18}},Embed{1,18},Embed{0,19},Apply{18,19,18},Apply{18,14,18},Rule{.head=12,.args_captured=0,.steps={},.replacement=Substitution{18}},Embed{1,18},Argument{1,19},Apply{4,19,19},Apply{18,19,18},Argument{0,19},Argument{1,20},Apply{19,20,19},Apply{18,19,18},Rule{.head=2,.args_captured=2,.steps={PullArgument{},PullArgument{}},.replacement=Substitution{18}},Argument{0,18},Rule{.head=4,.args_captured=1,.steps={PullArgument{}},.replacement=Substitution{18}},Embed{1,18},Argument{0,19},Apply{18,19,18},Argument{0,19},Apply{11,19,19},Apply{18,19,18},Rule{.head=5,.args_captured=1,.steps={PullArgument{}},.replacement=Substitution{18}},Embed{0,18},Rule{.head=6,.args_captured=2,.steps={PullArgument{},PullArgument{}},.replacement=Substitution{18}},Embed{0,18},Apply{10,18,18},Rule{.head=7,.args_captured=0,.steps={},.replacement=Substitution{18}},Embed{1,18},Apply{18,16,18},Argument{0,19},Apply{18,19,18},Rule{.head=8,.args_captured=1,.steps={PullArgument{}},.replacement=Substitution{18}},Argument{0,18},Apply{10,18,18},Rule{.head=9,.args_captured=1,.steps={PullArgument{}},.replacement=Substitution{18}},Embed{1,18},Embed{4,19},Argument{0,20},Apply{19,20,19},Apply{18,19,18},Argument{0,19},Apply{17,19,19},Argument{1,20},Apply{19,20,19},Apply{18,19,18},Rule{.head=11,.args_captured=2,.steps={PullArgument{},PullArgument{}},.replacement=Substitution{18}},Embed{1,18},Argument{0,19},Apply{18,19,18},Embed{2,19},Embed{0,20},Apply{19,20,19},Embed{0,20},Apply{19,20,19},Argument{0,20},Apply{19,20,19},Apply{18,19,18},Rule{.head=10,.args_captured=1,.steps={PullArgument{}},.replacement=Substitution{18}},Export{3}}};
  }
  expr_module::ModuleResult execute(new_expression::Arena& arena, solver::BasicContext& context, new_expression::WeakExpression vector_type, expr_module::Core const& mod) {
    return execute(
      {
        .arena = arena,
        .rule_collector = context.rule_collector,
        .type_collector = context.type_collector
      },
      mod,
      {
        .value_imports = mdb::make_vector(
          arena.copy(context.primitives.type),
          arena.copy(context.primitives.arrow),
          arena.copy(context.primitives.constant),
          arena.copy(context.primitives.id),
          arena.copy(vector_type)
        )
      }
    );
  }
  new_expression::OwnedExpression execute_single(new_expression::Arena& arena, solver::BasicContext& context, new_expression::WeakExpression vector_type, expr_module::Core const& mod) {
    auto result = execute(arena, context, vector_type, mod);
    if(result.exports.size() != 1) std::terminate();
    return std::move(result.exports[0]);
  }
}
namespace pipeline::compile {
  /*
    Previously generated module
  */

  StandardCompilerContext::StandardCompilerContext(new_expression::Arena& arena)
    :arena(arena),
    context(arena),
    u64(primitive::U64Data::register_on(arena, arena.axiom())),
    str(primitive::StringData::register_on(arena, arena.axiom())),
    vec_type(arena.axiom()),
    empty_vec(execute_single(arena, context, vec_type, empty_vec_module())),
    cons_vec(execute_single(arena, context, vec_type, push_vec_module()))
  {
    auto add_name = [&](std::string name, new_expression::WeakExpression expr) {
      names_to_values.insert(std::make_pair(
        std::move(name),
        new_expression::TypedValue{
          .value = arena.copy(expr),
          .type = arena.copy(context.type_collector.get_type_of(expr))
        }
      ));
    };
    context.type_collector.type_of_primitive.set(vec_type, arena.apply(
      arena.copy(context.primitives.arrow),
      arena.copy(context.primitives.type),
      arena.apply(
        arena.copy(context.primitives.constant),
        arena.copy(context.primitives.type),
        arena.copy(context.primitives.type),
        arena.copy(context.primitives.type)
      )
    ));
    context.type_collector.type_of_primitive.set(u64->type, arena.copy(context.primitives.type));
    context.type_collector.type_of_primitive.set(str->type, arena.copy(context.primitives.type));
    add_name("Type", context.primitives.type);
    add_name("arrow", context.primitives.arrow);
    add_name("U64", u64->type);
    add_name("String", str->type);
    add_name("Vector", vec_type);
    add_name("empty_vec", empty_vec);
    add_name("cons_vec", cons_vec);
  }
  StandardCompilerContext::~StandardCompilerContext() {
    destroy_from_arena(arena, *this);
  }
  CombinedContext StandardCompilerContext::combined_context() {
    return CombinedContext{
      .names_to_values = names_to_values,
      .u64 = u64.get(),
      .str = str.get(),
      .empty_vec = new_expression::TypedValue{
        .value = arena.copy(empty_vec),
        .type = arena.copy(context.type_collector.get_type_of(empty_vec))
      },
      .cons_vec = new_expression::TypedValue{
        .value = arena.copy(cons_vec),
        .type = arena.copy(context.type_collector.get_type_of(cons_vec))
      },
      .context = context
    };
  }
  ModuleInfo StandardCompilerContext::create_module_primitives() {
    auto result = execute(
      {
        .arena = arena,
        .rule_collector = context.rule_collector,
        .type_collector = context.type_collector
      },
      module_primitives_module(),
      {
        .value_imports = mdb::make_vector(
          arena.copy(context.primitives.type),
          arena.copy(context.primitives.arrow),
          arena.copy(context.primitives.constant),
          arena.copy(context.primitives.id),
          arena.copy(vec_type),
          arena.copy(str->type)
        )
      }
    );

    if(result.exports.size() != 4) std::terminate();
    auto add_name = [&](std::string name, new_expression::WeakExpression expr) {
      names_to_values.insert(std::make_pair(
        std::move(name),
        new_expression::TypedValue{
          .value = arena.copy(expr),
          .type = arena.copy(context.type_collector.get_type_of(expr))
        }
      ));
    };
    add_name("Module", result.exports[0]);
    add_name("module", result.exports[1]);
    add_name("ModuleEntry", result.exports[2]);
    add_name("module_entry", result.exports[3]);
    return {
      .module_type = std::move(result.exports[0]),
      .module_ctor = std::move(result.exports[1]),
      .module_entry_type = std::move(result.exports[2]),
      .module_entry_ctor = std::move(result.exports[3])
    };
  }
}
