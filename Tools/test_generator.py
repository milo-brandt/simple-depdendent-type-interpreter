import os;
from jinja2 import Environment, FileSystemLoader
import sys

class TestCase:
    def __init__(self, filename):
        self.name = None
        self.filename = filename
        self.preamble = None
        self.definitions = []
        self.body = None
        self.tags = None
    def add_tag(self, name):
        if self.tags == None:
            self.tags = []
        self.tags.append(name)
    def run_command(self, head, body):
        if head.startswith("BEGIN"):
            pass
        elif head.startswith("NAME"):
            if self.name != None:
                raise RuntimeError("Multiple names for test!")
            self.name = head[5:]
        elif head.startswith("TAG"):
            self.add_tag(head[4:])
        elif head.startswith("HEAVY"):
            self.add_tag(".") #hide
            self.add_tag("heavy")
        elif head.startswith("SET"):
            self.definitions.append({
                "kind": "SET",
                "var": head[4:],
                "source": body
            })
        elif head.startswith("FULL_SET"):
            self.definitions.append({
                "kind": "FULL_SET",
                "var": head[9:],
                "source": body
            })
        elif head.startswith("MUST_COMPILE"):
            self.definitions.append({
                "kind": "MUST_COMPILE",
                "source": body
            })
        elif head.startswith("DEFINITION"):
            if self.body != None:
                raise RuntimeError("Multiple definition bodies for test!")
            self.body = body
        else:
            raise RuntimeError("Unrecognized head: " + head)

def test_case_from_file(filename):
    with open(filename) as file:
        last_command_line = file.readline()
        if not last_command_line.startswith("# TEST "):
            raise RuntimeError("File " + filename + " does not begin with command")
        command_body = ""
        ret = TestCase(filename)
        while True:
            line = file.readline()
            if len(line) == 0:
                ret.run_command(last_command_line[7:-1], command_body)
                return ret
            if line.startswith("# TEST "):
                ret.run_command(last_command_line[7:-1], command_body)
                command_body = ""
                last_command_line = line
            else:
                command_body = command_body + line

jinja_env = Environment(
    loader=FileSystemLoader(os.path.dirname(os.path.abspath(__file__)) + '/SourceGeneratorTemplates')
);
test_template = jinja_env.get_template('test_generator.hpp.template')

def write_test_cases(input_list, output):
    content = test_template.render({
        "tests": [test_case_from_file(file) for file in input_list]
    })
    with open(output, "w") as file:
        file.write(content)

if __name__ == "__main__":
    if len(sys.argv) <= 2:
        raise RuntimeError("No input files supplied to test generator.")
    output = sys.argv[1]
    write_test_cases(sys.argv[2:], output)
