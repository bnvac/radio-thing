#!/bin/sh
# Regenerate + verify the whole hardware design.
#   KICAD_SYMBOL_DIR / KICAD_FOOTPRINT_DIR: KiCad 9 libraries (default: system install)
# Steps: schematic -> ERC -> netlist -> netlist/firmware cross-check -> PCB -> DRC (+ schematic parity) -> BOM/PDF
set -e
cd "$(dirname "$0")"
export KICAD_SYMBOL_DIR="${KICAD_SYMBOL_DIR:-/usr/share/kicad/symbols}"
export KICAD_FOOTPRINT_DIR="${KICAD_FOOTPRINT_DIR:-/usr/share/kicad/footprints}"
mkdir -p reports
python3 tools/gen_schematic.py
kicad-cli sch erc --severity-all -o reports/erc.rpt radio-thing.kicad_sch
kicad-cli sch export netlist -o netlist.net radio-thing.kicad_sch
python3 tools/check_netlist.py netlist.net
python3 tools/gen_pcb.py netlist.net
kicad-cli pcb drc --schematic-parity --severity-all -o reports/drc.rpt radio-thing.kicad_pcb
kicad-cli sch export bom --fields 'Reference,Value,Footprint,Variant,Note,${QUANTITY},${DNP}' \
  --labels 'Refs,Value,Footprint,Variant,Note,Qty,DNP' --group-by 'Value,Footprint,Variant' --ref-range-delimiter '' \
  -o bom.csv radio-thing.kicad_sch
kicad-cli sch export pdf -o radio-thing-schematic.pdf radio-thing.kicad_sch
echo "--- ERC:"; grep -E "^\[" reports/erc.rpt | sed 's/\]:.*/]/' | sort | uniq -c || true
echo "--- DRC:"; grep -E "^\[" reports/drc.rpt | sed 's/\]:.*/]/' | sort | uniq -c || true
