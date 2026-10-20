"""Verify KiCad's exported netlist matches design.py exactly, and design.py matches firmware pins."""
import os, re, sys
HERE = os.path.dirname(os.path.abspath(__file__)); HW = os.path.dirname(HERE)
sys.path.insert(0, HERE); sys.path.insert(0, HW)
from sexp import parse, find, first
from design import PARTS

net_path = sys.argv[1]
tree = parse(open(net_path).read())
got = {}
for net in find(first(tree, "nets"), "net"):
    name = first(net, "name")[1].lstrip("/")
    for node in find(net, "node"):
        got[(first(node, "ref")[1], first(node, "pin")[1])] = name
errors = 0
for p in PARTS:
    for pin, net in p.pins.items():
        g = got.get((p.ref, pin))
        if net is None:
            if g and not g.startswith("unconnected-"):
                print(f"NC mismatch {p.ref}.{pin}: got {g}"); errors += 1
        elif g != net:
            print(f"MISMATCH {p.ref}.{pin}: want {net} got {g}"); errors += 1

# firmware pin map cross-check: GPIO number -> net on U6
hdr = open(os.path.join(HW, "..", "firmware", "include", "board_pins.h")).read()
defs = {m[0]: int(m[1]) for m in re.findall(r"#define\s+(PIN_\w+)\s+(\d+)", hdr)}
u6 = next(p for p in PARTS if p.ref == "U6")
from klib import load, pins as kpins
gpio_to_pin = {}
for num, name, *_ in kpins(load("RF_Module", "ESP32-S3-WROOM-1")):
    m = re.fullmatch(r"IO(\d+)", name)
    if m: gpio_to_pin[int(m[1])] = num
gpio_to_pin.update({19: "13", 20: "14", 43: "37", 44: "36"})
expect = {"PIN_BOOT": "BOOT", "PIN_USB_DN": "USB_DN", "PIN_USB_DP": "USB_DP", "PIN_DBG_TX": "DBG_TX", "PIN_DBG_RX": "DBG_RX",
          "PIN_I2C_SDA": "I2C_SDA", "PIN_I2C_SCL": "I2C_SCL", "PIN_SPI_SCK": "SPI_SCK", "PIN_SPI_MOSI": "SPI_MOSI",
          "PIN_SPI_MISO": "SPI_MISO", "PIN_SPI_CS": "SPI_CS", "PIN_LCD_DC": "DC_IRQ", "PIN_DWM_IRQ": "DC_IRQ",
          "PIN_LCD_RST": "RST_SHARED", "PIN_DWM_RST": "RST_SHARED", "PIN_LCD_BL": "LCD_BL_CTL",
          "PIN_BTN_UP": "BTN_UP", "PIN_BTN_DOWN": "BTN_DOWN", "PIN_BTN_OK": "BTN_OK", "PIN_BUZZER": "BUZZER", "PIN_LED_R": "LED_R", "PIN_LED_G": "LED_G", "PIN_LED_B": "LED_B",
          "PIN_CHG_N": "CHG_N", "PIN_HEAD_EN": "HEAD_EN", "PIN_HEADA_TX": "HEADA_TX", "PIN_HEADA_RX": "HEADA_RX",
          "PIN_HEADB_TX": "HEADB_TX", "PIN_HEADB_RX": "HEADB_RX", "PIN_HEAD_ID": "HEAD_ID"}
assert set(expect) == set(defs), set(defs) ^ set(expect)
for d, gpio in defs.items():
    net = u6.pins.get(gpio_to_pin[gpio])
    if net != expect[d]:
        print(f"FIRMWARE MISMATCH {d}=GPIO{gpio}: board net {net}, expected {expect[d]}"); errors += 1
print(f"checked {sum(len(p.pins) for p in PARTS)} pin assignments + {len(defs)} firmware pins: {errors} errors")
sys.exit(1 if errors else 0)
