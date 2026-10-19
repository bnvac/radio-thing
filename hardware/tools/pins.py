"""Print pins (number, name, type) of a KiCad library symbol file."""
import sys, os
sys.path.insert(0, os.path.dirname(__file__))
from sexp import parse, find, first

def pins(path):
    tree = parse(open(path).read())
    sym = first(tree, "symbol")
    out = []
    def walk(n):
        for e in n:
            if isinstance(e, list) and e and e[0] == "pin":
                out.append((first(e, "number")[1], first(e, "name")[1], str(e[1])))
            elif isinstance(e, list):
                walk(e)
    walk(sym)
    return out

if __name__ == "__main__":
    for p in sys.argv[1:]:
        print("==", os.path.basename(p))
        print("  " + "  ".join(f"{n}:{nm}({t[:3]})" for n, nm, t in sorted(pins(p), key=lambda r: (len(r[0]), r[0]))))
