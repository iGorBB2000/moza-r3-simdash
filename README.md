# [WIP] Simhub ESP32 Display Setup

A guide to building a simhub dashboard using an ESP32 and a small SPI TFT screen, driven by SimHub telemetry data.

---

## Hardware

- ESP32 DevKit (30 or 38 pin)
- SPI TFT display (ST7735 driver, 128×160px)

### Wiring

| Display Pin | ESP32 GPIO |
|---|---|
| VCC | 3.3V |
| GND | GND |
| BL | 3.3V (always on) or GPIO 32 |
| CLK | GPIO 18 |
| DIN | GPIO 23 |
| CS | GPIO 5 |
| D/C | GPIO 2 |
| RST | GPIO 4 |

---

## Software

### Arduino IDE Setup

1. Download and install [Arduino IDE 2.x](https://www.arduino.cc/en/software)
2. Add the ESP32 board support URL in **File → Preferences**:
   ```
   https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json
   ```
3. Install **esp32 by Espressif** via **Tools → Board → Boards Manager**
4. Select **ESP32 Dev Module** as your board

### Libraries

Install via **Sketch → Include Library → Manage Libraries**:

- `TFT_eSPI` by Bodmer

### TFT_eSPI Configuration

Edit `Documents/Arduino/libraries/TFT_eSPI/User_Setup.h` with the following:

```cpp
#define ST7735_DRIVER
#define ST7735_BLACKTAB

#define TFT_WIDTH  128
#define TFT_HEIGHT 160

#define TFT_MOSI  23   // DIN
#define TFT_SCLK  18   // CLK
#define TFT_CS     5   // CS
#define TFT_DC     2   // D/C
#define TFT_RST    4   // RST
#define TFT_BL    32   // Backlight

#define SPI_FREQUENCY  27000000
#define USE_HSPI_PORT
```

---

## ESP32 Sketch

See `dashboard.ino` in this repository. The sketch receives semicolon-delimited telemetry from SimHub over USB Serial and renders:

- Large gear indicator on the left, colour coded (white 1–6, blue N, red R)
- Speed in km/h on the right
- Segmented RPM bar across the bottom, scaling dynamically to the current car's max RPM

---

## SimHub Configuration

### 1. Enable Custom Serial Devices

In SimHub go to **Settings** and enable **Custom serial devices**.

### 2. Add Device

Add a new device and select your ESP32 COM port with baud rate **115200**.

### 3. Protocol Definition

| Field | Formula (NCalc) |
|---|---|
| Message after connect | `'CONNECT\n'` |
| Update message | `format([DataCorePlugin.GameData.Rpms], '0') + ';' + format([DataCorePlugin.GameData.SpeedKmh], '0') + ';' + format([DataCorePlugin.GameData.Gear], '0') + ';' + format([DataCorePlugin.GameData.MaxRpm], '0') + '\n'` |
| Message before disconnect | `'DISCONNECT\n'` |

### Data Format

The ESP32 expects a newline-terminated string in this order:

```
RPM;SPEED;GEAR;MAXRPM\n
```

---

## Display Layout

```
+------------------+
| GEAR  |          |
|       |   142    |
|   3   |   KM/H   |
| RPM                  |
| ■■■■■■■■□□□□□□□□□□□□ |
+----------------------+
```

- Gear is displayed large on the left, color coded: white for 1–6, blue for N, red for R
- Speed occupies the right side
- RPM bar spans the full bottom, segments color through green → yellow → red scaling to the current car's max RPM

---

## Troubleshooting

| Symptom | Fix |
|---|---|
| Blank screen, backlight on | Check MOSI is on GPIO 23, not MISO |
| Wrong colors | Add `#define TFT_RGB_ORDER TFT_BGR` to User_Setup.h |
| Upside down / wrong orientation | Change `tft.setRotation(1)` to 2 or 3 |
| Screen works but no data | Check COM port and baud rate match in SimHub |
| RPM bar doesn't scale | Verify MaxRpm is included in SimHub formula and sep3 parsing is correct |
| Flickering | Ensure `setTextColor(FG, BG)` always has a background color set |