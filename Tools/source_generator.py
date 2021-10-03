import inflection
import os
from jinja2 import Environment, FileSystemLoader
import sys
import re
import itertools
import glob

jinja_env = Environment(
    loader=FileSystemLoader(os.path.dirname(os.path.abspath(__file__)) + '/SourceGeneratorTemplates')
);

commands_used = []

jinja_env.filters["camelize"] = inflection.camelize
jinja_env.filters["underscore"] = inflection.underscore
jinja_env.tests["empty"] = lambda t: len(t) == 0
jinja_env.tests["nonempty"] = lambda t: len(t) > 0

def passthrough_call(*, caller):
    return caller()
def move_call(*, caller):
    return "std::move(" + caller() + ")"

reference_kinds = {
    "value": {"suffix": "", "const_qualifier": "", "forward": move_call, "forward_as_this": passthrough_call, "name": "value", "forward_suffix": "&&"},
    "cvalue": {"suffix": " const", "const_qualifier": " const", "forward": move_call, "forward_as_this": passthrough_call, "name": "cvalue", "forward_suffix": "const&&"},
    "cref": {"suffix": " const&", "const_qualifier": " const", "forward": passthrough_call, "forward_as_this": passthrough_call, "name": "cref", "forward_suffix": ""},
    "ref": {"suffix": "&", "const_qualifier": "", "forward": passthrough_call, "forward_as_this": passthrough_call, "name": "ref", "forward_suffix": ""},
    "rref": {"suffix": "&&", "const_qualifier": "", "forward": move_call, "forward_as_this": move_call, "name": "rref", "forward_suffix": ""},
    "crref": {"suffix": " const&&", "const_qualifier": "", "forward": move_call, "forward_as_this": move_call, "name": "crref", "forward_suffix": ""}
}

def reference_kind_of(type_name):
    if type_name[-5:] == "const&":
        return reference_kinds["cref"]
    elif type_name[-6:] == "const&&":
        return reference_kinds["crref"]
    elif type_name[-1:] == "&":
        return reference_kinds["ref"]
    elif type_name[-2:] == "&&":
        return reference_kinds["rref"]
    else:
        return reference_kinds["value"]

jinja_env.globals["reference"] = reference_kinds
jinja_env.globals["reference_for"] = reference_kind_of

command_string = "!!!FILE_GENERATOR_COMMAND_"

def fix_multispaces(str):
    pattern = re.compile(r"\n(\s*\n)+");
    return pattern.sub(r"\n", str)

class TemplateCommandSegment:
    def __init__(self):
        self.content = ""
        self.last_namespace = ""
        self.absolute_includes = set()
        self.relative_includes = set()
        self.source_includes = set()
    def add_include(self, include, kind):
        if kind == "absolute":
            self.absolute_includes.add(include)
        elif kind == "relative":
            self.relative_includes.add(include)
        else:
            self.source_includes.add(include)
    def open_namespace(self, namespace):
        if namespace != "":
            self.content = self.content + "\nnamespace " + namespace + "{\n"
    def close_namespace(self):
        if self.last_namespace != "":
            self.content = self.content + "\n}\n"
    def ensure_namespace(self, namespace):
        if namespace != self.last_namespace:
            self.close_namespace()
            self.open_namespace(namespace)
            self.last_namespace = namespace
    def write(self, content, namespace):
        if re.fullmatch(r"\s*", content) == None: # If we're not just adding whitespace
            self.ensure_namespace(namespace)
        self.content += content
    def write_includes(self, filebase, extension):
        my_path = os.path.dirname(filebase + "." + extension)
        absolute_includes = "".join(map(lambda include : "#include <" + include + ">\n", self.absolute_includes));
        relative_includes = "".join(map(lambda include : "#include \"" + include + "\"\n", self.relative_includes));
        source_includes = "".join(map(lambda include : "#include \"" + os.path.relpath(include, my_path) + "\"\n", self.source_includes));
        return absolute_includes + source_includes + relative_includes
    def take_content(self, filebase, extension, context):
        if extension == "cpp" or extension == "inl":
            self.source_includes.add(filebase + ".hpp")
        if extension == "hpp" and "inl" in context.output_pieces:
            self.write("\n#include \"" + os.path.basename(filebase) + ".inl\"\n", "")
        if extension == "hpp" and "proto.hpp" in context.output_pieces:
            self.source_includes.add(filebase + ".proto.hpp")

        ifndefguard = ""
        ifndefguardend = ""
        if extension != "cpp":
            var = "FILE_" + inflection.underscore(filebase.replace("/","_").replace("\\","_").replace(" ","_")).upper() + "_" + inflection.underscore(extension).replace(".","_").upper()
            ifndefguard = "#ifndef " + var + "\n#define " + var + "\n"
            ifndefguardend = "#endif"

        self.ensure_namespace("")
        return fix_multispaces(ifndefguard + self.write_includes(filebase, extension) + self.content + ifndefguardend)

class TemplateCommandContext:
    def __init__(self):
        self.output_pieces = {}
        self.extension_stack = ["hpp"]
        self.namespace_stack = [""]
        self.position = 0
    def get_piece(self, piece):
        return self.output_pieces.setdefault(piece, TemplateCommandSegment())
    def write_to_piece(self, piece, content):
        self.get_piece(piece).write(content, self.namespace_stack[-1])
    def write(self, content):
        self.write_to_piece(self.extension_stack[-1], content)

class FileCommand:
    def __init__(self, extension):
        self.extension = extension
    def act(self, context):
        context.extension_stack.append(self.extension)
    def unact(self, context):
        context.extension_stack.pop()
class AbsoluteNamespaceCommand:
    def __init__(self, namespace):
        self.namespace = namespace
    def act(self, context):
        context.namespace_stack.append(self.namespace)
    def unact(self, context):
        context.namespace_stack.pop()
class RelativeNamespaceCommand:
    def __init__(self, namespace):
        self.namespace = namespace
    def act(self, context):
        context.namespace_stack.append(context.namespace_stack[-1] + "::" + self.namespace)
    def unact(self, context):
        context.namespace_stack.pop()
class IncludeCommand:
    def __init__(self, include, kind):
        self.include = include
        self.kind = kind
    def act(self, context):
        context.get_piece(context.extension_stack[-1]).add_include(self.include, self.kind)

def add_command(command):
    index = len(commands_used)
    commands_used.append(command)
    return command_string + str(index) + "!!!"
def add_command_block(command):
    index = len(commands_used)
    commands_used.append(command)
    return (command_string + str(index) + "!!!", command_string + "END_" + str(index) + "!!!")
def in_extension_command(extension, *, caller):
    (start_str, end_str) = add_command_block(FileCommand(extension))
    return start_str + caller() + end_str
def in_namespace_command(namespace, *, caller):
    (start_str, end_str) = add_command_block(AbsoluteNamespaceCommand(namespace))
    return start_str + caller() + end_str
def in_relative_namespace_command(namespace, *, caller):
    (start_str, end_str) = add_command_block(RelativeNamespaceCommand(namespace))
    return start_str + caller() + end_str
def absolute_include_command(include):
    return add_command(IncludeCommand(include, "absolute"))
def relative_include_command(include):
    return add_command(IncludeCommand(include, "relative"))
def source_include_command(include):
    return add_command(IncludeCommand(include, "source"))

jinja_env.globals["in_extension"] = in_extension_command
jinja_env.globals["in_namespace"] = in_namespace_command
jinja_env.globals["in_namespace_relative"] = in_relative_namespace_command
jinja_env.globals["absolute_include"] = absolute_include_command
jinja_env.globals["relative_include"] = relative_include_command
jinja_env.globals["source_include"] = source_include_command

def parse_template_output(output, filebase):
    context = TemplateCommandContext()
    while True:
        next_instance = output.find(command_string, context.position)
        context.write(output[context.position:next_instance])
        if next_instance == -1:
            break
        command_pos = next_instance + len(command_string)
        if output[command_pos:command_pos + 4] == "END_":
            command_pos = command_pos + 4
            command_end = output.find("!!!", command_pos)
            command_index = int(output[command_pos:command_end])
            commands_used[command_index].unact(context)
            context.position = command_end + 3
        else:
            command_end = output.find("!!!", command_pos)
            command_index = int(output[command_pos:command_end])
            commands_used[command_index].act(context)
            context.position = command_end + 3
    return { extension:piece.take_content(filebase, extension, context) for (extension, piece) in context.output_pieces.items()}

tree_template = jinja_env.get_template('tree.hpp.template')
type_erasure_template = jinja_env.get_template('type_erasure.hpp.template')
async_generic_template = jinja_env.get_template('async_generic.hpp.template')

# Utilities for writing trees
class TypeInfo:
    def __init__(self, data):
        self.data = data
    def is_reference(self):
        if isinstance(self.data, str):
            return self.data[-1] == "&";
        else:
            return False;
    def is_vector(self):
        if isinstance(self.data, str):
            return False;
        else:
            return self.data["kind"] == "vector"
    def is_optional(self):
        if isinstance(self.data, str):
            return False;
        else:
            return self.data["kind"] == "optional"
    def base_kind(self):
        if isinstance(self.data, str):
            return self.data;
        else:
            return self.data["base"]
    def wrapper_type(self):
        if isinstance(self.data, str):
            return "simple"
        else:
            return self.data["kind"]

    def archive_type(self, const_qualifier):
        if isinstance(self.data, str):
            return self.data + const_qualifier
        else:
            if self.data["kind"] == "optional":
                return "Optional" + self.data["base"] + const_qualifier
            elif self.data["kind"] == "vector":
                return "std::span<" + self.data["base"] + const_qualifier + ">" + const_qualifier
            else:
                raise RuntimeError("Unrecognized kind: " + t.kind)


def get_type_name(t):
    if isinstance(t, str):
        return t
    else:
        if t["kind"] == "optional":
            return "std::optional<" + t["base"] + ">"
        elif t["kind"] == "vector":
            return "std::vector<" + t["base"] + ">"
        else:
            raise RuntimeError("Unrecognized kind: " + t.kind)

def generate_kind_component(component_name, kind_component, data_component, index, global_index, kind_index):
    base_members = [
        {
            "name": kind_member_name,
            "type": get_type_name(kind_member_type),
            "type_info": TypeInfo(kind_member_type),
            "base_member": True
        }
        for (kind_member_name, kind_member_type) in kind_component
    ]
    extra_members = [
        {
            "name": extra_member_name,
            "type": get_type_name(extra_member_type),
            "type_info": TypeInfo(extra_member_type),
            "base_member": False
        }
        for (extra_member_name, extra_member_type) in data_component
    ]
    members = base_members + extra_members
    return {
        "name": component_name,
        "base_members": base_members,
        "extra_members": extra_members,
        "members": members,
        "index": index,
        "global_index": global_index,
        "kind_index": kind_index
    }

def generate_kind(kind_name, kind, data, global_index, kind_index, kind_uses):
    return {
        "name": kind_name,
        "components": [
            generate_kind_component(component_name, kind[component_name], data[component_name], count, count + global_index, kind_index)
            for (count, component_name) in enumerate(data)
        ],
        "kind_index": kind_index,
        "users": kind_uses
    }
def find_used_kinds(shape):
    ret = {}
    for (kind_name, kind) in shape.inner_kinds.items():
        ret.setdefault(kind_name, {})
        for (component_name, component) in kind.items():
            for (_, member) in component:
                t = TypeInfo(member)
                seen = ret.setdefault(t.base_kind(), {})
                seen.setdefault(t.wrapper_type(), set()).add(component_name)
    return ret

def generate_kinds(shape, data):
    kind_uses = find_used_kinds(shape)
    return [
        generate_kind(kind_name, shape.inner_kinds[kind_name], data[kind_name], global_index, kind_index, kind_uses[kind_name])
        for (kind_index, (kind_name, global_index)) in enumerate(zip(
            data,
            itertools.accumulate(map(lambda k : len(shape.inner_kinds[k]), data), initial = 0)
        ))
    ]

def empty_data_for_shape(kinds):
    return {
        kind_name: {
            component: []
            for component in components
        }
        for (kind_name, components) in kinds.items()
    }

class CompoundShape:
    def __init__(self, kinds):
        self.inner_kinds = kinds
        self.kinds = generate_kinds(self, empty_data_for_shape(self.inner_kinds))
        self.components = [component for kind in self.kinds for component in kind["components"]]
        self.multikind = len(self.kinds) > 1
    def generate_instance(self, *, namespace, data):
        return ShapeInstance(self, namespace, data)

class ShapeInstance:
    def __init__(self, shape, namespace, data):
        self.shape = shape
        self.namespace = namespace
        self.data = data
        self.kinds = generate_kinds(self.shape, self.data)
        self.components = [component for kind in self.kinds for component in kind["components"]]
        self.multikind = len(self.kinds) > 1

def multitree_merge_components(component_list):
    base_members = component_list[0]["base_members"]
    extra_members = [component["extra_members"] for component in component_list]
    members = [base_members + component["extra_members"] for component in component_list]
    return {
        "name": component_list[0]["name"],
        "base_members": base_members,
        "extra_members": extra_members,
        "members": members,
        "index": component_list[0]["index"],
        "global_index": component_list[0]["global_index"],
        "kind_index": component_list[0]["kind_index"]
    }
def multitree_merge_kinds(kind_list):
    components = [
        multitree_merge_components([kind["components"][index] for kind in kind_list])
        for (index, _) in enumerate(kind_list[0]["components"])
    ]
    return {
        "name": kind_list[0]["name"],
        "components": components
    }
def multitree_merge_shapes(shape_list):
    return [
        multitree_merge_kinds([shape.kinds[index] for shape in shape_list])
        for (index, _) in enumerate(shape_list[0].kinds)
    ]

class Multitree:
    def __init__(self, namespace, tree_dict):
        self.namespace = namespace
        self.trees = [{
            "namespace": tree.namespace,
            "member_name": member_name,
            "index": index
        } for (index, (member_name, tree)) in enumerate(tree_dict.items())]
        self.kinds = multitree_merge_shapes([tree for (_, tree) in tree_dict.items()])
        self.components = [component for kind in self.kinds for component in kind["components"]]
        self.multikind = len(self.kinds) > 1

class TreeOutput:
    def __init__(self, *, trees, archive_namespace = None, multitrees = None, shared = False):
        self.trees = trees
        self.shape = self.trees[0].shape
        for tree in self.trees:
            if tree.shape != self.shape:
                raise RuntimeError("Trees in output must have same shape!")
        if archive_namespace:
            self.archive = {
                "namespace": archive_namespace,
                "trees": trees,
                "shape": self.shape
            }
        else:
            self.archive = None
        self.multitrees = multitrees
        self.shared = shared
    def write_string(self, file_info):
        return tree_template.render({
            "trees": self.trees,
            "archive": self.archive,
            "shape": self.shape,
            "multitrees": self.multitrees,
            "shared": self.shared,
            "file_info": file_info
        })

# Type erasure utilities
class MemberFunction:
    def __init__(self, name, *, ret = None, args = None, ref = None):
        if ret == None:
            ret = "void"
        if args == None:
            args = []
        if ref == None:
            ref = "value"
        self.args = [{
            "name": name,
            "type": type
        } for (type, name) in args]
        self.name = name
        self.return_type = ret
        self.this_ref = reference_kinds[ref]
class TypeErasedKind:
    def __init__(self, namespace, name, functions):
        self.namespace = namespace
        self.name = name
        self.functions = functions
    def write_string(self, file_info):
        return type_erasure_template.render({
            "type_erase_info": self,
            "file_info": file_info
        })
# Async generic utilities
class AsyncGeneric:
    def __init__(self, namespace, interpreter_name, routine_name, primitives):
        self.namespace = namespace
        self.interpreter_name = interpreter_name
        self.routine_name = routine_name
        self.primitives = primitives
    def write_string(self, file_info):
        return async_generic_template.render({
            "async_info": self,
            "file_info": file_info
        })

# File writing utilities
class FileWriter:
    def __init__(self, spec_file):
        self.outputs = []
        self.file_info = {
            "spec_file": spec_file,
            "generator_file": __file__
        }
    def write(self, *args, absolute_includes = None, relative_includes = None, source_includes = None):
        if absolute_includes:
            for include in absolute_includes:
                self.outputs.append(absolute_include_command(include))
        if relative_includes:
            for include in relative_includes:
                self.outputs.append(relative_include_command(include))
        if source_includes:
            for include in source_includes:
                self.outputs.append(source_include_command(include))
        for arg in args:
            self.outputs.append(arg.write_string(self.file_info))

class FileContext:
    def __init__(self, path_to_file):
        def optional(x):
            return {"kind": "optional", "base": x}
        def vector(x):
            return {"kind": "vector", "base": x}
        def get_output(filename):
            if filename[:4] == "THIS":
                filename = os.path.splitext(path_to_file)[0] + filename[4:]
            if filename in self.outputs:
                return self.outputs[filename];
            else:
                self.outputs[filename] = FileWriter(path_to_file)
                return self.outputs[filename]
        self.outputs = {}
        self.context = {
            "get_output": get_output,
            "optional": optional,
            "vector": vector,
            "CompoundShape": CompoundShape,
            "Multitree": Multitree,
            "TreeOutput": TreeOutput,
            "TypeErasedKind": TypeErasedKind,
            "MemberFunction": MemberFunction,
            "AsyncGeneric": AsyncGeneric
        }

def process_file(path_to_file):
    context = FileContext(path_to_file)
    files_written = []
    with open(path_to_file) as file:
        exec(file.read(), context.context, {})
        for (filename, writer) in context.outputs.items():
            outputs = parse_template_output("\n".join(writer.outputs), filename)
            for (extension, content) in outputs.items():
                true_filename = filename + "." + extension
                if os.path.exists(true_filename):
                    with open(true_filename) as file:
                        body = file.read()
                        if content == body:
                            print(true_filename + " is up to date. No changes made.")
                            files_written.append(true_filename)
                            continue
                with open(true_filename, "w") as file:
                    print("Writing: " + true_filename)
                    file.write(content)
                    files_written.append(true_filename)
    return files_written


if __name__ == "__main__":
    if len(sys.argv) == 1:
        raise RuntimeError("No files supplied to source generator.")

    for argument in sys.argv[1:]:
        print("Processing file: " + argument)
        process_file(argument)
