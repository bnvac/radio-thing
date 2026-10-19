#!/bin/sh
# Render the board's front side to a PNG for quick review: render.sh <board.kicad_pcb> <out.png>
set -e
tmp=$(mktemp -d)
kicad-cli pcb export pdf -l F.Cu,F.Fab,F.CrtYd,Edge.Cuts,User.1 --sp -o "$tmp/b.pdf" "$1" >/dev/null
pdftoppm -r 200 -png -singlefile -x 740 -y 740 -W 700 -H 620 "$tmp/b.pdf" "${2%.png}"
rm -rf "$tmp"
