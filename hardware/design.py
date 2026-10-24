"""Radio Thing main board rev A — the circuit, as data.

Single source of truth for the schematic and PCB generators. Each part:
    P(ref, "Lib:Symbol", value, footprint, {pin_number: net}, block, variant)
Nets are plain strings; None marks an intentional no-connect.
Pin map must match firmware/include/board_pins.h.

variant: "BOTH" (fitted on every board), "POD", "DISP", or "OPT" (optional, not fitted by default).
"""
from dataclasses import dataclass, field

NC = None


@dataclass
class P:
    ref: str
    lib_id: str
    value: str
    footprint: str
    pins: dict
    block: str
    variant: str = "BOTH"
    note: str = ""
    lcsc: str = ""


R0402 = "Resistor_SMD:R_0402_1005Metric"
C0402 = "Capacitor_SMD:C_0402_1005Metric"
C0603 = "Capacitor_SMD:C_0603_1608Metric"
C0805 = "Capacitor_SMD:C_0805_2012Metric"
C1206 = "Capacitor_SMD:C_1206_3216Metric"
LED0603 = "LED_SMD:LED_0603_1608Metric"
SOT23 = "Package_TO_SOT_SMD:SOT-23"


def R(ref, val, a, b, block, variant="BOTH", note=""):
    return P(ref, "Device:R", val, R0402, {"1": a, "2": b}, block, variant, note)


def C(ref, val, a, b, block, fp=C0402, variant="BOTH", note=""):
    return P(ref, "Device:C", val, fp, {"1": a, "2": b}, block, variant, note)


PARTS = [
    # ------------------------------------------------------------------ USB-C input
    P("J1", "Connector:USB_C_Receptacle_USB2.0_16P", "USB-C", "Connector_USB:USB_C_Receptacle_HRO_TYPE-C-31-M-12",
      {"A1": "GND", "B1": "GND", "A12": "GND", "B12": "GND", "S1": "GND",
       "A4": "VBUS_C", "A9": "VBUS_C", "B4": "VBUS_C", "B9": "VBUS_C",
       "A5": "CC1", "B5": "CC2", "A6": "USB_DP", "B6": "USB_DP", "A7": "USB_DN", "B7": "USB_DN",
       "A8": NC, "B8": NC}, "USB"),
    R("R1", "5.1k", "CC1", "GND", "USB", note="USB-C sink pull-down"),
    R("R2", "5.1k", "CC2", "GND", "USB", note="USB-C sink pull-down"),
    P("F1", "Device:Polyfuse_Small", "1.5A hold", "Fuse:Fuse_1206_3216Metric", {"1": "VBUS_C", "2": "VBUS"}, "USB"),
    P("U1", "Power_Protection:USBLC6-2SC6", "USBLC6-2SC6", "Package_TO_SOT_SMD:SOT-23-6",
      {"1": "USB_DP", "6": "USB_DP", "3": "USB_DN", "4": "USB_DN", "2": "GND", "5": "VBUS"}, "USB"),

    # ------------------------------------------------------------------ Charger + power path
    P("U2", "Battery_Management:BQ24074RGT", "BQ24074RGT", "Package_DFN_QFN:VQFN-16-1EP_3x3mm_P0.5mm_EP1.6x1.6mm",
      {"13": "VBUS", "10": "VSYS", "11": "VSYS", "2": "VBAT", "3": "VBAT", "8": "GND", "17": "GND",
       "1": "BQ_TS", "4": "GND", "5": "VSYS", "6": "GND", "7": NC, "9": "CHG_N",
       "12": "BQ_ILIM", "14": "BQ_TMR", "15": "BQ_ITERM", "16": "BQ_ISET"}, "CHARGER",
      note="EN2=1, EN1=0 -> input current limit set by R_ILIM; /CE=0 charge enabled"),
    C("C1", "1u", "VBUS", "GND", "CHARGER", C0603),
    C("C2", "4.7u", "VSYS", "GND", "CHARGER", C0603),
    C("C3", "22u", "VSYS", "GND", "CHARGER", C0805),
    C("C4", "4.7u", "VBAT", "GND", "CHARGER", C0603),
    R("R3", "10k", "BQ_TS", "GND", "CHARGER", note="no NTC: fixed 10k = normal temp. Fit 10k NTC in cell instead if pack has one"),
    R("R4", "1.78k", "BQ_ILIM", "GND", "CHARGER", note="ILIM = 1610/R ~= 0.9 A input limit (USB-C default 900 mA). BQ2407x DS SLUS810N eq.1"),
    R("R5", "46.4k", "BQ_TMR", "GND", "CHARGER", note="fast-charge timer ~6.2 h, pre-charge ~37 min (DS 9.3.5.6)"),
    R("R6", "3.0k", "BQ_ITERM", "GND", "CHARGER", note="termination = 10% of ICHG for any RISET (0.03*R_ITERM/R_ISET); 15k would stop at 50%"),
    R("R7", "2k", "BQ_ISET", "GND", "CHARGER", note="ISET = 890/R ~= 445 mA (POD 500 mAh). DISPLAY build: 1k -> ~890 mA for 2000 mAh"),
    R("R8", "10k", "CHG_N", "+3V3", "CHARGER", note="/CHG open-drain pull-up"),
    P("J2", "Connector_Generic:Conn_01x02", "LiPo 1S", "Connector_JST:JST_PH_S2B-PH-SM4-TB_1x02-1MP_P2.00mm_Horizontal",
      {"1": "VBAT", "2": "GND"}, "CHARGER", note="CHECK battery lead polarity - vendors differ"),

    # ------------------------------------------------------------------ Fuel gauge
    P("U3", "RadioThing:MAX17048", "MAX17048G+T10", "Package_DFN_QFN:TDFN-8-1EP_2x2mm_P0.5mm_EP0.8x1.2mm",
      {"1": "GND", "2": "VBAT", "3": "VBAT", "4": "GND", "5": NC, "6": "GND", "7": "I2C_SCL", "8": "I2C_SDA", "9": "GND"},
      "FUEL", note="CTG and QSTRT tied to GND per datasheet; ALRT unused (polled)"),
    C("C5", "100n", "VBAT", "GND", "FUEL", note="MAX17048 VDD bypass 0.1 uF per datasheet"),

    # ------------------------------------------------------------------ Power switch + 3V3 buck-boost
    P("SW1", "Switch:SW_SPDT", "PWR", "Button_Switch_SMD:SW_SPDT_PCM12",
      {"1": "VSYS", "2": "PWR_EN", "3": "GND"}, "3V3", note="off = 3V3 rail off; battery still charges"),
    P("U4", "Regulator_Switching:TPS63001", "TPS63001", "Package_SON:Texas_DRC0010J_ThermalVias",
      {"5": "VSYS", "8": "BB_VINA", "4": "BB_L1", "2": "BB_L2", "1": "+3V3", "10": "+3V3",
       "3": "GND", "9": "GND", "11": "GND", "6": "PWR_EN", "7": "VSYS"}, "3V3",
      note="PS/SYNC high = forced PWM (lower, predictable ripple for the radar)"),
    P("L1", "Device:L", "2.2uH Isat>=2A low DCR", "Inductor_SMD:L_Taiyo-Yuden_NR-40xx", {"1": "BB_L1", "2": "BB_L2"}, "3V3",
      note="TPS6300x DS Table 1 uses 2.2 uH (VLF4012-2R2)"),
    C("C6", "10u", "VSYS", "GND", "3V3", C0805),
    R("R24", "100", "VSYS", "BB_VINA", "3V3", note="VINA RC filter per TPS6300x reference design"),
    C("C7", "100n", "BB_VINA", "GND", "3V3", note="VINA filter cap"),
    C("C8", "22u", "+3V3", "GND", "3V3", C0805),
    C("C9", "22u", "+3V3", "GND", "3V3", C0805),

    # ------------------------------------------------------------------ 5 V radar-head boost (POD only)
    P("U5", "Regulator_Switching:TPS61202DRC", "TPS61202", "Package_SON:Texas_S-PVSON-N10_ThermalVias",
      {"5": "VSYS", "7": "VSYS", "3": "HB_L", "2": "+5V_RAW", "10": "+5V_RAW", "1": "HB_VAUX",
       "6": "HEAD_EN", "8": "VSYS", "4": "GND", "9": "GND", "11": "GND"}, "5V", "POD",
      note="fixed 5 V; UVLO tied to VIN (unused); PS high (verify PS polarity in datasheet)"),
    P("L2", "Device:L", "2.2uH 2A", "Inductor_SMD:L_Taiyo-Yuden_NR-40xx", {"1": "VSYS", "2": "HB_L"}, "5V", "POD"),
    C("C10", "10u", "VSYS", "GND", "5V", C0805, "POD"),
    C("C11", "22u", "+5V_RAW", "GND", "5V", C0805, "POD", note="DS min ~5*L uF = 11 uF before DC-bias derating"),
    C("C12", "100n", "HB_VAUX", "GND", "5V", variant="POD"),
    R("R9", "100k", "HEAD_EN", "GND", "5V", "POD", note="head rail off until firmware enables it"),
    P("FB1", "Device:FerriteBead_Small", "600R@100MHz 1A", "Inductor_SMD:L_0805_2012Metric",
      {"1": "+5V_RAW", "2": "+5V_HEAD"}, "5V", "POD", note="LC filter: keep boost ripple out of the radar"),
    C("C13", "47u", "+5V_HEAD", "GND", "5V", C1206, "POD"),
    C("C14", "100n", "+5V_HEAD", "GND", "5V", variant="POD"),

    # ------------------------------------------------------------------ ESP32-S3
    P("U6", "RF_Module:ESP32-S3-WROOM-1", "ESP32-S3-WROOM-1U-N8R8", "RF_Module:ESP32-S3-WROOM-1U",
      {"1": "GND", "40": "GND", "41": "GND", "2": "+3V3", "3": "ESP_EN",
       "27": "BOOT", "13": "USB_DN", "14": "USB_DP", "37": "DBG_TX", "36": "DBG_RX",
       "12": "I2C_SDA", "11": "I2C_SCL",
       "20": "SPI_SCK", "19": "SPI_MOSI", "21": "SPI_MISO", "18": "SPI_CS", "17": "DC_IRQ", "22": "RST_SHARED",
       "23": "LCD_BL_CTL",
       "4": "BTN_UP", "5": "BTN_DOWN", "6": "BTN_OK", "7": "SPARE_IO7", "8": "SPARE_IO15",
       "9": "BUZZER", "31": "LED_R", "24": "LED_G", "25": "LED_B", "10": "CHG_N",
       "34": "HEAD_EN", "32": "HEADA_TX", "33": "HEADA_RX", "35": "HEADB_TX", "38": "HEADB_RX", "39": "HEAD_ID",
       "15": NC, "16": NC, "26": NC, "28": NC, "29": NC, "30": NC}, "MCU",
      note="-1U variant: U.FL antenna connector -> SMA bulkhead pigtail. IO35-37 used by octal PSRAM"),
    R("R10", "10k", "ESP_EN", "+3V3", "MCU"),
    C("C15", "1u", "ESP_EN", "GND", "MCU", note="EN RC delay"),
    R("R11", "10k", "BOOT", "+3V3", "MCU"),
    C("C16", "22u", "+3V3", "GND", "MCU", C0805),
    C("C17", "100n", "+3V3", "GND", "MCU"),
    P("SW2", "Switch:SW_Push", "RESET", "Button_Switch_SMD:SW_Push_SPST_NO_Alps_SKRK", {"1": "ESP_EN", "2": "GND"}, "MCU"),
    P("SW3", "Switch:SW_Push", "BOOT", "Button_Switch_SMD:SW_Push_SPST_NO_Alps_SKRK", {"1": "BOOT", "2": "GND"}, "MCU"),
    P("J5", "Connector_Generic:Conn_01x04", "DEBUG UART", "Connector_PinHeader_2.54mm:PinHeader_1x04_P2.54mm_Vertical",
      {"1": "+3V3", "2": "DBG_TX", "3": "DBG_RX", "4": "GND"}, "MCU"),

    # ------------------------------------------------------------------ Sensors (I2C)
    R("R12", "4.7k", "I2C_SDA", "+3V3", "SENSORS"),
    R("R13", "4.7k", "I2C_SCL", "+3V3", "SENSORS"),
    P("U7", "Sensor_Motion:ISM330DHCX", "ISM330DHCX", "Package_LGA:LGA-14_3x2.5mm_P0.5mm_LayoutBorder3x4y",
      {"13": "I2C_SCL", "14": "I2C_SDA", "12": "+3V3", "1": "IMU_SA0", "5": "+3V3", "8": "+3V3", "6": "GND", "7": "GND",
       "2": "GND", "3": "GND", "4": NC, "9": NC, "10": NC, "11": NC}, "SENSORS",
      note="I2C addr 0x6A (SA0=0). Aux SDx/SCx tied to GND when unused (verify)"),
    R("R22", "10k", "IMU_SA0", "GND", "SENSORS", note="SA0 low -> I2C addr 0x6A"),
    C("C18", "100n", "+3V3", "GND", "SENSORS"),
    C("C19", "100n", "+3V3", "GND", "SENSORS"),
    P("U8", "Sensor_Magnetic:LIS2MDL", "LIS2MDL", "Package_LGA:LGA-12_2x2mm_P0.5mm",
      {"1": "I2C_SCL", "4": "I2C_SDA", "3": "+3V3", "5": "MAG_C1", "9": "+3V3", "10": "+3V3", "6": "GND", "8": "GND",
       "7": NC, "2": NC, "11": NC, "12": NC}, "SENSORS", note="I2C addr 0x1E"),
    C("C20", "220n", "MAG_C1", "GND", "SENSORS", note="LIS2MDL C1 = 220 nF, low ESR, right at pins 5/6 (DS Table 2)"),
    C("C21", "100n", "+3V3", "GND", "SENSORS"),
    C("C25", "10u", "+3V3", "GND", "SENSORS", C0603, note="LIS2MDL recommends 100 nF + 10 uF on Vdd"),

    # ------------------------------------------------------------------ Display (DISPLAY build)
    P("J3", "Connector_Generic:Conn_01x14", "ST7796 3.5in SPI", "Connector_PinSocket_2.54mm:PinSocket_1x14_P2.54mm_Vertical",
      {"1": "PWR_EN", "2": "GND", "3": "SPI_CS", "4": "RST_SHARED", "5": "DC_IRQ", "6": "SPI_MOSI", "7": "SPI_SCK",
       "8": "LCD_LED", "9": "SPI_MISO", "10": NC, "11": NC, "12": NC, "13": NC, "14": "LCD_SD_CS"}, "DISPLAY", "DISP",
      note="lcdwiki MSP3525/MSP3526 (ST7796) pinout. VCC feeds the module's own 3.3 V LDO, so it takes switched VSYS (PWR_EN, after SW1) - fully off when switched off. Pins 10-13 = CTP (unused)"),
    R("R14", "10k", "LCD_SD_CS", "+3V3", "DISPLAY", "DISP", note="keep the module's SD card deselected (it shares MISO)"),
    R("R15", "100", "LCD_BL_CTL", "LCD_LED", "DISPLAY", "DISP",
      note="LED pin is a logic input (onboard BSS138 + 10k pull-up): high/open = backlight on, low = off; PWM ok"),

    # (Optional DWM3000 pod-to-pod UWB ranging removed from rev A: its antenna keep-out blocked routing.
    #  Planned for rev B - see hardware/README.md.)

    # ------------------------------------------------------------------ Buttons: 3, centred under the screen (active low)
    P("SW4", "Switch:SW_Push", "UP", "Button_Switch_THT:SW_PUSH-12mm", {"1": "BTN_UP", "2": "GND"}, "UI", "DISP",
      note="short = up/zoom in, hold = cycle mode"),
    P("SW6", "Switch:SW_Push", "OK", "Button_Switch_THT:SW_PUSH-12mm", {"1": "BTN_OK", "2": "GND"}, "UI",
      note="Fitted on both builds; pod's only button; deep-sleep wake. Hold 1.5 s = back, 3 s = sleep"),
    P("SW5", "Switch:SW_Push", "DOWN", "Button_Switch_THT:SW_PUSH-12mm", {"1": "BTN_DOWN", "2": "GND"}, "UI", "DISP",
      note="short = down/zoom out"),
    P("TP1", "Connector:TestPoint", "IO7", "TestPoint:TestPoint_Pad_D1.5mm", {"1": "SPARE_IO7"}, "UI", note="spare GPIO7"),
    P("TP2", "Connector:TestPoint", "IO15", "TestPoint:TestPoint_Pad_D1.5mm", {"1": "SPARE_IO15"}, "UI", note="spare GPIO15"),

    # ------------------------------------------------------------------ Buzzer + LEDs
    P("BZ1", "Device:Buzzer", "3V magnetic transducer ~3 kHz", "Buzzer_Beeper:MagneticBuzzer_Kingstate_KCG0601",
      {"1": "BZ_P", "2": "BUZ_D"}, "UI", "DISP", note="external-drive coil (~8.5 ohm), 2-4 V, <=150 mA mean: drive with ~2.7-3 kHz square wave"),
    R("R23", "15", "+3V3", "BZ_P", "UI", "DISP", note="limits coil current to ~140 mA peak / ~70 mA mean at 50% duty"),
    P("D1", "Device:D_Schottky", "BAT54", "Diode_SMD:D_SOD-123", {"1": "BZ_P", "2": "BUZ_D"}, "UI", "DISP",
      note="flyback across the coil"),
    P("Q3", "Transistor_FET:2N7002", "2N7002", SOT23, {"1": "BUZZER", "2": "GND", "3": "BUZ_D"}, "UI", "DISP"),
    R("R17", "100k", "BUZZER", "GND", "UI", "DISP"),
    R("R18", "1k", "LED_R", "LED_R_A", "UI"),
    R("R19", "1k", "LED_G", "LED_G_A", "UI"),
    R("R20", "1k", "LED_B", "LED_B_A", "UI"),
    P("D2", "Device:LED", "RED", LED0603, {"2": "LED_R_A", "1": "GND"}, "UI"),
    P("D3", "Device:LED", "GREEN", LED0603, {"2": "LED_G_A", "1": "GND"}, "UI"),
    P("D4", "Device:LED", "BLUE", LED0603, {"2": "LED_B_A", "1": "GND"}, "UI"),

    # ------------------------------------------------------------------ Radar head connector (POD)
    P("J4", "Connector_Generic:Conn_01x10", "RADAR HEAD", "Connector_JST:JST_GH_SM10B-GHS-TB_1x10-1MP_P1.25mm_Horizontal",
      {"1": "+5V_HEAD", "2": "+5V_HEAD", "3": "+3V3", "4": "GND", "5": "GND",
       "6": "HA_TX_C", "7": "HA_RX_C", "8": "HB_TX_C", "9": "HB_RX_C", "10": "HEAD_ID"}, "HEAD", "POD",
      note="Latching JST-GH; head ID resistor on head board to GND"),
    R("R21", "10k", "HEAD_ID", "+3V3", "HEAD", "POD"),
    R("R26", "100", "HEADA_TX", "HA_TX_C", "HEAD", "POD", note="series R: limits back-powering/ESD on the detachable head"),
    R("R27", "100", "HEADA_RX", "HA_RX_C", "HEAD", "POD"),
    R("R28", "100", "HEADB_TX", "HB_TX_C", "HEAD", "POD"),
    R("R29", "100", "HEADB_RX", "HB_RX_C", "HEAD", "POD"),
    C("C24", "100n", "HEAD_ID", "GND", "HEAD", variant="POD"),

    # ------------------------------------------------------------------ Mechanical
    P("H1", "Mechanical:MountingHole", "M3", "MountingHole:MountingHole_3.2mm_M3", {}, "MECH"),
    P("H2", "Mechanical:MountingHole", "M3", "MountingHole:MountingHole_3.2mm_M3", {}, "MECH"),
    P("H3", "Mechanical:MountingHole", "M3", "MountingHole:MountingHole_3.2mm_M3", {}, "MECH"),
    P("H4", "Mechanical:MountingHole", "M3", "MountingHole:MountingHole_3.2mm_M3", {}, "MECH"),
    P("H5", "Mechanical:MountingHole", "M3", "MountingHole:MountingHole_3.2mm_M3", {}, "MECH", "DISP",
      note="button tab support: screw the tab to the case so button presses don't stress the V-score"),
    P("H6", "Mechanical:MountingHole", "M3", "MountingHole:MountingHole_3.2mm_M3", {}, "MECH", "DISP",
      note="button tab support"),
]

BLOCK_TITLES = {
    "USB": "USB-C input + ESD", "CHARGER": "LiPo charger / power path (BQ24074)", "FUEL": "Fuel gauge (MAX17048)",
    "3V3": "Power switch + 3.3 V buck-boost (TPS63001)", "5V": "Radar head 5 V boost + LC filter [POD]",
    "MCU": "ESP32-S3-WROOM-1U", "SENSORS": "IMU + magnetometer (I2C)", "DISPLAY": "3.5in display + backlight [DISPLAY]",
"UI": "Buttons, buzzer, RGB LED", "HEAD": "Radar head connector [POD]",
    "MECH": "Mounting",
}

# Custom symbols not in the KiCad 9 library: name -> (left pins, right pins), pin = (number, name, type)
CUSTOM_SYMBOLS = {
    "MAX17048": {
        "ref": "U", "footprint": "Package_DFN_QFN:TDFN-8-1EP_2x2mm_P0.5mm_EP0.8x1.2mm",
        "datasheet": "https://www.analog.com/media/en/technical-documentation/data-sheets/MAX17048-MAX17049.pdf",
        "description": "1-cell LiPo fuel gauge, ModelGauge, I2C, TDFN-8 2x2 (pinout: verify against datasheet)",
        "left": [("3", "VDD", "power_in"), ("2", "CELL", "input"), ("1", "CTG", "input"), ("6", "QSTRT", "input")],
        "right": [("8", "SDA", "bidirectional"), ("7", "SCL", "input"), ("5", "~{ALRT}", "open_collector"),
                  ("4", "GND", "power_in"), ("9", "EP", "passive")],
    },
}
