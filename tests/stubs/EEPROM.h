#pragma once
struct EEPROMStub {
  template <typename T> void get(int, T& value) { value = 0; }
  template <typename T> void put(int, const T&) {}
};
static EEPROMStub EEPROM;
