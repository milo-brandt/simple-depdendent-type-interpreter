import os;
from glob import glob;
from jinja2 import Environment, FileSystemLoader
from functools import lru_cache
import re
import source_generator

def remove_c_comments(text): # from https://stackoverflow.com/questions/241327/remove-c-and-c-comments-using-python
    def replacer(match):
        s = match.group(0)
        if s.startswith('/'):
            return " " # note: a space and not an empty string
        else:
            return s
    pattern = re.compile(
        r'//.*?$|/\*.*?\*/|\'(?:\\.|[^\\\'])*\'|"(?:\\.|[^\\"])*"',
        re.DOTALL | re.MULTILINE
    )
    return re.sub(pattern, replacer, text)

@lru_cache
def get_direct_includes_of(filename):
    if not os.path.isfile(filename):
        raise RuntimeError("No file at " + filename)
    with open(filename) as file:
        source = file.read()
        stripped_source = remove_c_comments(source)
        include_iter = re.finditer("#include \"([^\"]+)\"", stripped_source)
        return [os.path.normpath(os.path.join(os.path.dirname(filename), m.group(1))) for m in include_iter]

@lru_cache
def get_direct_associates_of(filename): # Includes, but also .hpp files are associated to .cpp ones
    (root, ext) = os.path.splitext(filename)
    if ext == ".hpp" and os.path.exists(root + ".cpp"):
        ret = get_direct_includes_of(filename).copy()
        ret.append(root + ".cpp")
        return ret
    else:
        return get_direct_includes_of(filename)

@lru_cache
def get_nested_includes_of(filename):
    to_check = get_direct_includes_of(filename).copy()
    to_check.append(filename)
    ret = {i for i in to_check}
    while len(to_check) > 0:
        target_file = to_check.pop()
        target_includes = get_direct_includes_of(target_file)
        for include in target_includes:
            if not include in ret:
                to_check.append(include)
                ret.add(include)
    return ret

@lru_cache
def get_nested_associates_of(filename):
    to_check = get_direct_associates_of(filename).copy()
    to_check.append(filename)
    ret = {i for i in to_check}
    while len(to_check) > 0:
        target_file = to_check.pop()
        target_includes = get_direct_associates_of(target_file)
        for include in target_includes:
            if not include in ret:
                to_check.append(include)
                ret.add(include)
    return ret

def objects_to_compile(filename, object_folder):
    associates = get_nested_associates_of(filename)
    associate_cpp_roots = [
        os.path.relpath(os.path.splitext(file)[0], "Source")
        for file in associates if os.path.splitext(file)[1] == '.cpp'
    ]
    return [{
        "file": object_folder + "/" + root + ".o",
        "source": "Source/" + root + ".cpp",
        "sources_needed": get_nested_includes_of("Source/" + root + ".cpp")
    } for root in associate_cpp_roots]

def generate_python_info():
    def is_source_generator(name):
        try:
            with open(name) as file:
                line = file.readline()[:-1]
                return line == "# Source Generator"
        except:
            return False
    python_files = [file for folder in os.walk("Source") for file in glob(os.path.join(folder[0],"*.py"))]
    source_generators = [file for file in python_files if is_source_generator(file)]
    output_data = []
    for file in source_generators:
        outputs = source_generator.process_file(file)
        output_data.append({
            "source": file,
            "outputs": outputs
        })
    return output_data

source_generators = generate_python_info()

os.chdir(os.path.dirname(os.path.abspath(__file__)) + "/..")
jinja_env = Environment(
    loader=FileSystemLoader('Tools')
);
makefile = jinja_env.get_template('makefile_template').render(
    targets = [
        {
            "program": "EmscriptenBuild/index.js",
            "objects": objects_to_compile("Source/main.cpp", "EmscriptenBuild"),
            "compiler": "$(emcc_compiler)",
            "compile_options": "-std=c++20 -DCOMPILE_FOR_EMSCRIPTEN -O3",
            "link_options": "-std=c++20 --bind -s WASM=1 -O3 -s ALLOW_MEMORY_GROWTH=1"
        },
        {
            "program": "Debug/program",
            "objects": objects_to_compile("Source/main.cpp", "Debug"),
            "compiler": "$(compiler)",
            "compile_options": "-std=c++20 -ggdb -O0",
            "link_options": "-std=c++20 -ggdb -O0"
        }
    ],
    source_generators = source_generators
);

open("Makefile","w").write(makefile);
