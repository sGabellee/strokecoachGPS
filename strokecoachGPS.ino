#include <Adafruit_GFX.h>
#include <Adafruit_ST7789.h>
#include <SPI.h>
#include <Wire.h>
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>
#include <TinyGPSPlus.h>

// PIN CONFIG ESP32S3 (mini)
#define TFT_CS    5
#define TFT_DC    7
#define TFT_RST   10
#define TFT_MOSI  11
#define TFT_CLK   12
#define RXD2      3 // TX GPS
#define TXD2      4 // RX GPS

//  CONFIGURATION ALGORITHM STROKECOACH
// threshold for the accelerometer : lower mean sensible (keep over 9.81) 
const float THRESHOLD_ACC = 4.0; 
// Minimum interval between strokes, 1000ms = max 60 stroke/min
const unsigned long DEBOUNCE_STROKE = 1000; 

// OBJECT CREATION
Adafruit_ST7789 tft = Adafruit_ST7789(TFT_CS, TFT_DC, TFT_MOSI, TFT_CLK, TFT_RST);
Adafruit_MPU6050 mpu;
TinyGPSPlus gps;

// STATUS VARIABLE
// strats
int spm = 0;                  // Strokes Per Minute 
String split = "00:00";      // split /500m
unsigned long startTime = 0;
unsigned long totalMeters = 0; 
double lastLat = 0, lastLon = 0; // for the distance

// variable for the strokes
unsigned long timeLastStroke = 0;
bool stroking = false;

// variable for the screen
unsigned long lastDataUpdate = 0;
int oldSpm = -1;
String oldSplit = "";
unsigned long oldFormattedTime = 999999;
unsigned long oldMeters = 999999;

//update variable
unsigned long newLastUpdate = 0;
unsigned long oldLastUpdate = 0;
unsigned long watchdog = 15000;
bool changed = true;

// variables for gravity
float gravityX = 0, gravityY = 0, gravityZ = 0;

// FUNCTIONS

// inizialization of the screen
void drawInterface() {
  tft.fillScreen(ST77XX_BLACK);
  tft.setTextColor(ST77XX_WHITE);
  tft.setTextSize(2);

  // lines
  tft.drawFastHLine(0, 120, 320, ST77XX_WHITE); 
  tft.drawFastVLine(160, 0, 240, ST77XX_WHITE);

  // Titols
  tft.setTextSize(1);
  tft.setCursor(5, 5);   tft.print("SPM");
  tft.setCursor(165, 5); tft.print("/500m");
  tft.setCursor(5, 125);  tft.print("TEMPO");
  tft.setCursor(165, 125); tft.print("METRI");
}

// format seconds in mm:ss
String formattaTempo(unsigned long totalSeconds) {
  int minutes = totalSeconds / 60;
  int seconds = totalSeconds % 60;
  String s = "";
  if (minutes < 10) s += "0";
  s += String(minutes) + ":";
  if (seconds < 10) s += "0";
  s += String(seconds);
  return s;
}

// compute the split
String splitCalc(double kmph) {
  if (kmph < 1.5) return "00:00"; // too slow to compute

  // time for 500m in seconds = (500 * 3.6) / kmph = 1800 / kmph
  int totalSeconds500m = 1800 / kmph;
  int minutes = totalSeconds500m / 60;
  int seconds = totalSeconds500m % 60;

  String s = String(minutes) + ":";
  if (seconds < 10) s += "0";   // transform a 1:5 in 1:05
  s += String(seconds);
  return s;
}

// SETUP 
void setup() {
  Serial.begin(115200);
  Serial1.begin(9600, SERIAL_8N1, RXD2, TXD2); // GPS

  // Inizializza Display
  tft.init(240, 320);
  tft.setRotation(3); // Orizzontale corretto
  tft.invertDisplay(false);
  tft.fillScreen(ST77XX_BLACK);
  
  tft.setCursor(50, 100);
  tft.setTextColor(ST77XX_WHITE);
  tft.setTextSize(2);
  tft.println("Avvio RowComputer...");

  // initialize MPU6050
  Wire.begin(8, 9);
  mpu.begin(0x68, &Wire); 
  mpu.setAccelerometerRange(MPU6050_RANGE_8_G); // Range

  delay(2000); // Pause to read
  drawInterface();
  
  startTime = millis(); // start chrono

  newLastUpdate = millis();
  oldLastUpdate = millis();
}

// LOOP 
void loop() {
  // reset everything when stops for 15 sec
  tft.drawFastVLine(160, 0, 240, ST77XX_WHITE);
  oldLastUpdate = millis();
  
  if(!changed && oldLastUpdate - newLastUpdate > watchdog) {
    changed = true;

    spm = 0;
    oldSpm = 0;
    totalMeters = 0;
    oldMeters = 0;
    oldSplit = "00:00";
  
    tft.setTextSize(5);
    tft.setCursor(170, 50);
    tft.setTextColor(ST77XX_BLACK, ST77XX_BLACK);
    tft.print(oldSplit + " ");
    tft.setCursor(170, 50);
    tft.setTextColor(ST77XX_CYAN, ST77XX_BLACK);
    tft.print("00:00");
      
    tft.setTextSize(10);
    tft.setCursor(20, 30);
    tft.setTextColor(ST77XX_BLACK, ST77XX_BLACK);
    if (oldSpm < 10) tft.print("0");
    tft.print(oldSpm);
    tft.setCursor(20, 30);
    tft.setTextColor(ST77XX_YELLOW, ST77XX_BLACK); // Giallo per SPM
    tft.print("00");

    tft.setTextSize(5);
    tft.setCursor(170, 160);    
    tft.setTextColor(ST77XX_BLACK, ST77XX_BLACK);
    tft.print(String(oldMeters) + " ");
    tft.setCursor(170, 160);
    tft.setTextColor(ST77XX_GREEN, ST77XX_BLACK);
    tft.print("0");

    startTime = millis();
  }

  // always read the gps
  while (Serial1.available() > 0) {
    gps.encode(Serial1.read());
  }

  // strokecount
  sensors_event_t a, g, temp;
  mpu.getEvent(&a, &g, &temp);

  // Compute the magnitude (vector)
  // low pass filter: find where the gravity is
  const float alpha = 0.8;
  gravityX = alpha * gravityX + (1 - alpha) * a.acceleration.x;
  gravityY = alpha * gravityY + (1 - alpha) * a.acceleration.y;
  gravityZ = alpha * gravityZ + (1 - alpha) * a.acceleration.z;

  // remove the gravity
  float linearX = a.acceleration.x - gravityX;
  float linearY = a.acceleration.y - gravityY;
  float linearZ = a.acceleration.z - gravityZ;

  // now it computes
  float totalAccel = sqrt(linearX * linearX + linearY * linearY + linearZ * linearZ);

  unsigned long currentTime = millis();

  // stroke (start)
  if (totalAccel > THRESHOLD_ACC && !stroking) {
    // debounce verification
    if (currentTime - timeLastStroke > DEBOUNCE_STROKE) {
      
      // Compute stroke per minute based on the stroke
      unsigned long strokeInterval = currentTime - timeLastStroke;
      // Formule: SPM = 60000ms / intervallo_ms
      float spmIstant = 60000.0 / strokeInterval;
      
      // filter to stabilize (70% old, 30% new)
      spm = (spm * 0.7) + (spmIstant * 0.3);
      
      // SPM limit (Max 60)
      if (spm > 60) spm = 60;

      timeLastStroke = currentTime;
      stroking = true; // signal that we are stroking
      
      // Turn on the led to signal the stroke (esp32)
      digitalWrite(15, HIGH); 

      newLastUpdate = millis();
      changed = false;

    }
  }
  // Reset when acceleration return normal (end)
  if (totalAccel < (THRESHOLD_ACC * 0.5) && stroking) {
    stroking = false;
    digitalWrite(15, LOW); // turn off the led
  }
  
  // Reset SPM if i stay still for 4 sec
  if (currentTime - timeLastStroke > 4000) {
    spm = 0;
  }

  // DISTANCE LOGIC (GPS)
  if (gps.location.isValid() && gps.location.age() < 2000) {
    if (lastLat != 0 && lastLon != 0) {
      // Compute teh distance
      double partialDistance = TinyGPSPlus::distanceBetween(
        lastLat, lastLon,
        gps.location.lat(), gps.location.lng()
      );
      // Update the total meters
      if (partialDistance > 1.0 && gps.speed.kmph() > 2.0) {
        totalMeters += partialDistance;
        newLastUpdate = millis();
        changed = false;
      }
    }
    lastLat = gps.location.lat();
    lastLon = gps.location.lng();
  }


  // UPDATE DISPLAY (every 300ms, Flicker-free)
  if (currentTime - lastDataUpdate > 300) {
    lastDataUpdate = currentTime;

    // QUADRANTE 1: SPM ( HIGH LEFT, GIGANT) 
    if (spm != oldSpm && spm != 0) {
      tft.setTextSize(10);
      tft.setCursor(20, 30);
      // overwrite with a background to not update all the screen
      tft.setTextColor(ST77XX_BLACK, ST77XX_BLACK);
      if (oldSpm < 10) tft.print("0");
      tft.print(oldSpm);

      // write new value
      tft.setCursor(20, 30);
      tft.setTextColor(ST77XX_YELLOW, ST77XX_BLACK); // Giallo per SPM
      if (spm < 10) tft.print("0");
      tft.print(spm);
      oldSpm = spm;
    }

    // QUADRANTE 2: SPLIT /500m (HIGH RIGHT) 
    String splitAttuale = splitCalc(gps.speed.kmph());
    if (splitAttuale != oldSplit) {
      tft.setTextSize(5);
      tft.setCursor(170, 50);
      // Cancel old
      tft.setTextColor(ST77XX_BLACK, ST77XX_BLACK);
      tft.print(oldSplit + " "); 

      // write new
      tft.setCursor(170, 50);
      tft.setTextColor(ST77XX_CYAN, ST77XX_BLACK);
      tft.print(splitAttuale);
      oldSplit = splitAttuale;
    }

    // QUADRANTE 3: TIME CHRONO ( LOW LEFT ) 
    unsigned long secondiCronometro = (currentTime - startTime) / 1000;
    if (secondiCronometro != oldFormattedTime) {
      String tempoStr = formattaTempo(secondiCronometro);
      tft.setTextSize(5);
      tft.setCursor(10, 160);
      
      // overwrite all the line because they're bigger based on the number
      tft.setTextColor(ST77XX_BLACK, ST77XX_BLACK);
      tft.print(formattaTempo(oldFormattedTime));

      tft.setCursor(10, 160);
      tft.setTextColor(ST77XX_WHITE, ST77XX_BLACK);
      tft.print(tempoStr);
      oldFormattedTime = secondiCronometro;
    }

    // QUADRANTE 4: METERS (LOW RIGHT)
    if (totalMeters != oldMeters) {
      tft.setTextSize(5);
      tft.setCursor(170, 160);
      
      tft.setTextColor(ST77XX_BLACK, ST77XX_BLACK);
      tft.print(String(oldMeters) + " ");

      tft.setCursor(170, 160);
      tft.setTextColor(ST77XX_GREEN, ST77XX_BLACK);
      tft.print(totalMeters);
      oldMeters = totalMeters;
    }
  }
}
