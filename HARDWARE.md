# 🔧 Hardware Upgrade Guide

This document collects researched, buyable upgrades for two pain points of the
RowComputer: **sunlight readability** of the screen and **GPS quality** (signal
and speed/split stability). Prices are rough hobby-marketplace ranges (AliExpress /
Amazon, 2026) and only meant for comparison.

---

## 1. Sunlight-readable display

The current **ST7789 TFT** is a *transmissive, backlit* panel: great colors
indoors, but it washes out under direct sun and on the water (the worst case,
because of glare off the surface). The displays used by Casio watches and bike
computers look great in the sun because they are **reflective / transflective**
monochrome LCDs — they *use* ambient light instead of fighting it, and draw
almost no power because they need no backlight.

### Quick comparison

| Option | Tech | Size / Res | Sunlight | Live data? | Price | Reuses GFX code? |
| :-- | :-- | :-- | :-- | :-- | :-- | :-- |
| **Sharp Memory LCD `LS027B7DH01`** ⭐ | Reflective mono | 2.7" 400×240 | Excellent | ✅ fast | ~€15–25 bare, ~€40 breakout | ✅ Adafruit_GFX |
| Sharp Memory LCD `LS013B4DN04` | Reflective mono | 1.3" 168×144 | Excellent | ✅ fast | ~€10–15 | ✅ Adafruit_GFX |
| **Nokia 5110 `PCD8544`** 💰 | Reflective mono | 1.5" 84×48 | Good | ✅ fast | ~€2–5 | ✅ Adafruit_GFX |
| Waveshare `ESP32-S3-RLCD-4.2` | Reflective (RLCD) | 4.2" 400×300 | Excellent | ✅ | ~€35–45 (all-in-one) | partial (own board) |
| E-paper / e-ink | Reflective | various | Excellent | ❌ too slow | ~€15–30 | ✅ but unusable live |
| Custom segment LCD (HT1621) | Reflective TN | fixed segments | Excellent | ✅ | tooling NRE = €€€ one-off | ❌ rewrite |

### ⭐ Recommended: Sharp Memory LCD 2.7" (LS027B7DH01)

This is the closest thing to the "Casio / bike-computer" look while staying a
**graphic** display, and it is the best technical fit for this project:

- **Reflective, no backlight** → crisp black-on-silver that gets *more* readable
  the brighter it is outside. Perfect for on-water use.
- **Fast enough for live data** (unlike e-paper): it's a RAM-framebuffer panel
  refreshed over SPI at ~2 MHz; full-frame refresh is a few tens of ms, easily
  keeping the 300 ms cadence of SPM / split / chrono / meters.
- **Very low power** (no backlight) → kinder to the 3×AA / powerbank supply.
- **Reuses the code**: it is driven by `Adafruit_GFX`, the same library the
  project already uses, so all the `setCursor / setTextSize / print` logic
  stays — only color is dropped (it is monochrome).
- 400×240 is roomy enough for the existing 4-quadrant layout with big digits.

**Buy:** bare `LS027B7DH01` panel + breakout from AliExpress (~€15–25), or the
plug-and-play [Adafruit Sharp Memory Display breakout](https://learn.adafruit.com/adafruit-sharp-memory-display-breakout)
(~€40, easiest, includes the decoupling cap).

**Wiring (SPI — shares the bus with nothing else here):**

| Sharp pin | ESP32-S3 pin | Note |
| :-- | :-- | :-- |
| VIN | 3V3 (or 5V on breakout) | logic is 3.3 V |
| GND | GND | |
| CLK / SCK | 12 | same SPI clock as before |
| DI / MOSI | 11 | same SPI data as before |
| CS | 5 | **chip-select is active-HIGH** (library handles it) |

Frees up the old `DC` (7) and `RST` (10) pins — this panel needs neither.
Add a 1 µF cap across VIN/GND if you use a bare panel.

**Code changes** (library: `Adafruit SHARP Memory Display`):

```cpp
#include <Adafruit_SharpMem.h>
#define SHARP_SCK 12
#define SHARP_MOSI 11
#define SHARP_CS 5
Adafruit_SharpMem display(SHARP_SCK, SHARP_MOSI, SHARP_CS, 400, 240);
#define BLACK 0
#define WHITE 1
```

The display has a RAM framebuffer, so the "erase old text, then print new text"
trick used for the ST7789 is **not needed**. The cleanest pattern is:

```cpp
display.clearDisplay();      // clear the RAM buffer
// ... draw titles, dividers and the 4 values with display.print()/fillRect() ...
display.refresh();           // push the buffer to the glass
```

Because it is monochrome, replace the color-coded quadrants with **layout**
(boxes, bold/large fonts, small labels) to keep the four metrics distinct.

### 💰 Budget alternative: Nokia 5110 (PCD8544)

Famous "bike computer" LCD: 84×48 reflective monochrome, costs less than a
coffee, readable in the sun, `Adafruit_PCD8544` + `Adafruit_GFX`. 3.3 V logic
(perfect for the ESP32-S3, no level shifter needed). Downsides: low resolution
and small — it comfortably shows ~2 large numbers, so you'd page through metrics
or shrink the fonts to fit four. Great cheap way to prototype the reflective
look before committing to the Sharp panel.

### Not recommended here

- **E-paper / e-ink** — beautifully sunlight-readable, but refresh is far too
  slow (and ghosts) for a live SPM/split that updates several times per second.
- **Custom Casio-style segment LCD** — a *truly* custom glass needs tooling
  (NRE in the hundreds of € for a one-off), so it is not economical. Generic
  HT1621 6-digit modules exist but don't fit the color-coded 4-metric layout.

---

## 2. GPS: better signal, steadier split

Two different things are worth improving, and they compound:

1. **The receiver/chipset** — a NEO-6M is *GPS-only* and ships at **1 Hz**. Its
   speed reading is noisy and slow, which is exactly why the **/500m split
   dances**. A modern multi-GNSS receiver fixes this at the source.
2. **The antenna** — a bigger **active** ceramic patch (with a built-in LNA)
   locks faster, holds more satellites and gives a steadier speed.

> The single highest-impact change for this project is moving to a **u-blox
> NEO-M8N-class** receiver and running it at **5 Hz multi-GNSS**. It directly
> makes the split and the distance smoother and more accurate.

### Receiver options

| Module | Chipset | Constellations | Rate | Antenna | Price | Notes |
| :-- | :-- | :-- | :-- | :-- | :-- | :-- |
| (current) NEO-6M | u-blox 6 | GPS only | 1 Hz | small passive patch | ~€6–10 | noisy speed, slow |
| (current) BN-280 | u-blox M8030 | GPS+GLONASS | up to 10 Hz | ~15×15 mm patch | ~€10–14 | already decent, just configure it |
| **Beitian BN-220** | u-blox M8030 | GPS+GLONASS | 10 Hz | 22×20 mm patch | ~€12–16 | compact, big jump over NEO-6M |
| **Beitian BN-880** ⭐ | u-blox M8N | GPS+GLONASS | up to 10 Hz | **28×28 mm active** + compass | ~€18–25 | biggest onboard antenna, best lock |
| u-blox NEO-M9N / M10 | u-blox 9/10 | all 4 GNSS | up to 25 Hz | external (u.FL) | ~€25–40 | future-proof, overkill |

⭐ **Best balance: Beitian BN-880.** Same u-blox M8N brains as the popular
NEO-M8N boards, but with a **larger active patch antenna** (28×28 mm vs the
~15 mm patch on the BN-280) so it locks faster and holds a steadier speed —
which is what the split needs. The onboard compass is a free bonus if you ever
want heading. It speaks plain NMEA, so **`TinyGPSPlus` works unchanged**.

### Antenna, specifically

If you want to push antenna performance further (or your case shadows the
onboard patch):

- Pick a receiver with a **u.FL / IPEX connector** (most NEO-M8N breakouts have
  one) and add an **active GPS antenna** (≈28 dB gain, 25×25 mm or 35×35 mm
  ceramic patch, u.FL or SMA, ~€5–10).
- **Mount it flat, facing the sky, on top of the case** with a clear view — the
  patch must point up. On a rowing boat the sky view is excellent, so a good
  onboard active patch (e.g. the BN-880's) is usually enough; the external
  antenna mainly helps if the electronics/case block the patch.

### ⚠️ Software notes for the GPS upgrade

These matter as much as the hardware:

- **Configure once with u-center** (UBX-CFG-RATE for 5 Hz, UBX-CFG-GNSS for
  GPS+GLONASS+Galileo). M8N-class modules **save the config to flash**, so you
  do it once.
- **Raise the UART baud!** At 5 Hz with several NMEA sentences, **9600 baud
  overflows** and you lose data. Set the module to **38400 (or 57600)** and
  match it in the firmware:
  ```cpp
  Serial1.begin(38400, SERIAL_8N1, GPS_RX_PIN, GPS_TX_PIN);
  ```
- **Trim the NMEA output** to `GGA` + `RMC` (RMC carries speed/SOG and date/time,
  GGA carries fix quality and satellite count). Fewer sentences = more headroom.
- The firmware already reads `gps.speed.kmph()` (u-blox derives speed from
  Doppler, which is accurate) and now applies a light `SPEED_SMOOTH` filter, so
  the faster fix rate translates straight into a steadier split.

---

## 3. Bottom line — what to buy

| Goal | Pick | ~Cost |
| :-- | :-- | :-- |
| Best sunlight screen, keeps the code | **Sharp Memory LCD 2.7" LS027B7DH01** | €15–25 |
| Cheapest sunlight screen | **Nokia 5110 (PCD8544)** | €2–5 |
| Best GPS for a steady split | **Beitian BN-880 @ 5 Hz multi-GNSS** | €18–25 |
| Squeeze more antenna | u.FL receiver + 28 dB active patch on top of case | +€5–10 |

Total for the headline upgrade (Sharp 2.7" + BN-880): roughly **€35–50**.

## Sources

- [Adafruit Sharp Memory Display breakout (overview & wiring)](https://learn.adafruit.com/adafruit-sharp-memory-display-breakout)
- [Adafruit_SHARP_Memory_Display library](https://github.com/adafruit/Adafruit_SHARP_Memory_Display)
- [SHARP LS027B7DH01 product page (ifan-display)](https://ifan-display.com/product/2-7-inch-ls027b7dh01-spi-10pins-monochrome-screen/) · [DigiKey listing](https://www.digikey.com/en/products/detail/sharp-microelectronics/LS027B7DH01/5054066)
- [Sunlight-readable display recommendations (Arduino Forum)](https://forum.arduino.cc/t/sunlight-readable-graphical-display-for-esp32-recommendations/1042092)
- [Transflective vs. reflective displays (CDTech)](https://www.cdtech-lcd.com/news/comparing-sunlight-readable-displays-transflective-vs-reflective.html)
- [Waveshare ESP32-S3 reflective-LCD board (CNX-Software)](https://www.cnx-software.com/2026/01/06/esp32-s3-development-board-features-4-2-inch-reflective-lcd-rlcd-dual-microphone-array-onboard-speaker/)
- [Adafruit PCD8544 / Nokia 5110 library](https://github.com/adafruit/adafruit-pcd8544-nokia-5110-lcd-library) · [Random Nerd Tutorials guide](https://randomnerdtutorials.com/complete-guide-for-nokia-5110-lcd-with-arduino/)
- [GPS accuracy: NEO-6M vs M8N vs M9N (Zbotic)](https://zbotic.in/gps-accuracy-comparison-neo-6m-vs-neo-m8n-vs-neo-m9n/)
- [Beitian BN-220 vs BN-880 comparison (ArduPilot docs)](https://ardupilot.org/copter/docs/common-beitian-gps.html)
- [u-blox NEO-M8 datasheet](https://content.u-blox.com/sites/default/files/NEO-M8-FW3_DataSheet_UBX-15031086.pdf)
