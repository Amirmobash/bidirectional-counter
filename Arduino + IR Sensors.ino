/**
 * Bidirektionaler Holzstäbe-Zähler (Arduino + IR-Sensoren) - Premium Version
 * -------------------------------------------------------------------------
 * Autor: Amir Mobasheraghdam
 * Datum: 2025
 * Version: 2.0
 *
 * Beschreibung:
 *  - Professioneller bidirectional counter mit zwei E18-D80NK IR-Sensoren
 *  - 16x2 I2C LCD Display mit optimierter Anzeige
 *  - Fortschrittliche Entprellung und Rauschunterdrückung
 *  - EEPROM-Speicherung des Zählerstands
 *  - Statistische Auswertung (Rate pro Minute, Gesamtbetriebszeit)
 *  - Menüsystem für Einstellungen
 *
 * Verbesserungen gegenüber V1:
 *  ✓ EEPROM Speicherung bei Stromausfall
 *  ✓ Timer-basierte Entprellung (kein delay())
 *  ✓ Ratenberechnung (Stäbe pro Minute)
 *  ✓ Menü mit Reset-Funktion und Statistiken
 *  ✓ Optimierte LCD-Aktualisierung (reduziert Flackern)
 *  ✓ Debug-Modus über serielle Schnittstelle
 *  ✓ Konfigurierbare Sensortypen (NO/NC)
 */

#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <EEPROM.h>

// ==================== KONFIGURATION ====================
// LCD Einstellungen
const byte LCD_ADDRESS = 0x27;        // I2C Adresse (0x27 oder 0x3F)
const byte LCD_COLUMNS = 16;
const byte LCD_ROWS = 2;

// Pin Definitionen
const byte PIN_SENSOR_A = 2;           // Sensor A (links/außen)
const byte PIN_SENSOR_B = 3;           // Sensor B (rechts/innen)
const byte PIN_RESET = 4;              // Reset-Taster
const byte PIN_MENU = 5;               // Menü-Taster (optional)
const byte PIN_BUZZER = 6;             // Piezo-Summer für Bestätigung (optional)

// Zeitkonstanten (in Millisekunden)
const unsigned long DEBOUNCE_TIME = 50;        // Entprellzeit für Sensoren
const unsigned long RESET_DEBOUNCE = 100;      // Entprellzeit Reset-Taster
const unsigned long DISPLAY_UPDATE_INTERVAL = 100; // LCD Update-Intervall
const unsigned long STATS_INTERVAL = 60000;    // Statistik-Intervall (1 Minute)
const unsigned long SAVE_INTERVAL = 10000;     // EEPROM Speicherintervall

// EEPROM Adressen
const int EEPROM_ADDR_COUNTER = 0;      // 4 Bytes für long
const int EEPROM_ADDR_MAGIC = 4;        // 2 Bytes Magic Number
const unsigned int MAGIC_NUMBER = 0x5A5A; // Prüfsumme für gültigen EEPROM

// Betriebsparameter
const bool SENSOR_ACTIVE_LOW = true;    // true = LOW bei Objekt (E18-D80NK)
const bool DEBUG_MODE = true;           // Serielle Debug-Ausgaben
const bool USE_BUZZER = false;          // Summer aktivieren

// Menüpunkte
enum MenuItem {
  MENU_MAIN,
  MENU_RESET,
  MENU_STATS,
  MENU_CALIBRATE,
  MENU_EXIT
};

// ==================== GLOBALE VARIABLEN ====================
LiquidCrystal_I2C lcd(LCD_ADDRESS, LCD_COLUMNS, LCD_ROWS);

// Zählervariablen
volatile long counter = 0;              // Aktueller Zählerstand
long lastSavedCounter = 0;              // Letzter gespeicherter Wert

// Sensorstatus
volatile bool sensorAState = false;
volatile bool sensorBState = false;
volatile bool lastSensorA = false;
volatile bool lastSensorB = false;

// Zustandsmaschine
enum DirectionState { STATE_IDLE, STATE_A_FIRST, STATE_B_FIRST };
DirectionState directionState = STATE_IDLE;

// Zeitsteuerung
unsigned long lastDebounceTimeA = 0;
unsigned long lastDebounceTimeB = 0;
unsigned long lastDisplayUpdate = 0;
unsigned long lastStatsUpdate = 0;
unsigned long lastSaveTime = 0;
unsigned long lastRateCalc = 0;

// Statistikvariablen
unsigned long totalCounts = 0;          // Gesamtzahl Bewegungen (in/out)
unsigned long forwardCounts = 0;        // Vorwärtsbewegungen
unsigned long backwardCounts = 0;       // Rückwärtsbewegungen
float currentRate = 0.0;               // Aktuelle Rate (Stäbe/Minute)
unsigned long lastRateCounter = 0;      // Counter-Wert für Ratenberechnung
unsigned long programStartTime = 0;     // Programmstartzeit

// Menüvariablen
bool menuActive = false;
MenuItem currentMenu = MENU_MAIN;
byte menuPosition = 0;
unsigned long lastButtonPress = 0;
const unsigned long MENU_TIMEOUT = 10000; // 10 Sekunden Timeout

// ==================== FUNKTIONEN ====================

/**
 * Initialisiert das EEPROM und lädt den gespeicherten Zählerstand
 */
void loadFromEEPROM() {
  unsigned int magic;
  EEPROM.get(EEPROM_ADDR_MAGIC, magic);
  
  if (magic == MAGIC_NUMBER) {
    EEPROM.get(EEPROM_ADDR_COUNTER, counter);
    if (counter < 0 || counter > 1000000) counter = 0; // Plausibilitätsprüfung
    if (DEBUG_MODE) {
      Serial.print(F("[EEPROM] Geladen: "));
      Serial.println(counter);
    }
  } else {
    counter = 0;
    if (DEBUG_MODE) Serial.println(F("[EEPROM] Keine gültigen Daten, starte bei 0"));
  }
  
  lastSavedCounter = counter;
  totalCounts = abs(counter); // Vereinfachte Schätzung
}

/**
 * Speichert den aktuellen Zählerstand im EEPROM
 */
void saveToEEPROM() {
  if (counter != lastSavedCounter) {
    EEPROM.put(EEPROM_ADDR_COUNTER, counter);
    EEPROM.put(EEPROM_ADDR_MAGIC, MAGIC_NUMBER);
    lastSavedCounter = counter;
    if (DEBUG_MODE) {
      Serial.print(F("[EEPROM] Gespeichert: "));
      Serial.println(counter);
    }
  }
}

/**
 * Aktualisiert die Statistikwerte
 */
void updateStatistics() {
  unsigned long now = millis();
  
  if (now - lastStatsUpdate >= STATS_INTERVAL) {
    // Berechne Rate (Stäbe pro Minute)
    long deltaCount = counter - lastRateCounter;
    currentRate = (deltaCount * 60000.0) / (now - lastRateCalc);
    if (currentRate < 0) currentRate = 0;
    
    lastRateCounter = counter;
    lastRateCalc = now;
    lastStatsUpdate = now;
    
    if (DEBUG_MODE) {
      Serial.print(F("[STATS] Rate: "));
      Serial.print(currentRate, 1);
      Serial.println(F(" Stäbe/Min"));
    }
  }
}

/**
 * Gibt einen akustischen Bestätigungston aus
 */
void beep(int duration = 50, int frequency = 2000) {
  if (USE_BUZZER) {
    tone(PIN_BUZZER, frequency, duration);
  }
}

/**
 * Zeigt den Hauptbildschirm mit Zählerstand und Rate an
 */
void updateDisplay() {
  static char buffer[17];
  static long lastDisplayedCounter = -1;
  static float lastDisplayedRate = -1;
  
  // Nur aktualisieren wenn sich etwas geändert hat
  if (counter != lastDisplayedCounter || (int)currentRate != (int)lastDisplayedRate) {
    lcd.clear();
    
    // Zeile 1: Zählerstand mit Einheit
    lcd.setCursor(0, 0);
    lcd.print(F("Staebe: "));
    lcd.print(counter);
    
    // Zeile 2: Rate oder Status
    lcd.setCursor(0, 1);
    if (counter >= 0) {
      lcd.print(F("Rate: "));
      dtostrf(currentRate, 5, 1, buffer);
      lcd.print(buffer);
      lcd.print(F("/min"));
    } else {
      lcd.print(F("Negativ!   "));
    }
    
    lastDisplayedCounter = counter;
    lastDisplayedRate = currentRate;
  }
  
  // Sensorstatus in der oberen rechten Ecke (optional)
  lcd.setCursor(12, 0);
  lcd.print(sensorAState ? "A" : " ");
  lcd.print(sensorBState ? "B" : " ");
}

/**
 * Zeigt ein Menü an
 */
void showMenu() {
  lcd.clear();
  lcd.setCursor(0, 0);
  
  switch (currentMenu) {
    case MENU_MAIN:
      lcd.print(F(">Reset  Stats   "));
      lcd.setCursor(0, 1);
      lcd.print(F(" Calib Exit     "));
      break;
    case MENU_RESET:
      lcd.print(F("Reset bestaetigen?"));
      lcd.setCursor(0, 1);
      lcd.print(F("<Nein   Ja>     "));
      break;
    case MENU_STATS:
      lcd.print(F("Forward:"));
      lcd.print(forwardCounts);
      lcd.setCursor(0, 1);
      lcd.print(F("Back:"));
      lcd.print(backwardCounts);
      break;
    case MENU_CALIBRATE:
      lcd.print(F("Sensor-Kalibrierung"));
      lcd.setCursor(0, 1);
      lcd.print(F("A/B an/aus...    "));
      break;
    case MENU_EXIT:
      menuActive = false;
      updateDisplay();
      return;
  }
}

/**
 * Verarbeitet Menü-Eingaben
 */
void handleMenu() {
  static bool lastMenuButton = HIGH;
  static bool lastResetButton = HIGH;
  
  bool menuPressed = (digitalRead(PIN_MENU) == LOW);
  bool resetPressed = (digitalRead(PIN_RESET) == LOW);
  
  if (menuPressed && !lastMenuButton && (millis() - lastButtonPress > DEBOUNCE_TIME)) {
    lastButtonPress = millis();
    beep(30, 1500);
    
    if (!menuActive) {
      menuActive = true;
      currentMenu = MENU_MAIN;
      menuPosition = 0;
    } else {
      // Navigation im Menü
      switch (currentMenu) {
        case MENU_MAIN:
          menuPosition = (menuPosition + 1) % 4;
          break;
        case MENU_RESET:
          if (menuPosition == 1) { // Ja
            counter = 0;
            forwardCounts = 0;
            backwardCounts = 0;
            saveToEEPROM();
            beep(100, 1000);
            menuActive = false;
            updateDisplay();
          }
          menuActive = false;
          break;
        case MENU_STATS:
          menuActive = false;
          break;
        case MENU_CALIBRATE:
          menuActive = false;
          break;
      }
    }
    showMenu();
  }
  
  if (resetPressed && !lastResetButton && menuActive && (millis() - lastButtonPress > DEBOUNCE_TIME)) {
    lastButtonPress = millis();
    beep(30, 1200);
    
    if (currentMenu == MENU_MAIN) {
      switch (menuPosition) {
        case 0: currentMenu = MENU_RESET; break;
        case 1: currentMenu = MENU_STATS; break;
        case 2: currentMenu = MENU_CALIBRATE; break;
        case 3: currentMenu = MENU_EXIT; break;
      }
      showMenu();
    }
  }
  
  lastMenuButton = menuPressed;
  lastResetButton = resetPressed;
  
  // Menu timeout
  if (menuActive && (millis() - lastButtonPress > MENU_TIMEOUT)) {
    menuActive = false;
    updateDisplay();
  }
}

/**
 * Verarbeitet die Sensorzustände und Richtungserkennung
 */
void processSensors() {
  unsigned long now = millis();
  
  // Sensor A mit Entprellung
  bool rawA = (digitalRead(PIN_SENSOR_A) == (SENSOR_ACTIVE_LOW ? LOW : HIGH));
  if (rawA != lastSensorA) {
    lastDebounceTimeA = now;
  }
  if ((now - lastDebounceTimeA) > DEBOUNCE_TIME) {
    sensorAState = rawA;
  }
  lastSensorA = rawA;
  
  // Sensor B mit Entprellung
  bool rawB = (digitalRead(PIN_SENSOR_B) == (SENSOR_ACTIVE_LOW ? LOW : HIGH));
  if (rawB != lastSensorB) {
    lastDebounceTimeB = now;
  }
  if ((now - lastDebounceTimeB) > DEBOUNCE_TIME) {
    sensorBState = rawB;
  }
  lastSensorB = rawB;
  
  // Zustandsmaschine für Richtungserkennung
  static bool lastProcessedA = false;
  static bool lastProcessedB = false;
  
  switch (directionState) {
    case STATE_IDLE:
      if (sensorAState && !sensorBState) {
        directionState = STATE_A_FIRST;
        if (DEBUG_MODE) Serial.println(F("[DIR] A first detected"));
      } else if (sensorBState && !sensorAState) {
        directionState = STATE_B_FIRST;
        if (DEBUG_MODE) Serial.println(F("[DIR] B first detected"));
      }
      break;
      
    case STATE_A_FIRST:
      if (sensorBState) {
        // Vollständige Vorwärtsbewegung
        counter++;
        forwardCounts++;
        totalCounts++;
        updateStatistics();
        beep(30, 2500);
        if (DEBUG_MODE) {
          Serial.print(F("[COUNT] Vorwaerts: "));
          Serial.println(counter);
        }
        directionState = STATE_IDLE;
      } else if (!sensorAState) {
        // Abort - unvollständige Bewegung
        directionState = STATE_IDLE;
      }
      break;
      
    case STATE_B_FIRST:
      if (sensorAState) {
        // Vollständige Rückwärtsbewegung
        counter--;
        backwardCounts++;
        totalCounts++;
        updateStatistics();
        beep(30, 1500);
        if (DEBUG_MODE) {
          Serial.print(F("[COUNT] Rueckwaerts: "));
          Serial.println(counter);
        }
        directionState = STATE_IDLE;
      } else if (!sensorBState) {
        // Abort - unvollständige Bewegung
        directionState = STATE_IDLE;
      }
      break;
  }
}

/**
 * Zeigt detaillierte Statistiken über serielle Schnittstelle an
 */
void printStatistics() {
  if (!DEBUG_MODE) return;
  
  Serial.println(F("\n=== SYSTEM STATISTIK ==="));
  Serial.print(F("Zaehlerstand: "));
  Serial.println(counter);
  Serial.print(F("Vorwaerts: "));
  Serial.println(forwardCounts);
  Serial.print(F("Rueckwaerts: "));
  Serial.println(backwardCounts);
  Serial.print(F("Gesamtbewegungen: "));
  Serial.println(totalCounts);
  Serial.print(F("Aktuelle Rate: "));
  Serial.print(currentRate, 1);
  Serial.println(F(" Staebe/Min"));
  unsigned long runtime = (millis() - programStartTime) / 1000;
  Serial.print(F("Betriebszeit: "));
  Serial.print(runtime / 60);
  Serial.print(F(" min "));
  Serial.print(runtime % 60);
  Serial.println(F(" sec"));
  Serial.println(F("=======================\n"));
}

// ==================== SETUP ====================
void setup() {
  // Pin-Konfiguration
  pinMode(PIN_SENSOR_A, INPUT);
  pinMode(PIN_SENSOR_B, INPUT);
  pinMode(PIN_RESET, INPUT_PULLUP);
  pinMode(PIN_MENU, INPUT_PULLUP);
  if (USE_BUZZER) pinMode(PIN_BUZZER, OUTPUT);
  
  // Serielle Schnittstelle für Debug
  if (DEBUG_MODE) {
    Serial.begin(115200);
    Serial.println(F("\n[START] Bidirektionaler Zaehler V2.0"));
    Serial.println(F("[START] Autor: Amir Mobasheraghdam"));
  }
  
  // LCD Initialisierung
  lcd.init();
  lcd.backlight();
  lcd.clear();
  lcd.print(F("Bidir. Zaehler"));
  lcd.setCursor(0, 1);
  lcd.print(F("V2.0 - Starting"));
  delay(1500);
  
  // EEPROM laden
  loadFromEEPROM();
  
  // Statistik initialisieren
  programStartTime = millis();
  lastRateCalc = programStartTime;
  lastRateCounter = counter;
  forwardCounts = (counter > 0) ? counter : 0;
  backwardCounts = (counter < 0) ? -counter : 0;
  totalCounts = forwardCounts + backwardCounts;
  
  // Begrüßungston
  beep(100, 2000);
  delay(100);
  beep(100, 2500);
  
  // Hauptbildschirm anzeigen
  updateDisplay();
  
  if (DEBUG_MODE) printStatistics();
}

// ==================== MAIN LOOP ====================
void loop() {
  unsigned long now = millis();
  
  // Sensoren verarbeiten (priorität)
  processSensors();
  
  // Menü nur verarbeiten wenn nicht in kritischer Phase
  if (directionState == STATE_IDLE) {
    handleMenu();
  }
  
  // Display periodisch aktualisieren (nicht bei aktivem Menü)
  if (!menuActive && (now - lastDisplayUpdate >= DISPLAY_UPDATE_INTERVAL)) {
    updateDisplay();
    lastDisplayUpdate = now;
  }
  
  // Automatisches Speichern im EEPROM
  if (now - lastSaveTime >= SAVE_INTERVAL) {
    saveToEEPROM();
    lastSaveTime = now;
  }
  
  // Serieller Befehlsserver (für Debug)
  if (DEBUG_MODE && Serial.available()) {
    char cmd = Serial.read();
    switch (cmd) {
      case 's': // Status
        printStatistics();
        break;
      case 'r': // Reset
        counter = 0;
        saveToEEPROM();
        updateDisplay();
        Serial.println(F("[CMD] Counter reset"));
        break;
      case 'p': // Print current count
        Serial.print(F("[CMD] Counter: "));
        Serial.println(counter);
        break;
    }
  }
}
