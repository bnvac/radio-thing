"""Silkscreen tidy-up for the dense board (applied after routing; does not touch copper).

- Small passives (R, C, L, D, FB, TP, Q) lose their silkscreen reference; it stays on the Fab layer
  for the assembly drawing.
- Remaining references are shrunk to 0.8 mm and moved off pads/other text if they collide.
"""
import pcbnew

PASSIVE_PREFIXES = ("R", "C", "L", "D", "FB", "TP", "Q")
MM = pcbnew.FromMM


def _prefix(ref):
    return "".join(ch for ch in ref if ch.isalpha())


def cleanup(board):
    fps = list(board.GetFootprints())
    pads = [p.GetBoundingBox() for fp in fps for p in fp.Pads()]
    placed_text = []
    for fp in fps:
        ref = fp.Reference()
        if _prefix(fp.GetReference()) in PASSIVE_PREFIXES:
            ref.SetVisible(False)
            continue
        ref.SetTextSize(pcbnew.VECTOR2I_MM(0.8, 0.8))
        ref.SetTextThickness(MM(0.12))
        # try a few spots around the footprint; keep the first that clears pads and earlier text
        bb = fp.GetBoundingBox(False, False)
        c = bb.GetCenter()
        spots = [(c.x, bb.GetTop() - MM(0.7)), (c.x, bb.GetBottom() + MM(0.7)),
                 (bb.GetLeft() - MM(1.5), c.y), (bb.GetRight() + MM(1.5), c.y), (c.x, c.y)]
        for x, y in spots:
            ref.SetPosition(pcbnew.VECTOR2I(int(x), int(y)))
            tb = ref.GetBoundingBox()
            if not any(tb.Intersects(p) for p in pads) and not any(tb.Intersects(t) for t in placed_text):
                placed_text.append(tb)
                break
        else:
            ref.SetVisible(False)   # no clear spot: leave it to the Fab/assembly layer
