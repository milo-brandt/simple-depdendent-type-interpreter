import json;
import os;

def extractName(str):
    return os.path.basename(str)
def makeLine(str, frame):
    return "{} {} {} {} {}".format(extractName(str), frame['x'], frame['y'], frame['w'], frame['h'])

data = json.load(open("Preresource/texture.json"))['frames']
lines = [makeLine(k, v['frame']) for (k,v) in data.items()]
output = "\n".join(lines);
open("Resources/Images/Menu/atlas.txt","w").write(output);
