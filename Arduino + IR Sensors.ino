/**
 * Bidirektionaler Holzstäbe-Zähler (Arduino + IR-Sensoren)
 * ---------------------------------------------------------
 * Autor: Amir Mobasheraghdam
 * Datum: 2025
 *
 * Beschreibung:
 *  - Verwendet zwei E18-D80NK Infrarot-Sensoren und ein 16x2 I2C-LCD
 *  - Zählt Objekte, die sich vor den Sensoren vorbeibewegen
 *  - Vorwärtsbewegung:  Zähler +1
 *  - Rückwärtsbewegung: Zähler –1
 *
 * Funktionsweise:
 *  Zwei Sensoren nebeneinander (A links, B rechts).
 *  Erkennt A zuerst und dann B → Vorwärts (Zähler erhöhen).
 *  Erkennt B zuerst und dann A → Rückwärts (Zähler verringern).
 *  Eine Zustandsmaschine unterdrückt Prellen und Rauschen.
 *
 * Hardware:
 *  - Arduino Uno/Nano
 *  - 2x E18-D80NK IR-Sensor (5V)
 *  - 16x2 LCD mit I2C-Modul (PCF8574)
 *  - Taster zum Zurücksetzen des Zählers (optional)
 *
 * Verdrahtung:
 *  Sensor A (braun)   → 5V
 *  Sensor A (blau)    → GND
 *  Sensor A (schwarz) → D2
 *
 *  Sensor B (braun)   → 5V
 *  Sensor B (blau)    → GND
 *  Sensor B (schwarz) → D3
 *
 *  LCD I2C VCC → 5V
 *  LCD I2C GND → GND
 *  LCD I2C SDA → A4
 *  LCD I2C SCL → A5
 *
 *  Taster: eine Seite GND, andere Seite D4 (interner Pull-up)
 */

#include <Wire.h>
#include <LiquidCrystal_I2C.h>

LiquidCrystal_I2C lcd(0x27, 16, 2);  // I2C-Adresse anpassen (0x27 oder 0x3F)

// Pin-Definitionen
const int PIN_SENSOR_A = 2;   // Sensor A (links)
const int PIN_SENSOR_B = 3;   // Sensor B (rechts)
const int PIN_RESET    = 4;   // Taster zum Zurücksetzen

// Variablen
long anzahl = 0;              // Aktueller Zählerstand
bool statusA, statusB;        // Aktuelle Erkennung der Sensoren (true = Objekt erkannt)

// Zustandsmaschine für die Richtungserkennung
enum Zustand {WARTEN, A_ZUERST, B_ZUERST};
Zustand zustand = WARTEN;

void setup() {
  pinMode(PIN_SENSOR_A, INPUT);
  pinMode(PIN_SENSOR_B, INPUT);
  pinMode(PIN_RESET, INPUT_PULLUP);   // Taster mit internem Pull-up

  lcd.init();
  lcd.backlight();
  lcd.clear();
  lcd.print("Bidir. Zaehler");
  lcd.setCursor(0, 1);
  lcd.print("v. Amir Mobasher");
  delay(1000);
  lcd.clear();
}

void loop() {
  // Sensoren einlesen: LOW = Objekt erkannt (aktiver LOW bei E18-D80NK)
  statusA = (digitalRead(PIN_SENSOR_A) == LOW);
  statusB = (digitalRead(PIN_SENSOR_B) == LOW);

  // Live-Anzeige des Sensorstatus (für Debug)
  lcd.setCursor(0, 0);
  lcd.print("A:");
  lcd.print(statusA ? "1" : "0");
  lcd.print(" B:");
  lcd.print(statusB ? "1" : "0");
  lcd.print("   ");

  // Zustandsmaschine
  switch (zustand) {
    case WARTEN:
      // Möglicher Beginn einer Vorwärtsbewegung: A erkannt, B nicht
      if (statusA && !statusB) {
        zustand = A_ZUERST;
      }
      // Möglicher Beginn einer Rückwärtsbewegung: B erkannt, A nicht
      else if (statusB && !statusA) {
        zustand = B_ZUERST;
      }
      break;

    case A_ZUERST:
      // Wenn jetzt auch B erkannt wird → Vorwärtsbewegung abgeschlossen
      if (statusB) {
        anzahl++;
        zeigeZahl();
        zustand = WARTEN;
      }
      // Falls A zwischendurch verschwindet (Rauschen oder unvollständige Bewegung) → abbrechen
      else if (!statusA) {
        zustand = WARTEN;
      }
      break;

    case B_ZUERST:
      // Wenn jetzt auch A erkannt wird → Rückwärtsbewegung abgeschlossen
      if (statusA) {
        anzahl--;
        zeigeZahl();
        zustand = WARTEN;
      }
      // Falls B zwischendurch verschwindet → abbrechen
      else if (!statusB) {
        zustand = WARTEN;
      }
      break;
  }

  // Reset-Taster abfragen (entprellt)
  if (digitalRead(PIN_RESET) == LOW) {
    delay(50);
    if (digitalRead(PIN_RESET) == LOW) {
      anzahl = 0;
      zeigeZahl();
      while (digitalRead(PIN_RESET) == LOW); // Auf Loslassen warten
    }
  }
}

/**
 * Zeigt den aktuellen Zählerstand auf dem LCD an (zweite Zeile).
 */
void zeigeZahl() {
  lcd.setCursor(0, 1);
  lcd.print("Anzahl: ");
  lcd.print(anzahl);
  lcd.print("      ");  // Rest der Zeile löschen
}
