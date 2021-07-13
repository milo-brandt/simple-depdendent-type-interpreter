import os;
from glob import glob;
from jinja2 import Environment, FileSystemLoader
from functools import lru_cache
import re

@lru_cache
def getIncludedFileListImpl(filename):
    if not os.path.isfile(filename):
        raise RuntimeError("No file at " + filename)
    str = open(filename).read()
    requirements = {os.path.normpath(os.path.join(os.path.dirname(filename), m.group(1))) for m in re.finditer("#include \"([^\"]+)\"", str)}
    output = requirements.copy()
    [root, ext] = os.path.splitext(filename);
    if ext != ".inl":
        for r in requirements: #set comprehension caused caching to fail?
            try:
                output = output.union(getIncludedFileListImpl(r))
            except RuntimeError as err:
                print("Failed while parsing file " + filename)
                raise
    #inductive_requirements = {getIncludedFileList(r) for r in requirements}
    return output#.union(inductive_requirements)

def getIncludedFileList(filename):
    return [os.path.relpath(f, "..") for f in getIncludedFileListImpl("../Source/" + filename + ".cpp")]

os.chdir(os.path.dirname(os.path.abspath(__file__)))
jinja_env = Environment(
    loader=FileSystemLoader('.')
);

source_files = [os.path.splitext(os.path.relpath(file_path, "../Source"))[0] for folder in os.walk("../Source") for file_path in glob(os.path.join(folder[0],'*.cpp'))];
test_files = source_files.copy();
test_files.remove("main");
test_files = [filename for filename in source_files if filename != 'main' and os.path.commonpath(['WebInterface',filename]) != 'WebInterface'];
non_test_files = [filename for filename in source_files if os.path.commonpath(['Tests',filename]) != 'Tests'];

makefile = jinja_env.get_template('makefile_template').render(source_files = source_files, test_files = test_files, non_test_files = non_test_files, getIncludedFileList = getIncludedFileList);
open("../Makefile","w").write(makefile);
