#include "standard_compiler_context.hpp"
#include "../Module/execute.hpp"
#include "../Utility/vector_utility.hpp"

namespace {
  expr_module::Core module_primitives_module() {
    using namespace expr_module::step;
    using namespace expr_module::rule_step;
    using namespace expr_module::rule_replacement;
    return {.value_import_size=6,.data_type_import_size=0,.c_replacement_import_size=0,.register_count=38,.output_size=4,.steps={Declare{0},Declare{1},Declare{2},Declare{3},Declare{4},Declare{5},Declare{6},Declare{7},Declare{8},Declare{9},Declare{10},Declare{11},Declare{12},Declare{13},Declare{14},Declare{15},Axiom{16},Declare{17},Declare{18},Axiom{19},Declare{20},Declare{21},Declare{22},Axiom{23},Axiom{24},Declare{25},Declare{26},Declare{27},Declare{28},Declare{29},Declare{30},Declare{31},Declare{32},Declare{33},Embed{1,34},RegisterType{33,34},Embed{0,34},Embed{1,35},Apply{34,35,34},Apply{34,9,34},RegisterType{32,34},Embed{0,34},Embed{1,35},Apply{34,35,34},Apply{34,33,34},RegisterType{31,34},Embed{1,34},RegisterType{30,34},Embed{0,34},Embed{1,35},Apply{34,35,34},Apply{34,9,34},RegisterType{10,34},Embed{1,34},RegisterType{9,34},Embed{1,34},RegisterType{11,34},Embed{0,34},Embed{1,35},Apply{34,35,34},Apply{34,9,34},Apply{21,34,34},RegisterType{12,34},Apply{12,10,34},Apply{21,34,34},RegisterType{13,34},Embed{1,34},RegisterType{14,34},Apply{12,10,34},RegisterType{8,34},Embed{1,34},RegisterType{7,34},Embed{0,34},Embed{1,35},Apply{34,35,34},Apply{34,2,34},RegisterType{0,34},Embed{2,34},Apply{12,10,35},Apply{34,35,34},Embed{3,35},Embed{1,36},Apply{35,36,35},Embed{0,36},Embed{1,37},Apply{36,37,36},Apply{36,9,36},Apply{35,36,35},Apply{12,10,36},Apply{35,36,35},Apply{34,35,34},RegisterType{6,34},Embed{2,34},Embed{0,35},Embed{1,36},Apply{35,36,35},Apply{35,9,35},Apply{34,35,34},Embed{3,35},Embed{1,36},Apply{35,36,35},Embed{1,36},Apply{35,36,35},Embed{0,36},Embed{1,37},Apply{36,37,36},Apply{36,9,36},Apply{35,36,35},Apply{34,35,34},RegisterType{5,34},Apply{28,0,34},RegisterType{4,34},Embed{0,34},Embed{1,35},Apply{34,35,34},Apply{34,9,34},RegisterType{3,34},Embed{1,34},RegisterType{2,34},Embed{0,34},Embed{1,35},Apply{34,35,34},Apply{34,2,34},RegisterType{1,34},Apply{12,25,34},RegisterType{15,34},Embed{0,34},Embed{1,35},Apply{34,35,34},Apply{34,14,34},RegisterType{16,34},Embed{1,34},RegisterType{17,34},Embed{0,34},Embed{1,35},Apply{34,35,34},Apply{34,17,34},RegisterType{18,34},Embed{0,34},Embed{1,35},Apply{34,35,34},Apply{34,7,34},RegisterType{19,34},Embed{1,34},RegisterType{20,34},Embed{1,34},Apply{21,34,34},RegisterType{21,34},Embed{1,34},RegisterType{29,34},Embed{0,34},Embed{1,35},Apply{34,35,34},Apply{34,2,34},Apply{21,34,34},RegisterType{28,34},Embed{0,34},Embed{1,35},Apply{34,35,34},Apply{34,29,34},RegisterType{23,34},Embed{2,34},Embed{0,35},Embed{1,36},Apply{35,36,35},Apply{35,2,35},Apply{34,35,34},Embed{3,35},Embed{1,36},Apply{35,36,35},Embed{1,36},Apply{35,36,35},Embed{0,36},Embed{1,37},Apply{36,37,36},Apply{36,2,36},Apply{35,36,35},Apply{34,35,34},RegisterType{22,34},Embed{0,34},Embed{1,35},Apply{34,35,34},Apply{34,20,34},RegisterType{24,34},Embed{0,34},Embed{1,35},Apply{34,35,34},Apply{34,9,34},RegisterType{25,34},Apply{13,27,34},RegisterType{26,34},Apply{12,10,34},RegisterType{27,34},Embed{4,34},Apply{21,34,34},Rule{.head=33,.args_captured=0,.steps={},.replacement=Substitution{34}},Embed{1,34},Rule{.head=32,.args_captured=1,.steps={PullArgument{}},.replacement=Substitution{34}},Embed{2,34},Embed{5,35},Apply{35,24,35},Apply{34,35,34},Argument{0,35},Apply{4,35,35},Apply{34,35,34},Rule{.head=31,.args_captured=1,.steps={PullArgument{}},.replacement=Substitution{34}},Embed{1,34},Rule{.head=30,.args_captured=0,.steps={},.replacement=Substitution{34}},Embed{2,34},Embed{4,35},Apply{34,35,34},Argument{0,35},Apply{8,35,35},Apply{34,35,34},Rule{.head=10,.args_captured=1,.steps={PullArgument{}},.replacement=Substitution{34}},Embed{2,34},Embed{1,35},Apply{34,35,34},Apply{34,32,34},Rule{.head=9,.args_captured=0,.steps={},.replacement=Substitution{34}},Embed{4,34},Rule{.head=11,.args_captured=0,.steps={},.replacement=Substitution{34}},Embed{0,34},Embed{1,35},Apply{34,35,34},Argument{0,35},Apply{5,35,35},Apply{34,35,34},Rule{.head=12,.args_captured=1,.steps={PullArgument{}},.replacement=Substitution{34}},Argument{0,34},Apply{6,34,34},Apply{12,34,34},Rule{.head=13,.args_captured=1,.steps={PullArgument{}},.replacement=Substitution{34}},Embed{1,34},Rule{.head=14,.args_captured=0,.steps={},.replacement=Substitution{34}},Embed{1,34},Rule{.head=8,.args_captured=2,.steps={PullArgument{},PullArgument{}},.replacement=Substitution{34}},Embed{2,34},Embed{1,35},Apply{34,35,34},Apply{34,18,34},Rule{.head=7,.args_captured=0,.steps={},.replacement=Substitution{34}},Embed{5,34},Apply{34,24,34},Apply{21,34,34},Rule{.head=0,.args_captured=1,.steps={PullArgument{}},.replacement=Substitution{34}},Embed{2,34},Argument{1,35},Apply{3,35,35},Apply{34,35,34},Argument{0,35},Argument{1,36},Apply{35,36,35},Apply{34,35,34},Rule{.head=6,.args_captured=2,.steps={PullArgument{},PullArgument{}},.replacement=Substitution{34}},Embed{2,34},Apply{34,30,34},Argument{0,35},Apply{34,35,34},Rule{.head=5,.args_captured=1,.steps={PullArgument{}},.replacement=Substitution{34}},Rule{.head=4,.args_captured=2,.steps={PullArgument{},PullArgument{}},.replacement=Substitution{16}},Embed{4,34},Rule{.head=3,.args_captured=1,.steps={PullArgument{}},.replacement=Substitution{34}},Embed{2,34},Embed{4,35},Apply{34,35,34},Apply{34,1,34},Rule{.head=2,.args_captured=0,.steps={},.replacement=Substitution{34}},Embed{1,34},Rule{.head=1,.args_captured=1,.steps={PullArgument{}},.replacement=Substitution{34}},Embed{2,34},Argument{0,35},Apply{34,35,34},Argument{0,35},Apply{26,35,35},Argument{1,36},Apply{35,36,35},Apply{34,35,34},Rule{.head=15,.args_captured=2,.steps={PullArgument{},PullArgument{}},.replacement=Substitution{34}},Embed{1,34},Apply{21,34,34},Rule{.head=17,.args_captured=0,.steps={},.replacement=Substitution{34}},Embed{2,34},Embed{4,35},Apply{34,35,34},Argument{0,35},Apply{15,35,35},Apply{34,35,34},Rule{.head=18,.args_captured=1,.steps={PullArgument{}},.replacement=Substitution{34}},Embed{1,34},Rule{.head=20,.args_captured=0,.steps={},.replacement=Substitution{34}},Embed{2,34},Argument{0,35},Apply{34,35,34},Embed{3,35},Embed{1,36},Apply{35,36,35},Embed{1,36},Apply{35,36,35},Argument{0,36},Apply{35,36,35},Apply{34,35,34},Rule{.head=21,.args_captured=1,.steps={PullArgument{}},.replacement=Substitution{34}},Embed{2,34},Embed{4,35},Apply{34,35,34},Apply{34,31,34},Rule{.head=29,.args_captured=0,.steps={},.replacement=Substitution{34}},Embed{0,34},Embed{1,35},Apply{34,35,34},Argument{0,35},Apply{22,35,35},Apply{34,35,34},Rule{.head=28,.args_captured=1,.steps={PullArgument{}},.replacement=Substitution{34}},Embed{2,34},Apply{34,11,34},Argument{0,35},Apply{34,35,34},Rule{.head=22,.args_captured=1,.steps={PullArgument{}},.replacement=Substitution{34}},Embed{4,34},Apply{21,34,34},Rule{.head=25,.args_captured=1,.steps={PullArgument{}},.replacement=Substitution{34}},Rule{.head=26,.args_captured=3,.steps={PullArgument{},PullArgument{},PullArgument{}},.replacement=Substitution{24}},Argument{0,34},Apply{21,34,34},Rule{.head=27,.args_captured=2,.steps={PullArgument{},PullArgument{}},.replacement=Substitution{34}},Export{24},Export{19},Export{16},Export{23}}};
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
          arena.copy(context.primitives.id),
          arena.copy(context.primitives.type),
          arena.copy(context.primitives.arrow),
          arena.copy(context.primitives.constant),
          arena.copy(str->type),
          arena.copy(vec_type)
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
    add_name("ModuleEntry", result.exports[0]);
    add_name("module_entry", result.exports[1]);

    add_name("Module", result.exports[2]);
    add_name("full_module", result.exports[3]);
    return {
      .module_type = std::move(result.exports[2]),
      .full_module_ctor = std::move(result.exports[3]),
      .module_entry_type = std::move(result.exports[0]),
      .module_entry_ctor = std::move(result.exports[1])
    };
  }
}
