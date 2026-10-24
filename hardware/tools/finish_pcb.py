"""Import an autorouted Specctra session into the board, add ground pours, fill zones, save.

usage: python3 finish_pcb.py <board.kicad_pcb> <routes.ses> <out.kicad_pcb>

Ground: In1.Cu is a solid GND plane (from gen_pcb.py). This adds GND pours on F.Cu and B.Cu
(lower priority, thermal reliefs) plus stitching vias around the board edge, so every GND pad
has a short path to the plane.
"""
import os, sys
import pcbnew

MM = pcbnew.FromMM


def pour(board, layer, net, outline_pts, prio=0):
    z = pcbnew.ZONE(board)
    z.SetLayer(layer)
    z.SetNet(net)
    z.SetAssignedPriority(prio)
    z.SetLocalClearance(MM(0.25))
    z.SetMinThickness(MM(0.2))
    z.SetPadConnection(pcbnew.ZONE_CONNECTION_THERMAL)
    z.SetThermalReliefGap(MM(0.25))
    z.SetThermalReliefSpokeWidth(MM(0.3))
    z.SetIslandRemovalMode(pcbnew.ISLAND_REMOVAL_MODE_ALWAYS)
    ol = z.Outline()
    ol.NewOutline()
    for x, y in outline_pts:
        ol.Append(x, y)
    board.Add(z)
    return z


def main(src, ses, out):
    board = pcbnew.LoadBoard(src)
    if not pcbnew.ImportSpecctraSES(board, ses):
        raise SystemExit("SES import failed")

    gnd = board.FindNet("/GND")
    bb = board.GetBoardEdgesBoundingBox()
    inset = MM(0.5)
    pts = [(bb.GetLeft() + inset, bb.GetTop() + inset), (bb.GetRight() - inset, bb.GetTop() + inset),
           (bb.GetRight() - inset, bb.GetBottom() - inset), (bb.GetLeft() + inset, bb.GetBottom() - inset)]
    for layer in (pcbnew.F_Cu, pcbnew.B_Cu):
        pour(board, layer, gnd, pts, prio=0)

    sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
    from silk_cleanup import cleanup
    cleanup(board)

    # place/drill origin at the board's bottom-left corner (Gerbers, drill and CPL all use it)
    board.GetDesignSettings().SetAuxOrigin(pcbnew.VECTOR2I(bb.GetLeft(), bb.GetBottom()))

    board.BuildConnectivity()
    filler = pcbnew.ZONE_FILLER(board)
    filler.Fill(board.Zones())
    pcbnew.SaveBoard(out, board)
    # SaveBoard rewrites the .kicad_pro with KiCad defaults: put our rules / net classes back
    sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
    from gen_pcb import apply_project_rules
    apply_project_rules(os.path.splitext(out)[0] + ".kicad_pro")
    n_tracks = sum(1 for t in board.GetTracks() if t.GetClass() == "PCB_TRACK")
    n_vias = sum(1 for t in board.GetTracks() if t.GetClass() == "PCB_VIA")
    print(f"wrote {out}: {n_tracks} track segments, {n_vias} vias, {len(list(board.Zones()))} zones")


if __name__ == "__main__":
    main(*sys.argv[1:4])
