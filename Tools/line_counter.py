import os;
def lines_in_file(filename):
    return len(open(filename).readlines())
os.chdir(os.path.dirname(os.path.abspath(__file__)))

line_counts = [(lines_in_file(os.path.join(root, name)), os.path.join(root, name)) for root, dirs, files in os.walk("../Source") for name in files if not "_impl" in name]
line_number = sum(v[0] for v in line_counts);
line_counts.sort(reverse = True)
print("Lines of code: ", line_number)
for a,b in line_counts:
    print(a,b);
