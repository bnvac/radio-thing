// Radio Thing - DISPLAY unit enclosure (rev A board) + 3 button caps.
// OpenSCAD 2021+.  Units: mm.
//
// Parts (pick with -D 'part="..."'):
//   "front"    front shell: screen window, button holes, display bosses, side openings   (print face-down)
//   "lid"      back lid: board posts, battery pocket, 1/4"-20 tripod boss, strap slot      (print outside-down)
//   "caps"     3 button caps (UP / OK / DOWN) laid out for printing                       (print head-down)
//   "assembly" exploded preview with placeholder board, display and battery (not for printing)
//
// Coordinates: X right, Y up, Z = depth from the FRONT outer face going backwards (into the case).
// Board positions are given in KiCad board coordinates (origin top-left, y down) and mapped with bx()/by().
//
// MEASURE YOUR PARTS and adjust the values marked [MEASURE] before printing.

part = "assembly";
$fn = 64;

// ---------------------------------------------------------------- board (must match hardware/tools/gen_pcb.py)
BOARD_W = 82;  BOARD_H = 72;  BOARD_T = 1.6;
BTN_CX  = 48.8;                  // board x that sits on the case centre line (gen_pcb.py BTN_CX)
BTN_Y   = 61;  BTN_PITCH = 17.5; // button row (board y) and pitch
BOARD_HOLES = [[3.5,3.5],[78.5,3.5],[3.5,46.5],[78.5,46.5],[3.5,68.5],[78.5,68.5]];
USB_Y   = 35.5;                  // J1 USB-C centre (board y), left edge
SW1_Y   = 12.0;                  // power slide switch (board y), left edge
SMA_X   = 58.0;                  // SMA bulkhead position along the top wall (board x) - near U6's U.FL

// ---------------------------------------------------------------- display: lcdwiki MSP3525 (ST7796, 3.5")
DISP_W = 98.0;  DISP_H = 55.5;   // module PCB outline
DISP_T = 5.2;                    // [MEASURE] glass front face -> module PCB back (without header pins)
DISP_GLASS_T = 3.6;              // [MEASURE] glass+backlight thickness in front of the module PCB
AA_W = 73.44;  AA_H = 48.96;     // active area
AA_DX = 0;  AA_DY = 0;           // [MEASURE] active-area centre offset from module centre (+x = toward header)
DISP_HOLE_DX = 92.0/2;  DISP_HOLE_DY = 49.5/2;   // module mounting holes from module centre
DISP_HOLE_PILOT = 2.5;           // pilot for M3 self-tapping screw (or 2.0 for M2.5)

// ---------------------------------------------------------------- stack-up (depths from front outer face)
FRONT_T  = 2.0;                  // front wall
WALL     = 2.4;                  // side walls
GAP_BEHIND_DISP = 12.0;          // room for the display header/ribbon and board top-side parts (J3 = 8.5 mm)
BATT_T   = 10.0;                 // [MEASURE] LiPo thickness (+ wiring) behind the board
LID_T    = 2.0;
SW_H     = 7.3;                  // [MEASURE] 12x12 tactile switch height incl. plunger (e.g. B3F-4055 = 7.3)
CORNER_R = 4.0;
CLR      = 0.3;                  // general print clearance

DISP_Z0      = FRONT_T;                         // glass face
DISP_Z1      = FRONT_T + DISP_T;                // module back
BOARD_FRONT_Z = DISP_Z1 + GAP_BEHIND_DISP;      // board top (component side, faces the screen)
BOARD_BACK_Z  = BOARD_FRONT_Z + BOARD_T;
LID_IN_Z      = BOARD_BACK_Z + BATT_T + 0.5;    // inside face of the lid
TOTAL_D       = LID_IN_Z + LID_T;

// ---------------------------------------------------------------- inside outline (case coordinates)
IN_X = 49.5;                     // inside half-width (display 98 + clearance; USB-C reaches the left wall)
IN_TOP = 43.75;  IN_BOT = -43.75;
function bx(x) = x - BTN_CX;
function by(y) = 35.75 - y;      // board top edge sits 8 mm below the inside top (room for lid screw columns)
DISP_CY = by(25.25);             // display module centre: its bottom edge clears the button caps

OUT_W = 2*(IN_X + WALL);  OUT_H = (IN_TOP - IN_BOT) + 2*WALL;
OUT_CY = (IN_TOP + IN_BOT)/2;

BTN_X = [bx(BTN_CX - BTN_PITCH), bx(BTN_CX), bx(BTN_CX + BTN_PITCH)];
BTN_CY = by(BTN_Y);

// lid screw columns: top pair behind the display, bottom pair below the board
COLS = [[-(IN_X-3.5), IN_TOP-3.5], [IN_X-3.5, IN_TOP-3.5], [-(IN_X-3.5), IN_BOT+3.5], [IN_X-3.5, IN_BOT+3.5]];
COL_D = 7.0;  COL_PILOT = 2.5;   // M3 self-tap (or 4.0 for M3 heat-set inserts)

// ---------------------------------------------------------------- button caps
CAP_HEAD_D = 11.6;  CAP_HOLE_D = 12.3;  CAP_PROTRUDE = 1.2;
CAP_FLANGE_D = 15.0;  CAP_FLANGE_T = 1.2;  CAP_STEM_D = 7.0;

// =================================================================== helpers
module rrect(w, h, r, t) {  // rounded rectangle slab, centred XY, from z=0 to t
    linear_extrude(t) offset(r) square([w - 2*r, h - 2*r], center = true);
}
module at_case(p) translate([p[0], p[1], 0]) children();

// =================================================================== front shell
module front_shell() {
    difference() {
        union() {
            // shell: front plate + side walls (open at the back)
            difference() {
                translate([0, OUT_CY, 0]) rrect(OUT_W, OUT_H, CORNER_R, LID_IN_Z);
                translate([0, OUT_CY, FRONT_T]) rrect(OUT_W - 2*WALL, OUT_H - 2*WALL, CORNER_R - WALL/2, LID_IN_Z);
            }
            // display bosses (display PCB screws in from behind)
            for (sx = [-1, 1], sy = [-1, 1])
                translate([sx*DISP_HOLE_DX, DISP_CY + sy*DISP_HOLE_DY, FRONT_T - 0.01])
                    difference() { cylinder(d = 6, h = DISP_GLASS_T); cylinder(d = DISP_HOLE_PILOT, h = DISP_GLASS_T + 1); }
            // button cap centring rings
            for (x = BTN_X) translate([x, BTN_CY, FRONT_T - 0.01])
                difference() { cylinder(d = CAP_FLANGE_D + 0.6 + 2.4, h = 1.6); cylinder(d = CAP_FLANGE_D + 0.6, h = 2); }
            // lid screw columns (top two start behind the display)
            for (i = [0:3]) {
                c = COLS[i]; z0 = (i < 2) ? DISP_Z1 + 0.5 : FRONT_T - 0.01;
                translate([c[0], c[1], z0]) difference() {
                    cylinder(d = COL_D, h = LID_IN_Z - z0);
                    translate([0, 0, LID_IN_Z - z0 - 10]) cylinder(d = COL_PILOT, h = 11);
                }
            }
        }
        // screen window with a 45 degree bevel on the outside
        translate([AA_DX, DISP_CY + AA_DY, -0.01]) hull() {
            translate([0, 0, FRONT_T + 0.5]) cube([AA_W + 1, AA_H + 1, 1.0], center = true);
            cube([AA_W + 1 + 2*FRONT_T, AA_H + 1 + 2*FRONT_T, 0.02], center = true);
        }
        // button holes
        for (x = BTN_X) translate([x, BTN_CY, -1]) cylinder(d = CAP_HOLE_D, h = FRONT_T + 2);
        // USB-C (left wall) - plug overmold needs ~12.5 x 6.5
        translate([-IN_X - WALL - 1, by(USB_Y), BOARD_FRONT_Z - 1.6])
            rotate([0, 90, 0]) hull() for (s = [-1, 1]) translate([0, s*(9.6 - 3.8)/2, 0]) cylinder(d = 3.8, h = WALL + 2);
        translate([-IN_X - WALL - 0.01, by(USB_Y), BOARD_FRONT_Z - 1.6])   // overmold counterbore
            rotate([0, 90, 0]) hull() for (s = [-1, 1]) translate([0, s*(12.5 - 6.5)/2, 0]) cylinder(d = 6.5, h = 1.2);
        // power slide switch (left wall)
        translate([-IN_X - WALL - 1, by(SW1_Y) - 4, BOARD_FRONT_Z - 3.0]) cube([WALL + 2, 8, 3.2]);
        // SMA bulkhead (top wall), D-flat 6.4 mm
        translate([bx(SMA_X), IN_TOP - 1, DISP_Z1 + 7]) rotate([-90, 0, 0])
            intersection() { cylinder(d = 6.6, h = WALL + 2); translate([-5, -2.9, 0]) cube([10, 5.8, WALL + 2]); }
    }
}

// =================================================================== back lid
module lid() {
    difference() {
        union() {
            translate([0, OUT_CY, LID_IN_Z]) rrect(OUT_W, OUT_H, CORNER_R, LID_T);
            // locating tongue that sits just inside the walls
            translate([0, OUT_CY, LID_IN_Z - 1.5]) difference() {
                rrect(OUT_W - 2*WALL - 2*CLR, OUT_H - 2*WALL - 2*CLR, CORNER_R - WALL/2, 1.5);
                translate([0, 0, -1]) rrect(OUT_W - 2*WALL - 2*CLR - 2.4, OUT_H - 2*WALL - 2*CLR - 2.4, 1, 3.5);
            }
            // board posts: board screws in from the front (M3 x 6 self-tap, or M3 heat-set inserts: pilot 4.0)
            for (h = BOARD_HOLES) translate([bx(h[0]), by(h[1]), BOARD_BACK_Z])
                difference() { cylinder(d = 6.5, h = LID_IN_Z - BOARD_BACK_Z + 0.01); cylinder(d = 2.5, h = 8); }
            // battery pocket ribs (60 x 50 cell)
            translate([bx(41), by(36), LID_IN_Z - 3]) difference() {
                cube([64, 54, 6], center = true); cube([61, 51, 8], center = true);
                cube([70, 20, 8], center = true);   // gaps for the battery lead
            }
            // 1/4"-20 tripod boss (heat-set insert, 8.0 mm hole) in the free strip right of the board
            translate([bx(BOARD_W) + 8, 0, LID_IN_Z - 9]) cylinder(d = 13, h = 9.01);
        }
        // lid screws (countersunk M3)
        for (c = COLS) translate([c[0], c[1], LID_IN_Z - 2]) {
            cylinder(d = 3.4, h = LID_T + 4);
            translate([0, 0, 2 + LID_T - 1.7]) cylinder(d1 = 3.4, d2 = 6.6, h = 1.71);
        }
        // tripod insert hole
        translate([bx(BOARD_W) + 8, 0, LID_IN_Z - 10]) cylinder(d = 8.0, h = LID_T + 11);
        // wrist-strap slot (bottom edge)
        translate([0, IN_BOT + 4, LID_IN_Z - 1]) cube([14, 3, LID_T + 3], center = true);
        // vent / speaker holes over the buzzer area
        for (i = [0:4]) translate([bx(70) + i*2.4 - 4.8, by(10), LID_IN_Z - 1]) cylinder(d = 1.2, h = LID_T + 2);
    }
}

// =================================================================== button cap
// Head pokes through the front plate, flange stops it falling out, stem presses the switch plunger.
CAP_STEM_L = (BOARD_FRONT_Z - SW_H - 0.2) - (FRONT_T + CAP_FLANGE_T);
module button_cap(symbol = "OK") {
    difference() {
        union() {
            translate([0, 0, -CAP_PROTRUDE]) cylinder(d = CAP_HEAD_D, h = CAP_PROTRUDE + FRONT_T);   // head
            translate([0, 0, FRONT_T]) cylinder(d = CAP_FLANGE_D, h = CAP_FLANGE_T);              // flange
            translate([0, 0, FRONT_T + CAP_FLANGE_T]) cylinder(d = CAP_STEM_D, h = CAP_STEM_L);    // stem
        }
        // centring dimple for the switch plunger
        translate([0, 0, FRONT_T + CAP_FLANGE_T + CAP_STEM_L - 0.8]) cylinder(d = 4.0, h = 1);
        // engraved symbol on top (0.6 mm deep)
        translate([0, 0, -CAP_PROTRUDE - 0.01]) linear_extrude(0.6) mirror([1, 0, 0]) symbol2d(symbol);
    }
}
module symbol2d(s) {
    if (s == "UP")   polygon([[-3, -2], [3, -2], [0, 3]]);
    if (s == "DOWN") polygon([[-3, 2], [3, 2], [0, -3]]);
    if (s == "OK")   difference() { circle(d = 6); circle(d = 4); }
}

// =================================================================== placeholders (assembly preview)
module board_placeholder() {
    color("darkgreen") translate([bx(0), by(BOARD_H), BOARD_FRONT_Z]) cube([BOARD_W, BOARD_H, BOARD_T]);
    color("silver") translate([bx(52) - 9, by(18) - 9.6, BOARD_FRONT_Z - 3.2]) cube([18, 19.2, 3.2]);   // ESP32 module
    for (x = BTN_X) color("black") translate([x - 6, BTN_CY - 6, BOARD_FRONT_Z - SW_H]) cube([12, 12, SW_H]);
    color("silver") translate([bx(0) - 0.7, by(USB_Y) - 4.5, BOARD_FRONT_Z - 3.2]) cube([7.5, 9, 3.2]);  // USB-C
}
module display_placeholder() {
    color("royalblue") translate([-DISP_W/2, DISP_CY - DISP_H/2, DISP_Z1 - 1.6]) cube([DISP_W, DISP_H, 1.6]);
    color("black") translate([-84.96/2, DISP_CY - DISP_H/2, DISP_Z0]) cube([84.96, DISP_H, DISP_GLASS_T]);
}
module battery_placeholder() {
    color("orange") translate([bx(41) - 30, by(36) - 25, BOARD_BACK_Z + 0.3]) cube([60, 50, BATT_T - 1]);
}

// =================================================================== output
if (part == "front") rotate([0, 0, 0]) front_shell();                     // front face down on the bed
if (part == "lid")   translate([0, 0, LID_IN_Z + LID_T]) mirror([0, 0, 1]) lid();   // outside face down
if (part == "caps")  for (i = [0:2]) translate([i*20 - 20, 0, CAP_PROTRUDE]) button_cap(["UP", "OK", "DOWN"][i]);
if (part == "assembly") {
    explode = 25;
    mirror([0, 0, 1]) {   // flip so the screen faces the viewer (+Z)
        color("gray", 0.9) front_shell();
        translate([0, 0, explode * 0.4]) display_placeholder();
        translate([0, 0, explode * 1.0]) board_placeholder();
        for (i = [0:2]) translate([BTN_X[i], BTN_CY, explode * 0.2]) color("tomato") button_cap(["UP", "OK", "DOWN"][i]);
        translate([0, 0, explode * 1.6]) battery_placeholder();
        translate([0, 0, explode * 2.2]) color("dimgray", 0.9) lid();
    }
}
echo(str("Outer size: ", OUT_W, " x ", OUT_H, " x ", TOTAL_D, " mm;  cap stem ", CAP_STEM_L, " mm"));
