"""Generate hardware/radio-thing.kicad_pcb: board outline, 4-layer stack, all footprints placed by
functional block with every pad assigned to its net (from design.py). Routing is NOT done here.

Board: 82 x 50 mm main area (= POD board) + 82 x 22 mm button tab below a V-score line
(DISPLAY build keeps the tab; POD build snaps it off).

usage: KICAD_FOOTPRINT_DIR=/usr/share/kicad/footprints python3 gen_pcb.py <schematic netlist>
"""
import os, sys, uuid, hashlib
import pcbnew

HERE = os.path.dirname(os.path.abspath(__file__)); HW = os.path.dirname(HERE)
sys.path.insert(0, HW)
from design import PARTS

FPDIR = os.environ.get("KICAD_FOOTPRINT_DIR", "/usr/share/kicad/footprints")
OX, OY = 100.0, 100.0           # board origin in KiCad coordinates (mm)
W, H_MAIN, H_TAB = 82.0, 50.0, 22.0
MM = pcbnew.FromMM


def uid(*seed):
    return str(uuid.UUID(hashlib.md5("|".join(map(str, seed)).encode()).hexdigest()))


# Fixed positions (x, y, rotation) relative to board top-left, for connectors/edge parts/big modules.
FIXED = {
    "J4": (22.0, 3.5, 180),     # radar head connector, top edge (pod antenna face)
    "U6": (52.0, 18.0, 0),      # ESP32-S3-WROOM-1U; U.FL end toward top edge
    "J3": (73.2, 8.5, 0),       # display socket (pin 1 top), right edge - match your module's header!
    "SW1": (3.0, 12.0, 270),    # power slide switch, left edge
    "J2": (5.5, 23.5, 270),     # LiPo JST-PH, left edge
    "J1": (3.5, 35.5, 270),     # USB-C, left edge
    "U9": (52.0, 40.0, 90),     # optional DWM3000
    "SW2": (30.0, 26.0, 0), "SW3": (36.5, 26.0, 0),   # RESET / BOOT
    "J5": (37.0, 39.0, 0),      # debug UART header
    "H1": (3.5, 3.5, 0), "H2": (W - 3.5, 3.5, 0), "H3": (3.5, H_MAIN - 3.5, 0), "H4": (W - 3.5, H_MAIN - 3.5, 0),
    # button tab: 5 x 12 mm buttons on 16.2 mm pitch
    "SW4": (8.5, 61.0, 0), "SW5": (24.75, 61.0, 0), "SW6": (41.0, 61.0, 0), "SW7": (57.25, 61.0, 0), "SW8": (73.5, 61.0, 0),
}

# Remaining parts are shelf-packed into these regions (x0, y0, x1, y1), by block.
REGIONS = {
    "HEAD": (31.0, 0.5, 42.0, 7.0), "5V": (9.0, 7.0, 42.0, 14.5),
    "CHARGER": (11.2, 15.0, 27.0, 27.5), "FUEL": (27.0, 14.5, 42.0, 20.0),
    "MCU": (27.0, 20.0, 42.0, 23.5), "3V3": (9.0, 28.0, 27.0, 36.0),
    "USB": (9.0, 36.0, 23.0, 49.5), "SENSORS": (23.0, 31.0, 35.5, 49.5),
    "UI": (62.3, 0.5, 71.3, 30.5), "DISPLAY": (65.3, 30.5, 71.3, 44.0), "UWB": (65.3, 44.0, 74.8, 49.5),
}


def load_fp(fpid):
    lib, name = fpid.split(":")
    fp = pcbnew.FootprintLoad(os.path.join(FPDIR, lib + ".pretty"), name)
    if fp is None:
        raise SystemExit(f"footprint not found: {fpid}")
    fp.SetFPID(pcbnew.LIB_ID(lib, name))
    return fp


def seg(board, layer, x0, y0, x1, y1, w=0.1):
    s = pcbnew.PCB_SHAPE(board)
    s.SetShape(pcbnew.SHAPE_T_SEGMENT)
    s.SetStart(pcbnew.VECTOR2I_MM(OX + x0, OY + y0)); s.SetEnd(pcbnew.VECTOR2I_MM(OX + x1, OY + y1))
    s.SetLayer(layer); s.SetWidth(MM(w))
    board.Add(s)


def text(board, layer, s, x, y, size=1.0):
    t = pcbnew.PCB_TEXT(board)
    t.SetText(s); t.SetLayer(layer)
    t.SetPosition(pcbnew.VECTOR2I_MM(OX + x, OY + y))
    t.SetTextSize(pcbnew.VECTOR2I_MM(size, size)); t.SetTextThickness(MM(size * 0.15))
    board.Add(t)


def netlist_map(path):
    """(ref, pad) -> net name, read from `kicad-cli sch export netlist` output."""
    sys.path.insert(0, HERE)
    from sexp import parse, find, first
    tree = parse(open(path).read())
    m = {}
    for net in find(first(tree, "nets"), "net"):
        for node in find(net, "node"):
            m[(first(node, "ref")[1], first(node, "pin")[1])] = first(net, "name")[1]
    return m


def main():
    pad_net = netlist_map(sys.argv[1])
    sch_uuid = {p.ref: uid("sym", p.ref) for p in PARTS}      # same seeds as gen_schematic.py
    board = pcbnew.BOARD()
    board.SetCopperLayerCount(4)
    ds = board.GetDesignSettings()
    ds.SetBoardThickness(MM(1.6))
    ds.m_TrackMinWidth = MM(0.127); ds.m_MinClearance = MM(0.127)
    ds.m_ViasMinSize = MM(0.45); ds.m_MinThroughDrill = MM(0.2)

    nets = {}
    for n in sorted(set(pad_net.values())):
        ni = pcbnew.NETINFO_ITEM(board, n)
        board.Add(ni)
        nets[n] = ni

    # outline: main + tab, with V-score marker at y = H_MAIN
    total_h = H_MAIN + H_TAB
    for a, b in [((0, 0), (W, 0)), ((W, 0), (W, total_h)), ((W, total_h), (0, total_h)), ((0, total_h), (0, 0))]:
        seg(board, pcbnew.Edge_Cuts, *a, *b, w=0.1)
    seg(board, pcbnew.User_1, -3, H_MAIN, W + 3, H_MAIN, w=0.2)
    text(board, pcbnew.User_1, "V-SCORE (POD build: snap off button tab)", W / 2, H_MAIN + 1.5, 1.0)
    text(board, pcbnew.F_SilkS, "RADIO THING rev A", 41.0, 70.5, 1.2)

    placed, problems = [], []
    shelves = {k: [v[0], v[1], 0.0] for k, v in REGIONS.items()}   # cursor x, cursor y, row height
    for p in PARTS:
        fp = load_fp(p.footprint)
        fp.SetReference(p.ref); fp.SetValue(p.value)
        fp.SetPath(pcbnew.KIID_PATH(f"/{sch_uuid[p.ref]}"))
        if p.variant == "OPT":
            fp.SetDNP(True)
        board.Add(fp)
        if p.ref in FIXED:
            x, y, rot = FIXED[p.ref]
            fp.SetOrientationDegrees(rot)
            fp.SetPosition(pcbnew.VECTOR2I_MM(OX + x, OY + y))
        else:
            bb = fp.GetBoundingBox(False, False)
            w, h = pcbnew.ToMM(bb.GetWidth()) + 0.6, pcbnew.ToMM(bb.GetHeight()) + 0.6
            x0, y0, x1, y1 = REGIONS[p.block]
            cx, cy, rh = shelves[p.block]
            if cx + w > x1:
                cx, cy, rh = x0, cy + rh, 0.0
            if cy + h > y1:
                problems.append(f"{p.ref} ({p.block}) overflows region")
            fp.SetPosition(pcbnew.VECTOR2I_MM(OX + cx + w / 2, OY + cy + h / 2))
            # footprint origin is not always its bbox centre: correct it
            nb = fp.GetBoundingBox(False, False)
            dx = (OX + cx + w / 2) - pcbnew.ToMM(nb.GetCenter().x)
            dy = (OY + cy + h / 2) - pcbnew.ToMM(nb.GetCenter().y)
            fp.Move(pcbnew.VECTOR2I_MM(dx, dy))
            shelves[p.block] = [cx + w, cy, max(rh, h)]
        # nets
        for pad in fp.Pads():
            num = pad.GetNumber()
            if (p.ref, num) in pad_net:
                pad.SetNet(nets[pad_net[(p.ref, num)]])
            elif num and num not in p.pins and num not in ("MP",) and p.pins:
                problems.append(f"{p.ref} pad {num} has no pin in design (left unconnected)")
        placed.append(p.ref)

    # inner ground planes: In1 = GND, In2 = GND (route power on outer layers / pours)
    for layer, prio in ((pcbnew.In1_Cu, 0), (pcbnew.In2_Cu, 0)):
        z = pcbnew.ZONE(board)
        z.SetLayer(layer); z.SetNet(nets["/GND"]); z.SetAssignedPriority(prio)
        z.SetLocalClearance(MM(0.3)); z.SetMinThickness(MM(0.2))
        z.SetPadConnection(pcbnew.ZONE_CONNECTION_THERMAL)
        ol = z.Outline(); ol.NewOutline()
        for (x, y) in [(0.5, 0.5), (W - 0.5, 0.5), (W - 0.5, H_MAIN + H_TAB - 0.5), (0.5, H_MAIN + H_TAB - 0.5)]:
            ol.Append(MM(OX + x), MM(OY + y))
        board.Add(z)
    # zones are filled by KiCad (kicad-cli pcb drc --refill-zones, or press B in the editor)

    out = os.path.join(HW, "radio-thing.kicad_pcb")
    pcbnew.SaveBoard(out, board)
    apply_project_rules(os.path.join(HW, "radio-thing.kicad_pro"))
    print(f"wrote {out}: {len(placed)} footprints, {len(nets)} nets")
    for pr in problems:
        print("  NOTE:", pr)


def apply_project_rules(pro):
    """JLCPCB 4-layer friendly rules + a Power net class (wider tracks)."""
    import json
    d = json.load(open(pro))
    rules = d.setdefault("board", {}).setdefault("design_settings", {}).setdefault("rules", {})
    rules.update({"min_clearance": 0.1, "min_track_width": 0.127, "min_via_diameter": 0.45,
                  "min_through_hole_diameter": 0.2, "min_copper_edge_clearance": 0.3, "min_hole_to_hole": 0.25})
    ns = d.setdefault("net_settings", {})
    default = next(c for c in ns["classes"] if c["name"] == "Default")
    default.update({"clearance": 0.127, "track_width": 0.2, "via_diameter": 0.6, "via_drill": 0.3})
    power = dict(default, name="Power", clearance=0.127, track_width=0.5, via_diameter=0.8, via_drill=0.4, priority=0)
    ns["classes"] = [default, power]
    ns["netclass_patterns"] = [{"netclass": "Power", "pattern": pat} for pat in
                               ("/VBUS*", "/VSYS", "/VBAT", "/+3V3", "/+5V*", "/BB_L*", "/HB_L", "/GND")]
    json.dump(d, open(pro, "w"), indent=2)


if __name__ == "__main__":
    main()
