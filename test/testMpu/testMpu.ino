#include <Adafruit_GFX.h>
#include <Adafruit_ST7789.h>
#include <SPI.h>
#include <Wire.h>
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>

// --- PIN DELLO SCHERMO ---
#define TFT_CS    5
#define TFT_DC    7
#define TFT_RST   10
#define TFT_MOSI  11
#define TFT_CLK   12

// --- CONFIGURATION ---
const float SOGLIA_ATTACCO = 4.0;
const float SOGLIA_FINALE = 3.5;
const unsigned long DEBOUNCE = 1000;

// --- OBJECT ---
Adafruit_ST7789 tft = Adafruit_ST7789(TFT_CS, TFT_DC, TFT_MOSI, TFT_CLK, TFT_RST);
Adafruit_MPU6050 mpu;

// --- GLOBAL VAR ---
float gravityX = 0, gravityY = 0, gravityZ = 0;
bool stroking = false;
unsigned long timeLastStroke = 0;
int spm = 0;
unsigned long lastDisplayUpdate = 0;

void setup() {
  Serial.begin(115200);

  tft.init(240, 320);
  tft.setRotation(3);
  tft.invertDisplay(false);
  tft.fillScreen(ST77XX_BLACK);

  tft.setTextSize(2);
  tft.setTextColor(ST77XX_YELLOW, ST77XX_BLACK);
  tft.setCursor(10, 10);
  tft.println("TELEMETRIA MPU6050");
  tft.drawLine(0, 35, 320, 35, ST77XX_WHITE);

  tft.setTextSize(2);
  tft.setTextColor(ST77XX_WHITE, ST77XX_BLACK);
  tft.setCursor(10, 50); tft.print("Asse X:");
  tft.setCursor(10, 80); tft.print("Asse Y:");
  tft.setCursor(10, 110); tft.print("Asse Z:");
  tft.setCursor(10, 150); tft.print("TOTALE:");

  tft.drawLine(0, 180, 320, 180, ST77XX_WHITE);
  
  tft.setCursor(10, 190); tft.print("STATO:");
  tft.setCursor(200, 190); tft.print("SPM:");

  // MPU
  Wire.begin(8, 9);
  mpu.begin(0x68, &Wire); // Avvio forzato ignorando l'ID
  mpu.setAccelerometerRange(MPU6050_RANGE_8_G);
  
  // initialize variables
  sensors_event_t a, g, temp;
  for(int i=0; i<20; i++) {
    mpu.getEvent(&a, &g, &temp);
    gravityX = a.acceleration.x;
    gravityY = a.acceleration.y;
    gravityZ = a.acceleration.z;
    delay(10);
  }
}

void loop() {
  // read sensor
  sensors_event_t a, g, temp;
  mpu.getEvent(&a, &g, &temp);

  // gravity filter
  gravityX = (0.8 * gravityX) + (0.2 * a.acceleration.x);
  gravityY = (0.8 * gravityY) + (0.2 * a.acceleration.y);
  gravityZ = (0.8 * gravityZ) + (0.2 * a.acceleration.z);

  // compute the acc
  float linearX = a.acceleration.x - gravityX;
  float linearY = a.acceleration.y - gravityY;
  float linearZ = a.acceleration.z - gravityZ;

  // magnitude
  float totalAccel = sqrt(linearX * linearX + linearY * linearY + linearZ * linearZ);

  // stroke logic
  unsigned long currentTime = millis();

  // Inizio Palata
  if (totalAccel > SOGLIA_ATTACCO && !stroking) {
    if (currentTime - timeLastStroke > DEBOUNCE) {
      unsigned long strokeInterval = currentTime - timeLastStroke;
      float spmIstant = 60000.0 / strokeInterval;
      spm = (spm * 0.7) + (spmIstant * 0.3);
      if (spm > 60) spm = 60;
      
      timeLastStroke = currentTime;
      stroking = true;
      digitalWrite(15, HIGH); 
    }
  }
  
  // end stroke
  if (totalAccel < SOGLIA_FINALE && stroking) {
    stroking = false;
    digitalWrite(15, LOW);
  }

  // reset
  if (currentTime - timeLastStroke > 4000) {
    spm = 0;
  }

  // update every 150ms
  if (currentTime - lastDisplayUpdate > 150) {
    lastDisplayUpdate = currentTime;

    tft.setTextSize(2);
    
    // print axis
    tft.setTextColor(ST77XX_CYAN, ST77XX_BLACK);
    tft.setCursor(110, 50); tft.print(linearX, 2); tft.print("   ");
    tft.setCursor(110, 80); tft.print(linearY, 2); tft.print("   ");
    tft.setCursor(110, 110); tft.print(linearZ, 2); tft.print("   ");

    // print Total
    tft.setTextColor(ST77XX_GREEN, ST77XX_BLACK);
    tft.setCursor(110, 150); tft.print(totalAccel, 2); tft.print("   ");

    // print state
    tft.setCursor(10, 215);
    tft.setTextSize(2);
    if (stroking) {
      tft.setTextColor(ST77XX_RED, ST77XX_BLACK); // Rosso quando c'è lo sforzo
      tft.print("IN PALATA  ");
    } else {
      tft.setTextColor(ST77XX_WHITE, ST77XX_BLACK);
      tft.print("RECUPERO   ");
    }

    // print SPM
    tft.setCursor(200, 215);
    tft.setTextColor(ST77XX_YELLOW, ST77XX_BLACK);
    if (spm < 10) tft.print("0");
    tft.print(spm);
  }
}
