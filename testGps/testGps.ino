#include <Adafruit_GFX.h>
#include <Adafruit_ST7789.h>
#include <SPI.h>
#include <TinyGPSPlus.h>

// --- PIN DELLO SCHERMO ---
#define TFT_CS    5
#define TFT_DC    7
#define TFT_RST   10
#define TFT_MOSI  11
#define TFT_CLK   12

// --- PIN DEL GPS ---
#define RXD2 3 // Va al TX del modulo GPS
#define TXD2 4 // Va all'RX del modulo GPS

Adafruit_ST7789 tft = Adafruit_ST7789(TFT_CS, TFT_DC, TFT_MOSI, TFT_CLK, TFT_RST);
TinyGPSPlus gps;

unsigned long ultimoAggiornamento = 0;
// Variabile che "ricorda" se nel secondo precedente avevamo segnale o no
bool avevaSatelliti = false; 

void setup() {
  Serial.begin(115200);
  Serial1.begin(9600, SERIAL_8N1, RXD2, TXD2);

  tft.init(240, 320);
  tft.setRotation(3);
  tft.invertDisplay(false);
  
  // Puliamo tutto lo schermo solo all'accensione
  tft.fillScreen(ST77XX_BLACK);

  tft.setTextSize(2);
  tft.setTextColor(ST77XX_YELLOW, ST77XX_BLACK); // Impostiamo sempre anche il colore di sfondo!
  tft.setCursor(10, 10);
  tft.println("TEST GPS PURO");
  tft.drawLine(0, 35, 320, 35, ST77XX_WHITE);
}

void loop() {
  // 1. Legge i dati dal GPS continuamente
  while (Serial1.available() > 0) {
    char c = Serial1.read();
    Serial.print(c); 
    gps.encode(c);   
  }

  // 2. Aggiorna lo schermo ogni secondo
  if (millis() - ultimoAggiornamento > 1000) {
    ultimoAggiornamento = millis();

    tft.setCursor(10, 50);
    tft.setTextSize(2);

    // CONTROLLO 1: L'ESP32 sta ricevendo qualcosa dai cavi?
    if (gps.charsProcessed() < 10) {
      tft.setTextColor(ST77XX_RED, ST77XX_BLACK);
      tft.println("ERRORE DI CABLAGGIO!           "); // Spazi vuoti per cancellare vecchie scritte
      tft.setCursor(10, 80);
      tft.setTextSize(1);
      tft.setTextColor(ST77XX_WHITE, ST77XX_BLACK);
      tft.println("L'ESP32 non riceve nulla dal GPS.        ");
      tft.setCursor(10, 100);
      tft.println("Controlla che il TX del GPS sia sul Pin 3");
      tft.setCursor(10, 120);
      tft.println("e l'RX del GPS sia sul Pin 4.            ");
      avevaSatelliti = false;
    }
    // CONTROLLO 2: Ha trovato i satelliti?
    else if (gps.satellites.value() > 0) {
      
      // Se PRIMA non c'era segnale e ADESSO sì, puliamo una volta sola l'area sotto il titolo
      if (!avevaSatelliti) {
        tft.fillRect(0, 40, 320, 200, ST77XX_BLACK);
        avevaSatelliti = true;
      }

      tft.setTextColor(ST77XX_GREEN, ST77XX_BLACK);
      tft.print("Satelliti agganciati: ");
      tft.print(gps.satellites.value());
      tft.println("   "); // Gli spazi vuoti cancellano le cifre avanzate se il numero scende

      tft.setCursor(10, 90);
      tft.setTextColor(ST77XX_CYAN, ST77XX_BLACK);
      tft.print("Lat: "); tft.print(gps.location.lat(), 6); tft.println("   ");
      tft.setCursor(10, 120);
      tft.print("Lon: "); tft.print(gps.location.lng(), 6); tft.println("   ");

      tft.setCursor(10, 160);
      tft.setTextColor(ST77XX_YELLOW, ST77XX_BLACK);
      tft.setTextSize(3);
      tft.print(gps.speed.kmph(), 1); // Meglio mostrare un solo decimale per non farlo ballare troppo
      tft.println(" km/h  ");
    } 
    // CONTROLLO 3: I cavi sono giusti ma sta ancora cercando (o ha appena perso il segnale)
    else {
      
      // Se PRIMA c'era segnale e l'ha appena PERSO, puliamo l'area con i vecchi dati
      if (avevaSatelliti) {
        tft.fillRect(0, 40, 320, 200, ST77XX_BLACK);
        avevaSatelliti = false;
      }

      tft.setTextColor(ST77XX_ORANGE, ST77XX_BLACK);
      tft.println("In attesa dei satelliti...         ");
      tft.setCursor(10, 90);
      tft.setTextSize(1);
      tft.setTextColor(ST77XX_WHITE, ST77XX_BLACK);
      tft.println("I cavi sono corretti! Il modulo sta parlando.  ");
      tft.setCursor(10, 110);
      tft.println("Aspetta che agganci il segnale dal cielo...    ");
    }
  }
}
