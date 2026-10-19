"""Load symbols (resolving `extends`) from KiCad 9 .kicad_sym libraries."""
import os, sys, copy
sys.path.insert(0, os.path.dirname(__file__))
from sexp import parse, find, first, Sym

LIBDIR = os.environ.get("KICAD_SYMBOL_DIR", "/usr/share/kicad/symbols")
_cache = {}


def _lib(name):
    if name not in _cache:
        tree = parse(open(os.path.join(LIBDIR, name + ".kicad_sym")).read())
        _cache[name] = {s[1]: s for s in find(tree, "symbol")}
    return _cache[name]


def load(lib, name):
    """Return a flattened symbol s-expr (no `extends`), named name."""
    syms = _lib(lib)
    s = copy.deepcopy(syms[name])
    ext = first(s, "extends")
    if not ext:
        return s
    parent = load(lib, ext[1])
    props = {p[1]: p for p in find(s, "property")}
    out = [Sym("symbol"), name]
    for e in parent[2:]:
        if isinstance(e, list) and e and e[0] == "property" and e[1] in props:
            out.append(props.pop(e[1]))
        elif isinstance(e, list) and e and e[0] == "symbol":
            sub = copy.deepcopy(e)
            sub[1] = sub[1].replace(parent[1], name, 1)
            out.append(sub)
        else:
            out.append(e)
    # insert child-only properties before the unit sub-symbols
    idx = max(i for i, e in enumerate(out) if isinstance(e, list) and e and e[0] == "property") + 1
    for p in props.values():
        out.insert(idx, p)
        idx += 1
    return out


def pins(sym):
    """[(number, name, type, x, y, angle, length, unit)]"""
    res = []
    for sub in find(sym, "symbol"):
        unit = int(sub[1].rsplit("_", 2)[-2])
        for p in find(sub, "pin"):
            at = first(p, "at")
            res.append((first(p, "number")[1], first(p, "name")[1], str(p[1]),
                        float(at[1]), float(at[2]), float(at[3]) if len(at) > 3 else 0.0,
                        float(first(p, "length")[1]), unit))
    return res


def prop(sym, key):
    for p in find(sym, "property"):
        if p[1] == key:
            return p[2]
    return None
