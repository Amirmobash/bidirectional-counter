// Test the production sketch with a recording LCD and inert hardware adapters.
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstdint>

using byte = uint8_t;
#define F(text) text
constexpr int HIGH = 1, LOW = 0, INPUT = 0, OUTPUT = 1, INPUT_PULLUP = 2;
unsigned long fakeTime = 0;
unsigned long millis() { return fakeTime; }
void delay(unsigned long interval) { fakeTime += interval; }
void pinMode(int, int) {}
int digitalRead(int) { return HIGH; }
void tone(int, int, int) {}
char* dtostrf(double value, int width, unsigned int precision, char* buffer) {
  std::snprintf(buffer, 17, "%*.*f", width, static_cast<int>(precision), value);
  return buffer;
}
struct SerialStub {
  void begin(int) {}
  int available() { return 0; }
  char read() { return 0; }
  template <typename... T> void print(T...) {}
  template <typename... T> void println(T...) {}
} Serial;

#include "../Arduino + IR Sensors.ino"

int main() {
  counter = 42;
  currentRate = 3.0f;
  updateDisplay();
  assert(lcd.rows[0].find("Staebe: 42") == 0);

  menuActive = true;
  currentMenu = MENU_STATS;
  showMenu();
  assert(lcd.rows[0].find("Forward:") == 0);
  currentMenu = MENU_EXIT;
  showMenu();
  assert(!menuActive);
  assert(lcd.rows[0].find("Staebe: 42") == 0);

  menuActive = true;
  currentMenu = MENU_MAIN;
  showMenu();
  fakeTime = MENU_TIMEOUT + 1;
  handleMenu();
  assert(!menuActive);
  assert(lcd.rows[0].find("Staebe: 42") == 0);

  // Idle refreshes still keep the cached screen when no menu has overwritten it.
  lcd.rows[1] = "unchanged       ";
  updateDisplay();
  assert(lcd.rows[1] == "unchanged       ");
}
