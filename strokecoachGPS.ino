/*
 * RowComputer ESP32 (StrokeCoachGPS)
 * On-board rowing computer: stroke rate (SPM), GPS distance and /500m split.
 *
 * Hardware: ESP32-S3 Mini + MPU-6050 + GPS (NEO-6M / BN-280) + ST7789 TFT.
 * See README.md for wiring and HARDWARE.md for recommended upgrades
 * (sunlight-readable display, better GPS module/antenna).
 */

#include <Adafruit_GFX.h>
#include <Adafruit_ST7789.h>
#include <SPI.h>
#include <Wire.h>
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>
#include <TinyGPSPlus.h>

// ---------------------------------------------------------------------------
// PIN CONFIG (ESP32-S3 Mini)
// ---------------------------------------------------------------------------
#define TFT_CS      5
#define TFT_DC      7
#define TFT_RST     10
#define TFT_MOSI    11
#define TFT_CLK     12
#define GPS_RX_PIN  3    // ESP32 RX  <- GPS TX
#define GPS_TX_PIN  4    // ESP32 TX  -> GPS RX
#define BTN_RESET   6    // momentary button to GND (uses internal pull-up)
#define LED_STROKE  15   // LED that blinks on every detected stroke
#define MPU_SDA     8
#define MPU_SCL     9

// ---------------------------------------------------------------------------
// STROKE DETECTION / BEHAVIOUR TUNING
// ---------------------------------------------------------------------------
// Net acceleration (gravity removed) needed to detect the catch. Lower = more
// sensitive. Raise it if you get "ghost strokes", lower it if strokes are missed.
const float         THRESHOLD_ACC   = 3.0;
// The stroke is considered "released" below this fraction of the threshold.
const float         RELEASE_FACTOR  = 0.5;
// Dead time between two strokes (ms). 1000 ms => max 60 SPM.
const unsigned long DEBOUNCE_STROKE = 1000;
// SPM smoothing: spm = SPM_SMOOTH*old + (1 - SPM_SMOOTH)*instant.
const float         SPM_SMOOTH      = 0.7;
const int           SPM_MAX         = 60;
// SPM is forced back to 0 after this idle time with no strokes (ms).
const unsigned long STROKE_TIMEOUT  = 4000;

// ---------------------------------------------------------------------------
// GPS / DISTANCE TUNING
// ---------------------------------------------------------------------------
const double MOVE_MIN_KMPH  = 2.0;  // min speed to accumulate distance (anti-drift)
const double MIN_SEGMENT_M  = 1.0;  // ignore GPS jumps smaller than this (noise)
const double SPLIT_MIN_KMPH = 1.5;  // below this the split is shown as 00:00
const float  SPEED_SMOOTH   = 0.6;  // smoothing on GPS speed so the split stops dancing

// ---------------------------------------------------------------------------
// SESSION / UI TIMING
// ---------------------------------------------------------------------------
const unsigned long WATCHDOG_MS        = 15000; // wipe session after this idle time
const unsigned long DISPLAY_REFRESH_MS = 300;   // screen refresh period (flicker-free)
const unsigned long BTN_DEBOUNCE_MS    = 500;   // reset-button debounce

// ---------------------------------------------------------------------------
// OBJECTS
// ---------------------------------------------------------------------------
Adafruit_ST7789  tft = Adafruit_ST7789(TFT_CS, TFT_DC, TFT_MOSI, TFT_CLK, TFT_RST);
Adafruit_MPU6050 mpu;
TinyGPSPlus      gps;

// ---------------------------------------------------------------------------
// SESSION STATE
// ---------------------------------------------------------------------------
float         spmFiltered = 0.0;       // smoothed stroke rate (float accumulator)
int           spm         = 0;         // value shown on screen
char          splitStr[8] = "00:00";   // current /500m split
double        totalMeters = 0.0;       // accumulated distance (double avoids drift)
unsigned long startTime   = 0;         // chrono start (ms)

// stroke detection
unsigned long timeLastStroke = 0;
bool          stroking       = false;
bool          firstStroke    = true;

// gravity vector (low-pass) used to isolate linear acceleration
float gravityX = 0, gravityY = 0, gravityZ = 0;

// GPS distance helpers
double lastLat = 0, lastLon = 0;
float  speedFiltered = 0.0;

// idle watchdog
unsigned long lastActivity = 0;
bool          statsCleared = false;

// reset button
unsigned long lastButtonPress = 0;

// values currently on screen (so we redraw only what changed)
int           oldSpm        = -1;
char          oldSplitStr[8] = "";
unsigned long oldSeconds    = 999999;
long          oldMeters     = -1;
unsigned long lastDisplayUpdate = 0;

// ---------------------------------------------------------------------------
// HELPERS
// ---------------------------------------------------------------------------

// Format seconds as "MM:SS" (minutes clamped to 99) into buf (>= 6 bytes).
void formatMMSS(char *buf, unsigned long totalSeconds) {
  unsigned long minutes = totalSeconds / 60;
  unsigned long seconds = totalSeconds % 60;
  if (minutes > 99) minutes = 99;
  snprintf(buf, 6, "%02lu:%02lu", minutes, seconds);
}

// Projected time to cover 500 m at the given speed -> "M:SS" into buf (>= 6).
void formatSplit(char *buf, double kmph) {
  if (kmph < SPLIT_MIN_KMPH) { strcpy(buf, "00:00"); return; }
  int totalSeconds = (int)(1800.0 / kmph);   // 500 m takes 1800/kmph seconds
  if (totalSeconds > 5999) totalSeconds = 5999;
  snprintf(buf, 6, "%d:%02d", totalSeconds / 60, totalSeconds % 60);
}

// Static layout: dividers and quadrant titles. Clears the whole screen.
void drawInterface() {
  tft.fillScreen(ST77XX_BLACK);
  tft.drawFastHLine(0, 120, 320, ST77XX_WHITE);
  tft.drawFastVLine(160, 0, 240, ST77XX_WHITE);

  tft.setTextSize(1);
  tft.setTextColor(ST77XX_WHITE, ST77XX_BLACK);
  tft.setCursor(5, 5);     tft.print("SPM");
  tft.setCursor(165, 5);   tft.print("/500m");
  tft.setCursor(5, 125);   tft.print("TEMPO");
  tft.setCursor(165, 125); tft.print("METRI");
}

// Reset all session stats and restart the chrono / watchdog.
void resetSession() {
  spmFiltered  = 0;  spm = 0;
  totalMeters  = 0;
  strcpy(splitStr, "00:00");
  speedFiltered = 0;
  lastLat = 0; lastLon = 0;
  timeLastStroke = 0;
  firstStroke  = true;
  stroking     = false;
  digitalWrite(LED_STROKE, LOW);
  startTime    = millis();
  lastActivity = millis();
  statsCleared = false;
}

// Redraw a quadrant only when its value changed (or force == true). Uses the
// "print old value in background colour, then print the new one" trick so we
// never clear a whole quadrant -> no flicker.
void updateDisplay(bool force) {
  // QUADRANT 1: SPM (top-left, giant yellow). Stays visible at its last value;
  // it is only painted as "00" when we explicitly force a reset.
  if (force || (spm != oldSpm && spm != 0)) {
    int prev = oldSpm < 0 ? 0 : oldSpm;
    tft.setTextSize(10);
    tft.setCursor(20, 30);
    tft.setTextColor(ST77XX_BLACK, ST77XX_BLACK);
    if (prev < 10) tft.print('0');
    tft.print(prev);
    tft.setCursor(20, 30);
    tft.setTextColor(ST77XX_YELLOW, ST77XX_BLACK);
    if (spm < 10) tft.print('0');
    tft.print(spm);
    oldSpm = spm;
  }

  // QUADRANT 2: /500m split (top-right, cyan)
  if (force || strcmp(splitStr, oldSplitStr) != 0) {
    tft.setTextSize(5);
    tft.setCursor(170, 50);
    tft.setTextColor(ST77XX_BLACK, ST77XX_BLACK);
    tft.print(oldSplitStr); tft.print(' ');
    tft.setCursor(170, 50);
    tft.setTextColor(ST77XX_CYAN, ST77XX_BLACK);
    tft.print(splitStr);
    strcpy(oldSplitStr, splitStr);
  }

  // QUADRANT 3: chrono (bottom-left, white)
  unsigned long seconds = (millis() - startTime) / 1000;
  if (force || seconds != oldSeconds) {
    char buf[6], oldBuf[6];
    formatMMSS(buf, seconds);
    formatMMSS(oldBuf, oldSeconds);
    tft.setTextSize(5);
    tft.setCursor(10, 160);
    tft.setTextColor(ST77XX_BLACK, ST77XX_BLACK);
    tft.print(oldBuf);
    tft.setCursor(10, 160);
    tft.setTextColor(ST77XX_WHITE, ST77XX_BLACK);
    tft.print(buf);
    oldSeconds = seconds;
  }

  // QUADRANT 4: meters (bottom-right, green)
  long meters = (long)totalMeters;
  if (force || meters != oldMeters) {
    char buf[12];
    tft.setTextSize(5);
    tft.setCursor(170, 160);
    tft.setTextColor(ST77XX_BLACK, ST77XX_BLACK);
    snprintf(buf, sizeof(buf), "%ld ", oldMeters < 0 ? 0 : oldMeters);
    tft.print(buf);
    tft.setCursor(170, 160);
    tft.setTextColor(ST77XX_GREEN, ST77XX_BLACK);
    tft.print(meters);
    oldMeters = meters;
  }
}

// ---------------------------------------------------------------------------
// SETUP
// ---------------------------------------------------------------------------
void setup() {
  Serial.begin(115200);
  Serial1.begin(9600, SERIAL_8N1, GPS_RX_PIN, GPS_TX_PIN); // GPS

  pinMode(BTN_RESET, INPUT_PULLUP);
  pinMode(LED_STROKE, OUTPUT);
  digitalWrite(LED_STROKE, LOW);

  // Display
  tft.init(240, 320);
  tft.setRotation(3);          // landscape
  tft.invertDisplay(false);
  tft.fillScreen(ST77XX_BLACK);
  tft.setTextColor(ST77XX_WHITE);
  tft.setTextSize(2);
  tft.setCursor(20, 100);
  tft.println("Avvio RowComputer...");

  // MPU-6050
  Wire.begin(MPU_SDA, MPU_SCL);
  if (!mpu.begin(0x68, &Wire)) {
    tft.setTextColor(ST77XX_RED, ST77XX_BLACK);
    tft.setCursor(20, 140);
    tft.println("MPU6050 non trovato!");
  }
  mpu.setAccelerometerRange(MPU6050_RANGE_8_G);

  delay(2000); // splash

  drawInterface();
  resetSession();
  updateDisplay(true);
}

// ---------------------------------------------------------------------------
// LOOP
// ---------------------------------------------------------------------------
void loop() {
  unsigned long now = millis();

  // --- RESET BUTTON ---
  if (digitalRead(BTN_RESET) == LOW && now - lastButtonPress > BTN_DEBOUNCE_MS) {
    lastButtonPress = now;
    resetSession();
    drawInterface();
    updateDisplay(true);
  }

  // --- IDLE WATCHDOG: wipe the session after WATCHDOG_MS without activity ---
  if (!statsCleared && now - lastActivity > WATCHDOG_MS) {
    statsCleared  = true;
    spmFiltered   = 0;  spm = 0;
    totalMeters   = 0;
    strcpy(splitStr, "00:00");
    speedFiltered = 0;
    startTime     = now;
    updateDisplay(true);
  }

  // --- GPS: always feed the parser ---
  while (Serial1.available() > 0) gps.encode(Serial1.read());

  // --- STROKE DETECTION (MPU-6050) ---
  sensors_event_t a, g, temp;
  mpu.getEvent(&a, &g, &temp);

  // Low-pass filter to estimate where gravity points, then subtract it so only
  // the linear push of the stroke remains (device can be mounted at any angle).
  const float alpha = 0.8;
  gravityX = alpha * gravityX + (1 - alpha) * a.acceleration.x;
  gravityY = alpha * gravityY + (1 - alpha) * a.acceleration.y;
  gravityZ = alpha * gravityZ + (1 - alpha) * a.acceleration.z;

  float linearX = a.acceleration.x - gravityX;
  float linearY = a.acceleration.y - gravityY;
  float linearZ = a.acceleration.z - gravityZ;
  float totalAccel = sqrtf(linearX * linearX + linearY * linearY + linearZ * linearZ);

  // Catch: net acceleration crosses the threshold (with debounce).
  if (totalAccel > THRESHOLD_ACC && !stroking &&
      now - timeLastStroke > DEBOUNCE_STROKE) {
    if (firstStroke) {
      firstStroke = false;                 // don't compute SPM from boot-time interval
    } else {
      float spmInstant = 60000.0 / (now - timeLastStroke);
      spmFiltered = SPM_SMOOTH * spmFiltered + (1 - SPM_SMOOTH) * spmInstant;
      if (spmFiltered > SPM_MAX) spmFiltered = SPM_MAX;
      spm = (int)(spmFiltered + 0.5);      // rounded value for the display
    }
    timeLastStroke = now;
    stroking = true;
    digitalWrite(LED_STROKE, HIGH);
    lastActivity = now;
    statsCleared = false;
  }
  // Release: acceleration falls back to rest.
  if (totalAccel < THRESHOLD_ACC * RELEASE_FACTOR && stroking) {
    stroking = false;
    digitalWrite(LED_STROKE, LOW);
  }
  // Idle: forget the rate after a long pause.
  if (now - timeLastStroke > STROKE_TIMEOUT) {
    spmFiltered = 0;
    spm = 0;
  }

  // --- DISTANCE & SPEED (GPS), once per new fix ---
  if (gps.location.isUpdated() && gps.location.isValid() && gps.location.age() < 2000) {
    double lat = gps.location.lat();
    double lon = gps.location.lng();
    double spd = gps.speed.kmph();
    speedFiltered = SPEED_SMOOTH * speedFiltered + (1 - SPEED_SMOOTH) * spd;

    if (lastLat != 0 && lastLon != 0) {
      double segment = TinyGPSPlus::distanceBetween(lastLat, lastLon, lat, lon);
      if (segment > MIN_SEGMENT_M && spd > MOVE_MIN_KMPH) {
        totalMeters += segment;
        lastActivity = now;
        statsCleared = false;
      }
    }
    lastLat = lat;
    lastLon = lon;
  }

  // --- DISPLAY REFRESH (flicker-free) ---
  if (now - lastDisplayUpdate > DISPLAY_REFRESH_MS) {
    lastDisplayUpdate = now;
    formatSplit(splitStr, speedFiltered);
    updateDisplay(false);
  }
}
