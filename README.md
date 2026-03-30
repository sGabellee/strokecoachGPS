# 🚣‍♂️ RowComputer ESP32

A DIY onboard computer for rowing, inspired by the famous NK SpeedCoach. Based on an **ESP32-S3 Mini**, it uses an accelerometer to count strokes (SPM) and a GPS module to calculate distance and the 500m split. Everything is displayed on a color TFT screen with a high-contrast, *flicker-free* interface.

## 🛠 Hardware Required

* **Microcontroller:** ESP32-S3 Mini (SuperMini)
* **Display:** 2.4" or 2.8" SPI TFT Module (**ST7789** Driver)
* **Accelerometer/Gyroscope:** MPU-6050 (I2C Interface)
* **GPS Receiver:** Beitian BN-280 or generic NEO-6M (UART Serial Interface)
* **Power Supply:** 3x AA batteries (connected to 5V and GND pins) or Powerbank (USB-C)

## 🔌 Wiring (Pinout)

All modules communicate at 3.3V, so no additional resistors or level shifters are needed between the data pins. *(Note: the VCC pins of the display and GPS might require 5V, check the silkscreen on your specific boards).*

### TFT Display (SPI)
| TFT Pin | ESP32-S3 Pin | Note |
| :--- | :--- | :--- |
| VCC | 5V | Power supply |
| GND | GND | Ground |
| CS | 5 | Chip Select |
| DC | 7 | Data / Command |
| RES / RST | 10 | Reset |
| SDA / MOSI | 11 | Master Out Slave In |
| SCL / SCK | 12 | Clock |
| BLK / LED | 3V3 | Backlight always ON |

### GPS Module (UART)
| GPS Pin | ESP32-S3 Pin | Note |
| :--- | :--- | :--- |
| VCC | 5V | Power supply |
| GND | GND | Ground |
| TX | 3 | Connect to the ESP32 RX2 pin |
| RX | 4 | Connect to the ESP32 TX2 pin |

### Accelerometer MPU-6050 (I2C)
| MPU Pin | ESP32-S3 Pin | Note |
| :--- | :--- | :--- |
| VCC | 3V3 | Power supply |
| GND | GND | Ground |
| SDA | 8 | I2C Data |
| SCL | 9 | I2C Clock |

## 📦 Software Dependencies (Arduino Libraries)

To compile this code via the Arduino IDE, install the following libraries using the **Library Manager**:
1. `Adafruit GFX Library` (Core graphics engine)
2. `Adafruit ST7735 and ST7789 Library` (Hardware-specific display driver)
3. `Adafruit MPU6050` (Accelerometer driver)
4. `Adafruit Unified Sensor` (Hardware dependency required by Adafruit)
5. `TinyGPSPlus` by Mikal Hart (Parsing raw NMEA satellite data)

## ⚙️ Configuration and Calibration

Before taking it out on the water, you might need to tweak a few parameters at the top of the source file depending on how you mount the case on the boat:

* `THRESHOLD_ACC = 16.0`: This is the sensitivity for detecting the catch (stroke). If the device counts "ghost strokes", increase this value. If it misses real strokes, decrease it. *(Note: do not go below 9.81, which is the baseline Earth's gravity acceleration).*
* `DEBOUNCE_STROKE = 1000`: The "dead time" in milliseconds (e.g., 1000ms = 1 second) required between one stroke and the next. This prevents the slide recovery from being accidentally counted as a second stroke.
* **Exponential Moving Average (EMA) Filter for SPM:** In the `loop()` function, you will find the formula `spm = (spm * 0.7) + (spmIstant * 0.3);`. Modify these percentages (the sum must always equal 1.0) to make the display update more stable/slow (e.g., `0.8` and `0.2`) or more reactive/nervous (e.g., `0.5` and `0.5`).

## 🚀 Usage and Interface

The graphical interface is divided into 4 high-visibility quadrants designed for readability under direct sunlight:

1. **Top Left Quadrant (Yellow):** SPM (Strokes per Minute). Shows the stroke rate. It automatically resets to zero if no strokes are detected for more than 4 seconds.
2. **Top Right Quadrant (Cyan):** /500m Split. Calculates the projected time needed to cover 500 meters based on the instantaneous GPS speed.
3. **Bottom Left Quadrant (White):** Stopwatch in `MM:SS` format. Starts automatically when the device is powered on.
4. **Bottom Right Quadrant (Green):** Total meters traveled. Includes an anti-drift GPS filter: the distance only updates if the boat is moving faster than 2.0 km/h.