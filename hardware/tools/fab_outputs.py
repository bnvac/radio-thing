"""Manufacturing outputs for the routed board (JLCPCB-style).

usage: python3 fab_outputs.py        (run from hardware/, after route.sh)

Writes hardware/fab/:
  radio-thing-revA-gerbers.zip      Gerbers (4 layers, mask, paste, silk, outline) + Excellon drill
  bom-POD.csv / bom-DISPLAY.csv     Comment, Designator, Footprint, LCSC  (one file per build variant)
  cpl-POD.csv / cpl-DISPLAY.csv     Designator, Mid X, Mid Y, Layer, Rotation
"""
import csv, os, shutil, subprocess, sys, tempfile, zipfile

HERE = os.path.dirname(os.path.abspath(__file__))
HW = os.path.dirname(HERE)
sys.path.insert(0, HW)
from design import PARTS

BOARD = os.path.join(HW, "radio-thing.kicad_pcb")
FAB = os.path.join(HW, "fab")
LAYERS = "F.Cu,In1.Cu,In2.Cu,B.Cu,F.Paste,B.Paste,F.Silkscreen,B.Silkscreen,F.Mask,B.Mask,Edge.Cuts"
VARIANTS = {"POD": {"BOTH", "POD"}, "DISPLAY": {"BOTH", "DISP"}}
VALUE_OVERRIDES = {"DISPLAY": {"R7": "1k"}}          # 890 mA charge for the 2000 mAh display cell
NOT_ASSEMBLED = ("Mechanical:", "Connector:TestPoint")


def run(*args):
    subprocess.run(["kicad-cli", *args], check=True, stdout=subprocess.DEVNULL)


def gerbers():
    tmp = tempfile.mkdtemp()
    run("pcb", "export", "gerbers", "--layers", LAYERS, "--subtract-soldermask", "--no-x2", "--use-drill-file-origin", "-o", tmp + "/", BOARD)
    run("pcb", "export", "drill", "--format", "excellon", "--drill-origin", "plot", "--excellon-separate-th", "--generate-map",
        "--map-format", "gerberx2", "-o", tmp + "/", BOARD)
    out = os.path.join(FAB, "radio-thing-revA-gerbers.zip")
    with zipfile.ZipFile(out, "w", zipfile.ZIP_DEFLATED) as z:
        for f in sorted(os.listdir(tmp)):
            z.write(os.path.join(tmp, f), f)
    shutil.rmtree(tmp)
    return out


def positions():
    tmp = tempfile.mktemp(suffix=".csv")
    run("pcb", "export", "pos", "--format", "csv", "--units", "mm", "--side", "both", "--use-drill-file-origin",
        "-o", tmp, BOARD)
    rows = {r["Ref"]: r for r in csv.DictReader(open(tmp))}
    os.remove(tmp)
    return rows


def main():
    os.makedirs(FAB, exist_ok=True)
    print("wrote", gerbers())
    pos = positions()
    for variant, keep in VARIANTS.items():
        parts = [p for p in PARTS if p.variant in keep and not p.lib_id.startswith(NOT_ASSEMBLED)]
        groups = {}
        for p in parts:
            value = VALUE_OVERRIDES.get(variant, {}).get(p.ref, p.value)
            groups.setdefault((value, p.footprint.split(":")[1]), []).append(p.ref)
        with open(os.path.join(FAB, f"bom-{variant}.csv"), "w", newline="") as f:
            w = csv.writer(f)
            w.writerow(["Comment", "Designator", "Footprint", "LCSC"])
            for (value, fp), refs in sorted(groups.items(), key=lambda kv: kv[1][0]):
                w.writerow([value, ",".join(refs), fp, ""])
        with open(os.path.join(FAB, f"cpl-{variant}.csv"), "w", newline="") as f:
            w = csv.writer(f)
            w.writerow(["Designator", "Mid X", "Mid Y", "Layer", "Rotation"])
            for p in parts:
                r = pos.get(p.ref)
                if r:
                    w.writerow([p.ref, f"{float(r['PosX']):.3f}mm", f"{float(r['PosY']):.3f}mm",
                                "Top" if r["Side"] == "top" else "Bottom", f"{float(r['Rot']):.1f}"])
        print(f"wrote bom-{variant}.csv ({len(parts)} parts), cpl-{variant}.csv")


if __name__ == "__main__":
    main()
