# RowComputer ESP32

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

## 🧠 Advanced Stroke Algorithm

This project uses a dynamic **High-Pass Filter** to isolate linear acceleration. 
Instead of relying on absolute raw data (which includes Earth's constant 9.81 m/s² pull), the code calculates an Exponential Moving Average (EMA) of the gravity vector and subtracts it in real-time. 
**Benefit:** You can mount the device at any angle on the boat. The algorithm will automatically find the "down" direction, eliminate it, and measure only the pure horizontal push of your strokes.

## ⚙️ Configuration and Calibration

Before taking it out on the water, you might need to tweak a few parameters at the top of the source file (`strokecoachGPS.ino`):

* `THRESHOLD_ACC = 3.0`: The sensitivity for detecting the catch (stroke). Since gravity is dynamically removed, this value represents the pure net acceleration. If the device counts "ghost strokes" during recovery, increase it (e.g., to 4.0). If it misses real strokes, decrease it. 
* `DEBOUNCE_STROKE = 1000`: The "dead time" in milliseconds (e.g., 1000ms = 1 second) required between one stroke and the next. This acts as a shield, preventing the slide recovery or final pull from being accidentally counted as a second stroke.
* **EMA Filter for SPM:** In the `loop()`, you will find `spm = (spm * 0.7) + (spmIstant * 0.3);`. Modify these percentages (sum must be 1.0) to make the display update more stable (e.g., `0.8` / `0.2`) or more reactive (e.g., `0.5` / `0.5`).

## 📁 Project Structure & Testing Tools

The repository is organized to help you test individual hardware components before running the full complex code:

* `/strokecoachGPS.ino`: The main application, with the 15s Watchdog( Reset ) logic.
* `/test/testGps/testGps.ino`: Isolates the GPS module. Use this to verify wiring and satellite lock (cold start) without screen flickering.
* `/test/testMpu/testMpu.ino`: A real-time telemetry dashboard for the MPU-6050. It displays raw axis data, the gravity filter in action, and the net vector. Perfect for testing your `THRESHOLD_ACC` sensitivity off the water.

## 🚀 Usage and Interface

The graphical interface is divided into 4 high-visibility quadrants designed for readability under direct sunlight:

1. **Top Left Quadrant (Yellow):** SPM (Strokes per Minute). Shows the stroke rate. Never Reset to zero.
2. **Top Right Quadrant (Cyan):** /500m Split. Calculates the projected time needed to cover 500 meters based on instantaneous GPS speed.
3. **Bottom Left Quadrant (White):** Stopwatch in `MM:SS` format. Starts automatically when the device is powered on.
4. **Bottom Right Quadrant (Green):** Total meters traveled. Includes an anti-drift filter: the distance only updates if the boat is moving > 2.0 km/h. An idle watchdog resets all stats if the boat is stationary for 15 seconds.
