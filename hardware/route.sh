#!/bin/sh
# Autoroute the placed board and produce the routed, filled board.
#   FREEROUTING_JAR: path to freerouting-1.9.0.jar (needs Java 17+; run under Xvfb because 1.9.0 needs a display)
# Steps: export DSN -> Freerouting pass 1 -> import -> export DSN with routes -> Freerouting passes 2 and 3
#        (finish leftovers) -> import -> GND pours on F/B -> fill -> DRC with schematic parity.
# Output: radio-thing.kicad_pcb is overwritten with the routed board (the unrouted one is kept as placed.kicad_pcb).
set -e
cd "$(dirname "$0")"
JAR="${FREEROUTING_JAR:?set FREEROUTING_JAR=/path/to/freerouting-1.9.0.jar}"
work=$(mktemp -d)
cp radio-thing.kicad_pcb placed.kicad_pcb
cp radio-thing.kicad_pro placed.kicad_pro
python3 -c "import pcbnew; b=pcbnew.LoadBoard('placed.kicad_pcb'); assert pcbnew.ExportSpecctraDSN(b, '$work/p1.dsn')"
xvfb-run -a java -Xmx8g -jar "$JAR" -de "$work/p1.dsn" -do "$work/p1.ses" -mp 60 -mt 1 > reports/freerouting-pass1.log 2>&1
python3 -c "
import pcbnew
b = pcbnew.LoadBoard('placed.kicad_pcb')
assert pcbnew.ImportSpecctraSES(b, '$work/p1.ses')
assert pcbnew.ExportSpecctraDSN(b, '$work/p2.dsn')"
xvfb-run -a java -Xmx8g -jar "$JAR" -de "$work/p2.dsn" -do "$work/p2.ses" -mp 40 -mt 1 > reports/freerouting-pass2.log 2>&1
# pass 3: one more clean-up pass for anything pass 2 left (the router is not deterministic)
python3 -c "
import pcbnew
b = pcbnew.LoadBoard('placed.kicad_pcb')
assert pcbnew.ImportSpecctraSES(b, '$work/p2.ses')
assert pcbnew.ExportSpecctraDSN(b, '$work/p3.dsn')"
timeout 300 xvfb-run -a java -Xmx8g -jar "$JAR" -de "$work/p3.dsn" -do "$work/p3.ses" -mp 40 -mt 1 > reports/freerouting-pass3.log 2>&1 || true
final="$work/p3.ses"; [ -s "$final" ] || final="$work/p2.ses"     # pass 3 occasionally stalls: fall back to pass 2
python3 tools/finish_pcb.py placed.kicad_pcb "$final" radio-thing.kicad_pcb
kicad-cli pcb drc --schematic-parity --severity-all -o reports/drc.rpt radio-thing.kicad_pcb
echo "--- DRC:"; grep -E "^\[" reports/drc.rpt | sed 's/\]:.*/]/' | sort | uniq -c || true
rm -rf "$work"
