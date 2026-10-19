"""Tiny S-expression reader/writer for KiCad files (no external deps)."""
import re

_TOK = re.compile(r'\s*(?:(\()|(\))|("(?:[^"\\]|\\.)*")|([^\s()"]+))')


class Sym(str):
    """Unquoted atom (keyword/number) - distinguished from quoted strings."""


def parse(text):
    stack, cur = [], []
    pos = 0
    while True:
        m = _TOK.match(text, pos)
        if not m or m.end() == pos:
            break
        pos = m.end()
        lp, rp, s, a = m.groups()
        if lp:
            stack.append(cur)
            cur = []
        elif rp:
            done, cur = cur, stack.pop()
            cur.append(done)
        elif s is not None:
            cur.append(bytes(s[1:-1], "utf-8").decode("unicode_escape") if "\\" in s else s[1:-1])
        else:
            cur.append(Sym(a))
    return cur[0] if len(cur) == 1 else cur


def dump(x, indent=0):
    sp = "\t" * indent
    if isinstance(x, list):
        if not x:
            return "()"
        simple = all(not isinstance(e, list) for e in x)
        if simple:
            return "(" + " ".join(dump(e) for e in x) + ")"
        head = [dump(e) for e in x if not isinstance(e, list)]
        out = "(" + " ".join(head)
        for e in x:
            if isinstance(e, list):
                out += "\n" + sp + "\t" + dump(e, indent + 1)
        return out + "\n" + sp + ")"
    if isinstance(x, Sym):
        return str(x)
    if isinstance(x, (int, float)):
        return repr(x)
    return '"' + str(x).replace("\\", "\\\\").replace('"', '\\"').replace("\n", "\\n") + '"'


def find(x, key):
    return [e for e in x if isinstance(e, list) and e and e[0] == key]


def first(x, key):
    r = find(x, key)
    return r[0] if r else None
