import inflection
import os
from jinja2 import Environment, FileSystemLoader
import sys

def read_specification(path_to_file):
    imported = {}
    with open(path_to_file) as file:
        exec(file.read(), {}, imported)
        return imported['specification']

def get_name_and_namespace(str):
    separator = str.rfind("::")
    if separator == -1:
        return (str, None)
    else:
        return (str[separator+2:], str[:separator])

def get_namespace_locator(str):
    separator = str.rfind("::")
    if separator == -1:
        return ""
    else:
        return str[:separator+2]

def datatype_for_base(str, tree_name):
    if str == 'Self':
        return tree_name
    elif str == 'OptionalSelf':
        return 'std::optional<' + tree_name + '>'
    else:
        raise RuntimeError("Unrecognized base kind: " + str)

def process_path(shape, path_name):
    (class_name, namespace) = get_name_and_namespace(path_name)
    return {
        "base_namespace" : namespace,
        "class_name": class_name
    }

def process_tree(spec, tree_name):
    shapes = spec['shapes']
    tree = spec['trees'][tree_name]
    shape = shapes[tree['shape']]
    data = tree['data']

    (class_name, namespace) = get_name_and_namespace(tree_name)

    components = []
    for component_name, base_data in shape['components'].items():
        base_members = [{
            "name": name,
            "kind": base_type,
            "type": datatype_for_base(base_type, class_name),
        } for (name, base_type) in base_data]
        extra_members = [{"name": name, "type": type} for (name, type) in data[component_name]]
        base_member_types = list(set([datatype_for_base(base_type, class_name) for (name, base_type) in base_data]))
        components.append({
            "name": component_name,
            "members": base_members + extra_members,
            "base_members": base_members,
            "extra_members": extra_members,
            "base_member_types": base_member_types
        })

    return {
        "base_namespace": namespace,
        "components": components,
        "class_name": class_name,
        "path_class": tree.get('path')
    }

def process_multitree(spec, multitree_name):
    shapes = spec['shapes']
    multitree = spec['multitrees'][multitree_name]
    trees = [(tree_name, spec['trees'][tree_name]) for (tree_name, member_name) in multitree]
    example_tree = trees[0][1]
    shape = shapes[example_tree['shape']]

    (class_name, namespace) = get_name_and_namespace(multitree_name)

    components = []
    for component_name, base_data in shape['components'].items():
        base_members = [{
            "name": name,
            "kind": base_type,
            "type": datatype_for_base(base_type, class_name),
        } for (name, base_type) in base_data]
        extra_members = {}
        for (tree_name, tree) in trees:
            extra_members[tree_name] = [{"name": name, "type": type} for (name, type) in tree['data'][component_name]]

        components.append({
            "name": component_name,
            "base_members": base_members,
            "extra_members": extra_members
        })

    return {
        "base_namespace": namespace,
        "class_name": class_name,
        "components": components,
        "trees": [{"type": tree_name, "member": member_name, "namespace": get_namespace_locator(tree_name)} for (tree_name, member_name) in multitree],
    }


jinja_env = Environment(
    loader=FileSystemLoader(os.path.dirname(os.path.abspath(__file__)) + '/InductiveDataTemplate')
);

jinja_env.filters["camelize"] = inflection.camelize
jinja_env.filters["underscore"] = inflection.underscore
jinja_env.tests["empty"] = lambda t: len(t) == 0
jinja_env.tests["nonempty"] = lambda t: len(t) > 0

inductive_template = jinja_env.get_template('tree.hpp.template')
path_template = jinja_env.get_template('path.hpp.template')
head_template = jinja_env.get_template('head.hpp.template')
multi_template = jinja_env.get_template('multitree.hpp.template')

def process_file(specification, filename):
    ret = ""
    ret = ret + head_template.render() + "\n"

    for include_name in specification.get('includes', []):
        if include_name[0] == '<':
            ret = ret + "#include " + include_name + "\n";
        else:
            ret = ret + "#include \"" + include_name + "\"\n";

    for class_name in specification['files'][filename]:
        path = specification['paths'].get(class_name)
        if path:
            render_data = process_path(specification['shapes'][path], class_name)
            ret = ret + path_template.render(render_data) + "\n"
            continue
        tree = specification['trees'].get(class_name)
        if tree:
            render_data = process_tree(specification, class_name)
            ret = ret + inductive_template.render(render_data) + "\n"
            continue
        multitree = specification['multitrees'].get(class_name)
        if multitree:
            render_data = process_multitree(specification, class_name)
            ret = ret + multi_template.render(render_data) + "\n"
            continue
        raise RuntimeError("No class named " + class_name)
    return ret

def process_all(path_to_file):
    specification = read_specification(path_to_file)
    for filename in specification['files']:
        output = process_file(specification, filename)
        true_filename = os.path.dirname(path_to_file) + "/" + filename
        if filename[:4] == "this":
            true_filename = os.path.splitext(path_to_file)[0] + filename[4:]
        with open(true_filename, "w") as file:
            file.write(output)

if len(sys.argv) == 1:
    raise RuntimeError("No files supplied to source generator.")

for argument in sys.argv[1:]:
    process_all(argument)
