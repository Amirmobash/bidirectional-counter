#pragma once
#include <sstream>
#include <string>

class LiquidCrystal_I2C {
 public:
  std::string rows[2] = {std::string(16, ' '), std::string(16, ' ')};
  int column = 0;
  int row = 0;
  LiquidCrystal_I2C(int, int, int) {}
  void init() {}
  void backlight() {}
  void clear() { rows[0] = rows[1] = std::string(16, ' '); column = row = 0; }
  void setCursor(int x, int y) { column = x; row = y; }
  template <typename T> void print(const T& value) {
    std::ostringstream text;
    text << value;
    for (char c : text.str()) {
      if (row >= 0 && row < 2 && column >= 0 && column < 16) rows[row][column] = c;
      ++column;
    }
  }
};
