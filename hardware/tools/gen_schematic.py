"""Generate hardware/radio-thing.kicad_sch (+ project symbol library) from hardware/design.py.

Every pin gets a net label (or a no-connect flag) at its connection point, so the schematic
is wire-free but fully connected; KiCad's ERC + netlist export are used to verify it.

usage: KICAD_SYMBOL_DIR=/usr/share/kicad/symbols python3 gen_schematic.py
"""
import os, re, sys, uuid, hashlib
HERE = os.path.dirname(os.path.abspath(__file__))
HW = os.path.dirname(HERE)
sys.path.insert(0, HERE)
sys.path.insert(0, HW)
from sexp import Sym as S, dump, find, first
import klib
from design import PARTS, BLOCK_TITLES, CUSTOM_SYMBOLS

PROJECT = "radio-thing"
GRID = 2.54


def uid(*seed):
    return str(uuid.UUID(hashlib.md5("|".join(map(str, seed)).encode()).hexdigest()))


def snap(v):
    return round(round(v / GRID) * GRID, 4)


def font(size=1.27):
    return [S("effects"), [S("font"), [S("size"), size, size]]]


# ---------------------------------------------------------------- custom symbols
def custom_symbol(name, d):
    left, right = d["left"], d["right"]
    rows = max(len(left), len(right))
    h = (rows + 1) * GRID
    w = 15.24
    top = snap(h / 2)
    body = [S("symbol"), f"{name}_0_1",
            [S("rectangle"), [S("start"), -w / 2, top], [S("end"), w / 2, top - h],
             [S("stroke"), [S("width"), 0.254], [S("type"), S("default")]], [S("fill"), [S("type"), S("background")]]]]
    pins = [S("symbol"), f"{name}_1_1"]
    for side, lst in (("L", left), ("R", right)):
        for i, (num, pname, ptype) in enumerate(lst):
            y = top - GRID * (i + 1)
            x = -w / 2 - GRID if side == "L" else w / 2 + GRID
            ang = 0 if side == "L" else 180
            pins.append([S("pin"), S(ptype), S("line"), [S("at"), x, y, ang], [S("length"), GRID],
                         [S("name"), pname, font()], [S("number"), num, font()]])

    def prop(k, v, y, hide=False):
        e = font()
        if hide:
            e.append([S("hide"), S("yes")])
        return [S("property"), k, v, [S("at"), 0, y, 0], e]

    return [S("symbol"), name, [S("exclude_from_sim"), S("no")], [S("in_bom"), S("yes")], [S("on_board"), S("yes")],
            prop("Reference", d["ref"], top + 1.27), prop("Value", name, top - h - 1.27),
            prop("Footprint", d["footprint"], top - h - 3.81, True), prop("Datasheet", d["datasheet"], 0, True),
            prop("Description", d["description"], 0, True), body, pins]


def write_project_lib():
    syms = [custom_symbol(n, d) for n, d in CUSTOM_SYMBOLS.items()]
    lib = [S("kicad_symbol_lib"), [S("version"), 20241209], [S("generator"), "radio_thing_gen"],
           [S("generator_version"), "9.0"]] + syms
    open(os.path.join(HW, "radio-thing.kicad_sym"), "w").write(dump(lib) + "\n")
    open(os.path.join(HW, "sym-lib-table"), "w").write(
        '(sym_lib_table\n\t(version 7)\n\t(lib (name "RadioThing")(type "KiCad")(uri "${KIPRJMOD}/radio-thing.kicad_sym")(options "")(descr "Radio Thing custom symbols"))\n)\n')
    return {n: s for n, s in zip(CUSTOM_SYMBOLS, syms)}


# ---------------------------------------------------------------- helpers
def get_symbol(lib_id, custom):
    lib, name = lib_id.split(":")
    s = custom[name] if lib == "RadioThing" else klib.load(lib, name)
    return s


def embed(lib_id, sym):
    out = list(sym)
    out[1] = lib_id
    return out


def pin_list(sym):
    return klib.pins(sym)


def sym_bbox(sym):
    xs, ys = [0.0], [0.0]
    for _, _, _, x, y, a, ln, _ in pin_list(sym):
        xs.append(x)
        ys.append(y)
    def walk(n):
        for e in n:
            if isinstance(e, list) and e and e[0] in ("start", "end", "xy", "center"):
                xs.append(float(e[1])); ys.append(float(e[2]))
            elif isinstance(e, list):
                walk(e)
    walk(sym)
    return min(xs), max(xs), min(ys), max(ys)


LABEL_ANGLE = {0: 180, 180: 0, 90: 270, 270: 90}


def main():
    custom = write_project_lib()
    root_uuid = uid("root", PROJECT)
    lib_symbols, used = [], set()
    items = []

    # place blocks in columns, parts stacked inside each block
    blocks = []
    for p in PARTS:
        if p.block not in blocks:
            blocks.append(p.block)
    cols = [["USB", "CHARGER", "FUEL"], ["3V3", "5V", "HEAD"], ["MCU"], ["SENSORS", "UWB"], ["DISPLAY", "UI", "MECH"]]
    assert sorted(sum(cols, [])) == sorted(blocks), (blocks, cols)
    col_x = 25.4
    max_y = 0
    label_margin = 22.86     # room for labels left/right of each symbol
    pwr_flag_nets = set()
    net_pins = {}            # net -> [(ref, pin, type)]
    texts = []

    for col in cols:
        y = 30.48
        col_w = 0
        for blk in col:
            texts.append([S("text"), BLOCK_TITLES[blk], [S("exclude_from_sim"), S("no")],
                          [S("at"), col_x, y, 0], [S("effects"), [S("font"), [S("size"), 2.0, 2.0], [S("bold"), S("yes")]],
                                                   [S("justify"), S("left"), S("bottom")]], [S("uuid"), uid("txt", blk)]])
            y += 7.62
            for p in [q for q in PARTS if q.block == blk]:
                sym = get_symbol(p.lib_id, custom)
                if p.lib_id not in used:
                    used.add(p.lib_id)
                    lib_symbols.append(embed(p.lib_id, sym))
                x0, x1, y0, y1 = sym_bbox(sym)
                ox = snap(col_x + label_margin - x0)
                oy = snap(y + y1 + 2.54)
                col_w = max(col_w, (x1 - x0) + 2 * label_margin)
                items.append(place_symbol(p, sym, ox, oy, root_uuid))
                # labels / no-connects at every pin end
                seen = set()
                pins = pin_list(sym)
                pin_nums = {n for n, *_ in pins}
                missing = set(p.pins) - pin_nums
                assert not missing, f"{p.ref}: unknown pins {missing}"
                for num, name, ptype, px, py, pa, ln, unit in pins:
                    if num not in p.pins:
                        raise SystemExit(f"{p.ref} pin {num} ({name}) not assigned in design.py")
                    net = p.pins[num]
                    ax, ay = round(ox + px, 4), round(oy - py, 4)
                    key = (ax, ay)
                    if key in seen:
                        continue
                    seen.add(key)
                    if net is None:
                        items.append([S("no_connect"), [S("at"), ax, ay], [S("uuid"), uid("nc", p.ref, num)]])
                        continue
                    net_pins.setdefault(net, []).append((p.ref, num, ptype))
                    la = LABEL_ANGLE[int(pa) % 360]
                    just = [S("justify"), S("right" if la in (180, 270) else "left"), S("bottom")]
                    items.append([S("label"), net, [S("at"), ax, ay, la], [S("fields_autoplaced"), S("yes")],
                                  [S("effects"), [S("font"), [S("size"), 1.0, 1.0]], just], [S("uuid"), uid("lbl", p.ref, num)]])
                y = oy - y0 + 7.62
                max_y = max(max_y, y)
            y += 5.08
        col_x = snap(col_x + col_w + 12.7)

    # power flags: nets with power inputs but no power output driver
    for net, lst in net_pins.items():
        types = {t for _, _, t in lst}
        if "power_in" in types and "power_out" not in types:
            pwr_flag_nets.add(net)
    pf = klib.load("power", "PWR_FLAG")
    lib_symbols.append(embed("power:PWR_FLAG", pf))
    fx, fy = 25.4, 12.7
    for i, net in enumerate(sorted(pwr_flag_nets)):
        x = snap(fx + i * 20.32)
        ref = f"#FLG{i + 1:02d}"
        items.append(place_raw("power:PWR_FLAG", pf, ref, "PWR_FLAG", "", x, fy, root_uuid, in_bom=False))
        items.append([S("label"), net, [S("at"), x, fy, 270], [S("fields_autoplaced"), S("yes")],
                      [S("effects"), [S("font"), [S("size"), 1.0, 1.0]], [S("justify"), S("right"), S("bottom")]],
                      [S("uuid"), uid("pflbl", net)]])

    width = max(594.0, col_x + 20)
    sch = [S("kicad_sch"), [S("version"), 20250114], [S("generator"), "eeschema"], [S("generator_version"), "9.0"],
           [S("uuid"), root_uuid], [S("paper"), "User", snap(width), snap(max(420.0, max_y + 40))],
           [S("title_block"), [S("title"), "Radio Thing main board"], [S("date"), "2026-10-19"], [S("rev"), "A"],
            [S("company"), "radio-thing"],
            [S("comment"), 1, "Generated from hardware/design.py by hardware/tools/gen_schematic.py - edit design.py, not this file"],
            [S("comment"), 2, "Same PCB, two builds: POD and DISPLAY (see Variant field / BOM)"]],
           [S("lib_symbols")] + lib_symbols] + texts + items + [
           [S("sheet_instances"), [S("path"), "/", [S("page"), "1"]]], [S("embedded_fonts"), S("no")]]
    out = os.path.join(HW, f"{PROJECT}.kicad_sch")
    open(out, "w").write(dump(sch) + "\n")
    write_project_file()
    print(f"wrote {out}: {len(PARTS)} parts, {len(net_pins)} nets, {len(pwr_flag_nets)} PWR_FLAGs")


def place_symbol(p, sym, x, y, root):
    extra = {"Variant": p.variant}
    if p.note:
        extra["Note"] = p.note
    return place_raw(p.lib_id, sym, p.ref, p.value, p.footprint, x, y, root,
                     in_bom=not p.lib_id.startswith("Mechanical:"), on_board=True,
                     dnp=(p.variant == "OPT"), extra=extra)


def place_raw(lib_id, sym, ref, value, footprint, x, y, root, in_bom=True, dnp=False, extra=None, on_board=None):
    on_board = in_bom if on_board is None else on_board
    u = uid("sym", ref)
    props = []
    lib_props = {pp[1]: pp for pp in find(sym, "property")}
    vals = {"Reference": ref, "Value": value}
    if footprint:
        vals["Footprint"] = footprint
    for k in ("Reference", "Value", "Footprint", "Datasheet", "Description"):
        lp = lib_props.get(k)
        v = vals.get(k, lp[2] if lp else "")
        at = first(lp, "at") if lp else None
        px, py, pa = (float(at[1]), float(at[2]), float(at[3]) if len(at) > 3 else 0) if at else (0, 0, 0)
        e = font()
        if k not in ("Reference", "Value") or (lp and first(first(lp, "effects") or [], "hide")):
            e.append([S("hide"), S("yes")])
        props.append([S("property"), k, v, [S("at"), round(x + px, 4), round(y - py, 4), pa], e])
    for k, v in (extra or {}).items():
        props.append([S("property"), k, v, [S("at"), x, y, 0], font() + [[S("hide"), S("yes")]]])
    pin_nums = sorted({n for n, *_ in klib.pins(sym)})
    return ([S("symbol"), [S("lib_id"), lib_id], [S("at"), x, y, 0], [S("unit"), 1],
             [S("exclude_from_sim"), S("no")], [S("in_bom"), S("yes" if in_bom else "no")], [S("on_board"), S("yes" if on_board else "no")],
             [S("dnp"), S("yes" if dnp else "no")], [S("fields_autoplaced"), S("yes")], [S("uuid"), u]] + props +
            [[S("pin"), n, [S("uuid"), uid("pin", ref, n)]] for n in pin_nums] +
            [[S("instances"), [S("project"), PROJECT, [S("path"), f"/{root}", [S("reference"), ref], [S("unit"), 1]]]]])


def write_project_file():
    path = os.path.join(HW, f"{PROJECT}.kicad_pro")
    if not os.path.exists(path):
        open(path, "w").write('{\n  "meta": {"filename": "radio-thing.kicad_pro", "version": 1},\n  "schematic": {},\n  "board": {}\n}\n')


if __name__ == "__main__":
    main()
