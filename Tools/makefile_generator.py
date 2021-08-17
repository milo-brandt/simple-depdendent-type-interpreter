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
def getIncludedFileListImpl(filename):
    if not os.path.isfile(filename):
        raise RuntimeError("No file at " + filename)
    str = open(filename).read()
    requirements = {os.path.normpath(os.path.join(os.path.dirname(filename), m.group(1))) for m in re.finditer("#include \"([^\"]+)\"", remove_c_comments(str))}
    output = requirements.copy()
    [root, ext] = os.path.splitext(filename);
    if ext != ".inl":
        for r in requirements: #set comprehension caused caching to fail?
            try:
                output = output.union(getIncludedFileListImpl(r))
            except:
                print("Failed while parsing file " + filename)
                raise
    return output

def getIncludedFileList(filename):
    return [f for f in getIncludedFileListImpl("Source/" + filename + ".cpp")]

os.chdir(os.path.dirname(os.path.abspath(__file__)) + "/..")
jinja_env = Environment(
    loader=FileSystemLoader('.')
);

def list_roots_with_extension(ext, subdirectory = ""):
    return [os.path.splitext(os.path.relpath(file_path, "Source"))[0] for folder in os.walk("Source/" + subdirectory) for file_path in glob(os.path.join(folder[0],"*." + ext))];
def get_object_files_from(list):
    roots = [os.path.splitext(file)[0] for file in list]
    return {os.path.relpath(file, "Source") for file in roots if os.path.isfile(file + ".cpp")}
def generate_python_info():
    def is_source_generator(name):
        try:
            with open(name) as file:
                line = file.readline()[:-1]
                return line == "# Source Generator"
        except:
            return False
    python_files = ["Source/" + file + ".py" for file in list_roots_with_extension("py")]
    source_generators = [file for file in python_files if is_source_generator(file)]
    output_data = []
    for file in source_generators:
        outputs = source_generator.process_file(file)
        output_data.append({
            "source": file,
            "outputs": outputs
        })
    return output_data

source_gen_data = generate_python_info()

source_files = list_roots_with_extension("cpp")
test_roots = list_roots_with_extension("cpp", "Tests")
test_files = get_object_files_from([file for test_root in test_roots for file in getIncludedFileList(test_root)] + ["Source/" + root + ".cpp" for root in test_roots])
main_files = get_object_files_from(getIncludedFileList("main") + ["Source/main.cpp"])




makefile = jinja_env.get_template('Tools/makefile_template').render(
    source_files = source_files,
    test_files = test_files,
    main_files = main_files,
    source_generators = source_gen_data,
    getIncludedFileList = getIncludedFileList
);
open("Makefile","w").write(makefile);
