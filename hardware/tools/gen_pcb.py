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
# Break line between main board and button tab: routed slots (x0, x1) at y = H_MAIN, SLOT_W wide.
# Gaps between slots are solid bridges; button traces cross through the bridges.
SLOT_W = 1.5
SLOTS = [(1.5, 24.0), (28.0, 38.5), (43.5, 54.0), (58.0, 80.5)]
EDGE_KEEPOUT = 0.35   # no tracks/vias this close to any board edge or slot


def uid(*seed):
    return str(uuid.UUID(hashlib.md5("|".join(map(str, seed)).encode()).hexdigest()))


# Fixed positions (x, y, rotation) relative to board top-left: connectors, edge parts, modules and the
# ICs that anchor each power/sensor cluster.
FIXED = {
    "J4": (22.0, 3.5, 180),     # radar head connector, top edge (pod antenna face)
    "U6": (52.0, 18.0, 0),      # ESP32-S3-WROOM-1U; U.FL end toward top edge
    "J3": (73.2, 8.5, 0),       # display socket (pin 1 top), right edge - match your module's header!
    "SW1": (3.0, 12.0, 270),    # power slide switch, left edge
    "J2": (5.5, 23.5, 270),     # LiPo JST-PH, left edge
    "J1": (3.5, 35.5, 270),     # USB-C, left edge
    "SW2": (33.0, 26.3, 0), "SW3": (66.5, 27.5, 90),  # RESET / BOOT (BOOT next to IO0 on the module's right side)
    "J5": (60.0, 37.0, 0),      # debug UART header
    # cluster anchors
    "U5": (16.5, 12.0, 0),      # TPS61202 5 V boost [POD]
    "U2": (17.5, 23.5, 0),      # BQ24074 charger
    "U3": (30.0, 17.5, 0),      # MAX17048 fuel gauge
    "U4": (31.0, 37.0, 0),      # TPS63001 3.3 V buck-boost
    "U7": (44.0, 40.0, 0),      # ISM330DHCX IMU
    "U8": (51.0, 40.0, 0),      # LIS2MDL magnetometer
    "H1": (3.5, 3.5, 0), "H2": (W - 3.5, 3.5, 0), "H3": (3.5, H_MAIN - 3.5, 0), "H4": (W - 3.5, H_MAIN - 3.5, 0),
    "H5": (3.5, H_MAIN + H_TAB - 3.5, 0), "H6": (W - 3.5, H_MAIN + H_TAB - 3.5, 0),   # tab support
    # button tab: 3 x 12 mm buttons, centred right under the screen (UP | OK | DOWN, 20 mm pitch).
    # Assumes the display module is centred on the main area; its lower edge lands ~2.5 mm into the tab.
    "SW4": (21.0, 61.0, 0), "SW6": (41.0, 61.0, 0), "SW5": (61.0, 61.0, 0),
}

# Parts placed tight against the IC pins they serve: ref -> (anchor ref, dx, dy). Rotation is chosen
# automatically (0/90/180/270) to minimise the distance from each pad to the anchor pads on the same net.
SATELLITES = {
    # 5 V boost: inductor on the L/VIN side, input/output caps at their pins, LC filter beside
    "L2": ("U5", -4.9, 0.6), "C10": ("U5", -1.2, 3.1), "C11": ("U5", -3.6, -2.8), "C12": ("U5", -1.0, -3.0),
    "R9": ("U5", 3.4, 1.6), "FB1": ("U5", 4.0, -2.6), "C13": ("U5", 7.6, -1.6), "C14": ("U5", 4.0, 0.0),
    # charger: battery side faces J2, input/timers on top, output/status on the right
    "C4": ("U2", -3.3, 0.6), "R3": ("U2", -3.1, -1.4), "C1": ("U2", 1.6, -3.3), "R5": ("U2", 0.0, -3.4),
    "R6": ("U2", -1.5, -3.4), "R7": ("U2", -3.0, -3.2), "C2": ("U2", 3.3, 0.6), "C3": ("U2", 5.5, 0.6),
    "R4": ("U2", 3.2, -1.6), "R8": ("U2", 3.2, 2.4),
    # fuel gauge
    "C5": ("U3", 2.6, 0.0),
    # 3.3 V buck-boost: inductor on the L1/L2 side, VIN cap below, VOUT caps above, VINA RC on the right
    "L1": ("U4", -4.9, 0.0), "C6": ("U4", -1.2, 3.3), "C8": ("U4", -1.4, -3.4), "C9": ("U4", 1.4, -3.4),
    "C7": ("U4", 3.3, 0.0), "R24": ("U4", 3.3, 1.8),
    # sensors: decoupling at the supply pins, C1 cap right at LIS2MDL pins 5/6
    "C18": ("U7", 0.0, 2.6), "C19": ("U7", -1.6, 2.6), "R22": ("U7", -2.8, -0.6),
    "R12": ("U7", -2.6, -2.8), "R13": ("U7", -4.3, -2.8),   # I2C pull-ups at the sensor end of the bus
    "C20": ("U8", 0.0, 2.4), "C21": ("U8", 2.4, -0.4), "C25": ("U8", 2.6, 1.6),
    # USB-C: ESD array in line with the data pins, CC resistors at the CC pins, fuse on the VBUS side
    "U1": ("J1", 9.5, 0.0), "R1": ("J1", 8.5, -3.8), "R2": ("J1", 8.5, 3.8), "F1": ("J1", 9.5, 7.2),
    # radar head ID network next to J4 pin 10
    "R21": ("J4", -10.0, 0.5), "C24": ("J4", -10.0, 2.5),
    # status LEDs + resistors just below the ESP32 module (light-pipe to the case)
    "R18": ("U6", 4.5, 11.6), "R19": ("U6", 6.5, 11.6), "R20": ("U6", 8.5, 11.6),
    "D2": ("U6", 4.5, 13.6), "D3": ("U6", 6.5, 13.6), "D4": ("U6", 8.5, 13.6),
}

# Remaining parts are shelf-packed into these regions (x0, y0, x1, y1), by block.
REGIONS = {
    "HEAD": (31.0, 0.5, 42.0, 7.0), "5V": (26.0, 7.0, 42.0, 14.0),
    "CHARGER": (11.2, 27.0, 23.0, 30.0), "FUEL": (34.0, 15.0, 42.0, 20.0),
    "MCU": (27.0, 20.5, 42.0, 23.5), "3V3": (23.0, 30.5, 38.0, 32.5),
    "USB": (9.0, 31.0, 23.0, 49.5), "SENSORS": (38.5, 44.0, 57.0, 49.5),
    "UI": (62.3, 0.5, 71.3, 30.5), "DISPLAY": (65.3, 30.5, 71.3, 44.0),
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

    # outline: main + tab. The tab joins the main board only through solid bridges between routed slots
    # (a V-score would cut every copper layer, including the button traces). POD build: cut the bridges.
    total_h = H_MAIN + H_TAB
    for a, b in [((0, 0), (W, 0)), ((W, 0), (W, total_h)), ((W, total_h), (0, total_h)), ((0, total_h), (0, 0))]:
        seg(board, pcbnew.Edge_Cuts, *a, *b, w=0.1)
    for x0, x1 in SLOTS:
        y0, y1 = H_MAIN, H_MAIN + SLOT_W
        for a, b in [((x0, y0), (x1, y0)), ((x1, y0), (x1, y1)), ((x1, y1), (x0, y1)), ((x0, y1), (x0, y0))]:
            seg(board, pcbnew.Edge_Cuts, *a, *b, w=0.1)
    text(board, pcbnew.User_1, "POD build: cut the bridges between the slots and sand flush", W / 2, H_MAIN + SLOT_W + 1.2, 0.8)
    text(board, pcbnew.F_SilkS, "RADIO THING rev A", 41.0, 70.5, 1.2)

    placed, problems = [], []
    fps = {}
    for p in PARTS:
        fp = load_fp(p.footprint)
        fp.SetReference(p.ref); fp.SetValue(p.value)
        fp.SetPath(pcbnew.KIID_PATH(f"/{sch_uuid[p.ref]}"))
        if p.variant == "OPT":
            fp.SetDNP(True)
        board.Add(fp)
        for pad in fp.Pads():
            num = pad.GetNumber()
            if (p.ref, num) in pad_net:
                pad.SetNet(nets[pad_net[(p.ref, num)]])
            elif num and num not in p.pins and num not in ("MP",) and p.pins:
                problems.append(f"{p.ref} pad {num} has no pin in design (left unconnected)")
        fps[p.ref] = fp
        placed.append(p.ref)

    def put(fp, x, y, rot):
        fp.SetOrientationDegrees(rot)
        fp.SetPosition(pcbnew.VECTOR2I_MM(OX + x, OY + y))

    for ref, (x, y, rot) in FIXED.items():
        if ref in fps:
            put(fps[ref], x, y, rot)

    def net_dist(fp, anchor):
        apads = [(a.GetNetname(), a.GetPosition()) for a in anchor.Pads() if a.GetNetname()]
        total = 0.0
        for pad in fp.Pads():
            n = pad.GetNetname()
            if not n or n == "/GND":
                continue
            d = [((pad.GetPosition().x - q.x) ** 2 + (pad.GetPosition().y - q.y) ** 2) ** 0.5 for m, q in apads if m == n]
            if d:
                total += min(d)
        return total

    for ref, (anc, dx, dy) in SATELLITES.items():
        if ref not in fps:
            continue
        ax, ay, _ = FIXED[anc]
        best = None
        for rot in (0, 90, 180, 270):
            put(fps[ref], ax + dx, ay + dy, rot)
            d = net_dist(fps[ref], fps[anc])
            if best is None or d < best[0] - 1e-6:
                best = (d, rot)
        put(fps[ref], ax + dx, ay + dy, best[1])

    # resolve overlaps: push each satellite away from its anchor until its courtyard is clear
    def court(fp):
        fp.BuildCourtyardCaches()
        poly = fp.GetCourtyard(pcbnew.F_CrtYd)
        bb = poly.BBox() if poly.OutlineCount() else fp.GetBoundingBox(False, False)
        return (bb.GetLeft(), bb.GetTop(), bb.GetRight(), bb.GetBottom())

    def overlaps(r, q, m=MM(0.1)):
        return not (r[2] + m <= q[0] or q[2] + m <= r[0] or r[3] + m <= q[1] or q[3] + m <= r[1])

    settled = [r for r in FIXED if r in fps]
    for ref, (anc, dx, dy) in SATELLITES.items():
        if ref not in fps:
            continue
        fp = fps[ref]
        norm = max((dx * dx + dy * dy) ** 0.5, 1e-6)
        ux, uy = dx / norm, dy / norm
        for step in range(40):
            box = court(fp)
            hit = [o for o in settled if overlaps(box, court(fps[o]))]
            if not hit:
                break
            fp.Move(pcbnew.VECTOR2I_MM(ux * 0.25, uy * 0.25))
        else:
            problems.append(f"{ref}: could not clear overlap near {anc}")
        settled.append(ref)

    shelves = {k: [v[0], v[1], 0.0] for k, v in REGIONS.items()}   # cursor x, cursor y, row height
    for p in PARTS:
        if p.ref in FIXED or p.ref in SATELLITES:
            continue
        fp = fps[p.ref]
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
        fp.Move(pcbnew.VECTOR2I_MM((OX + cx + w / 2) - pcbnew.ToMM(nb.GetCenter().x),
                                   (OY + cy + h / 2) - pcbnew.ToMM(nb.GetCenter().y)))
        shelves[p.block] = [cx + w, cy, max(rh, h)]

    # exposed/thermal pads of the power ICs: solid (not thermal-relief) connection to GND pours
    for ref in ("U2", "U3", "U4", "U5"):
        if ref in fps:
            fps[ref].SetLocalZoneConnection(pcbnew.ZONE_CONNECTION_FULL)

    # keep tracks and vias EDGE_KEEPOUT away from every board edge and slot
    def keepout(x0, y0, x1, y1):
        z = pcbnew.ZONE(board)
        z.SetIsRuleArea(True)
        z.SetDoNotAllowTracks(True); z.SetDoNotAllowVias(True)
        z.SetDoNotAllowPads(False); z.SetDoNotAllowFootprints(False); z.SetDoNotAllowCopperPour(False)
        z.SetLayerSet(pcbnew.LSET.AllCuMask())
        ol = z.Outline(); ol.NewOutline()
        for (x, y) in [(x0, y0), (x1, y0), (x1, y1), (x0, y1)]:
            ol.Append(MM(OX + x), MM(OY + y))
        board.Add(z)
    k, th = EDGE_KEEPOUT, H_MAIN + H_TAB
    keepout(0, 0, W, k); keepout(0, th - k, W, th); keepout(0, 0, k, th); keepout(W - k, 0, W, th)
    for x0, x1 in SLOTS:
        keepout(x0 - k, H_MAIN - k, x1 + k, H_MAIN + SLOT_W + k)

    # USB-C orientation joins (fixed, locked): D- pads B7/A7 joined by an F.Cu loop around A6,
    # D+ pads A6/B6 joined through two vias and a B.Cu jumper under the loop.
    def trk(net, layer, pts, w=0.15):
        for (xa, ya), (xb, yb) in zip(pts, pts[1:]):
            t = pcbnew.PCB_TRACK(board)
            t.SetStart(pcbnew.VECTOR2I_MM(xa, ya)); t.SetEnd(pcbnew.VECTOR2I_MM(xb, yb))
            t.SetLayer(layer); t.SetWidth(MM(w)); t.SetNet(nets[net]); t.SetLocked(True)
            board.Add(t)

    def via(net, x, y, d=0.45, drill=0.2):
        v = pcbnew.PCB_VIA(board)
        v.SetPosition(pcbnew.VECTOR2I_MM(x, y)); v.SetWidth(MM(d)); v.SetDrill(MM(drill))
        v.SetNet(nets[net]); v.SetLocked(True)
        board.Add(v)

    j1 = {p.GetNumber(): p for p in fps["J1"].Pads()}
    def pend(num):   # inner end (toward the board) of a J1 pad, in mm
        bb = j1[num].GetBoundingBox()
        return pcbnew.ToMM(bb.GetRight()), pcbnew.ToMM(j1[num].GetPosition().y)
    (xb7, yb7), (xa7, ya7), (xa6, ya6), (xb6, yb6) = pend("B7"), pend("A7"), pend("A6"), pend("B6")
    xl = xb7 + 1.45                                  # D- loop return column
    trk("/USB_DN", pcbnew.F_Cu, [(xb7, yb7), (xl, yb7), (xl, ya7), (xa7, ya7)])
    xv = xa6 + 0.55                                   # D+ via column, inside the D- loop
    trk("/USB_DP", pcbnew.F_Cu, [(xa6, ya6), (xv, ya6)])
    trk("/USB_DP", pcbnew.F_Cu, [(xb6, yb6), (xv, yb6)])
    via("/USB_DP", xv, ya6); via("/USB_DP", xv, yb6)
    trk("/USB_DP", pcbnew.B_Cu, [(xv, ya6), (xv, yb6)])

    # stackup: F.Cu signals/parts | In1 solid GND reference | In2 power + signals | B.Cu signals
    for layer, prio in ((pcbnew.In1_Cu, 0),):
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
    power = dict(default, name="Power", clearance=0.127, track_width=0.3, via_diameter=0.8, via_drill=0.4, priority=0)
    ns["classes"] = [default, power]
    ns["netclass_patterns"] = [{"netclass": "Power", "pattern": pat} for pat in
                               ("/VBUS*", "/VSYS", "/VBAT", "/+3V3", "/+5V*", "/BB_L*", "/HB_L", "/GND", "/PWR_EN")]
    json.dump(d, open(pro, "w"), indent=2)


if __name__ == "__main__":
    main()
