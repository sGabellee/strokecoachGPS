# 🚣‍♂️ RowComputer ESP32 (SpeedCoach Clone)

Un computer di bordo fai-da-te per il canottaggio, ispirato al celebre NK SpeedCoach. Basato su un **ESP32-S3 Mini**, utilizza un accelerometro per contare i colpi (SPM) e un modulo GPS per calcolare la distanza e il passo sui 500 metri. Il tutto visualizzato su uno schermo TFT a colori con un'interfaccia ad alto contrasto e aggiornamenti *flicker-free*.

## 🛠 Hardware Necessario

* **Microcontrollore:** ESP32-S3 Mini (SuperMini)
* **Display:** Modulo TFT SPI 2.4" o 2.8" (Driver **ST7789**)
* **Accelerometro/Giroscopio:** MPU-6050 (Interfaccia I2C)
* **Ricevitore GPS:** Beitian BN-280 o generico NEO-6M (Interfaccia Seriale UART)
* **Alimentazione:** 3x batterie stilo AA (collegate ai pin 5V e GND) o Powerbank (USB-C)

## 🔌 Schema di Collegamento (Pinout)

Tutti i moduli comunicano a 3.3V, quindi non servono resistenze aggiuntive o level shifter tra i pin dati. *(Nota: i pin VCC del display e del GPS possono richiedere 5V, verifica le specifiche delle tue basette).*

### Display TFT (SPI)
| Pin TFT | Pin ESP32-S3 | Note |
| :--- | :--- | :--- |
| VCC | 5V | Alimentazione |
| GND | GND | Massa |
| CS | 5 | Chip Select |
| DC | 7 | Data / Command |
| RES / RST | 10 | Reset |
| SDA / MOSI | 11 | Master Out Slave In |
| SCL / SCK | 12 | Clock |
| BLK / LED | 3V3 | Retroilluminazione sempre ON |

### Modulo GPS (UART)
| Pin GPS | Pin ESP32-S3 | Note |
| :--- | :--- | :--- |
| VCC | 5V | Alimentazione |
| GND | GND | Massa |
| TX | 3 | Collegato al pin RX2 dell'ESP32 |
| RX | 4 | Collegato al pin TX2 dell'ESP32 |

### Accelerometro MPU-6050 (I2C)
| Pin MPU | Pin ESP32-S3 | Note |
| :--- | :--- | :--- |
| VCC | 3V3 | Alimentazione |
| GND | GND | Massa |
| SDA | 8 | Dati I2C |
| SCL | 9 | Clock I2C |

## 📦 Dipendenze Software (Librerie Arduino)

Per compilare questo codice tramite l'IDE di Arduino, installa le seguenti librerie tramite il **Library Manager**:
1. `Adafruit GFX Library` (Motore grafico base)
2. `Adafruit ST7735 and ST7789 Library` (Driver specifico per lo schermo)
3. `Adafruit MPU6050` (Driver per l'accelerometro)
4. `Adafruit Unified Sensor` (Dipendenza hardware richiesta da Adafruit)
5. `TinyGPSPlus` di Mikal Hart (Parsing dei dati satellitari grezzi NMEA)

## ⚙️ Configurazione e Calibrazione

Prima del varo in acqua, potresti aver bisogno di affinare alcuni parametri in cima al file sorgente a seconda di come posizioni il case sulla barca:

* `THRESHOLD_ACC = 16.0`: È la sensibilità per rilevare l'attacco in acqua (palata). Se lo strumento conta "colpi fantasma", alza il valore. Se perde colpi effettivi, abbassalo. *(Nota: non scendere sotto 9.81, che è l'accelerazione di gravità terrestre di base).*
* `DEBOUNCE_STROKE = 1000`: Il "tempo morto" in millisecondi (es. 1 secondo) richiesto tra un colpo e l'altro, utile per evitare di contare il ritorno del carrello come una seconda palata.
* **Filtro Esponenziale (EMA) SPM:** Nel `loop()` trovi la formula `spm = (spm * 0.7) + (spmIstant * 0.3);`. Modifica queste percentuali (la somma deve fare sempre 1.0) per rendere l'aggiornamento a display più stabile/lento (es. `0.8` e `0.2`) o più reattivo/nervoso (es. `0.5` e `0.5`).

## 🚀 Uso e Interfaccia

L'interfaccia grafica è divisa in 4 quadranti ad alta visibilità progettati per la lettura sotto il sole:

1. **Quadrante in Alto a Sinistra (Giallo):** SPM (Strokes per Minute). Mostra la cadenza. Si azzera automaticamente se non vengono rilevati colpi per più di 4 secondi.
2. **Quadrante in Alto a Destra (Ciano):** Split /500m. Calcola la proiezione del tempo necessario per percorrere 500 metri basata sulla velocità GPS istantanea.
3. **Quadrante in Basso a Sinistra (Bianco):** Cronometro nel formato `MM:SS`. Si avvia in automatico all'accensione dello strumento.
4. **Quadrante in Basso a Destra (Verde):** Metri totali percorsi. Integra un filtro anti-deriva GPS: la distanza si aggiorna solo se la barca viaggia a più di 2.0 km/h.