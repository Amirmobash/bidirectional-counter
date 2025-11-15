# bidirectional-counter
## 📝 Updated `README.md` (with your name & website)

```md
# Bidirectional Wooden Stick Counter (Arduino + IR Sensors)

Author: **Amir Mobasheraghdam**  
Website: [www.nivta.de](https://www.nivta.de)

This project implements a **bidirectional counter** using two E18-D80NK infrared proximity sensors,  
an Arduino board (Uno/Nano), and a 16x2 LCD with I2C interface.

The system detects objects passing in front of the sensors and determines the **direction of movement**:
- Moving **forward** → count increases (+1)  
- Moving **backward** → count decreases (–1)

The project is useful for:
- Conveyor belt counters  
- Stock counting  
- Position tracking  
- Manual sliding counters  

---

## ✨ Features

- ✔ Bidirectional counting using two IR sensors  
- ✔ Debounced state-machine logic (no double counts, no jitter)  
- ✔ 16x2 LCD display with real-time status  
- ✔ Reset button support  
- ✔ Works with E18-D80NK (5V-compatible IR sensor)  
- ✔ Noise-proof and stable even at slow or fast movement  

---

## 📦 Hardware Required

| Component                   | Quantity | Notes |
|-----------------------------|----------|-------|
| Arduino Uno or Nano        | 1        | 5V logic |
| E18-D80NK IR Sensor        | 2        | Works at 5V input |
| LCD 16x2 with I2C module   | 1        | PCF8574-based |
| Push Button                | 1        | Reset count |
| Jumper Wires               | Several  | Male/Female |
| 5V power via USB           | 1        | Arduino supplies sensors |

---

## 🔌 Wiring Diagram

### IR Sensor Wiring (Both Sensors)

E18-D80NK cable colors:

| Sensor Wire | Arduino Pin |
|-------------|-------------|
| Brown       | 5V          |
| Blue        | GND         |
| Black       | D2 (Sensor A), D3 (Sensor B) |

> Sensor output:  
> - **5V = No Object**  
> - **0V = Object Detected**

### LCD I2C

| LCD Pin | Arduino Pin |
|---------|-------------|
| VCC     | 5V          |
| GND     | GND         |
| SDA     | A4          |
| SCL     | A5          |

### Reset Button

| Button Pin | Arduino Pin |
|------------|-------------|
| One side   | GND |
| Other side | D4  |

---

## 🔧 How It Works (Direction Detection Logic)

The two sensors are positioned side-by-side:

```

[SENSOR A]   [SENSOR B]

```

### Forward movement (→)
```

A detects first → then B detects → COUNT +1

```

### Backward movement (←)
```

B detects first → then A detects → COUNT –1

```

A simple **state machine** ensures:
- No double-counting  
- No bouncing  
- No jitter when moving slowly  
- No random transitions from sensor noise  

---

## 🧠 State Machine

```

IDLE
├── A detects first → A_FIRST
│       └── B detects → COUNT++
│                 └── return to IDLE
└── B detects first → B_FIRST
└── A detects → COUNT--
└── return to IDLE

````

Anything else resets to IDLE (noise rejection).

---

## 🧾 Full Arduino Code

File: `arduino/bidirectional_counter.ino`

```cpp
/**
 * Bidirectional Wooden Stick Counter
 *
 * Author: Amir Mobasheraghdam
 * Website: https://www.nivta.de
 *
 * Description:
 *  - Uses two E18-D80NK IR sensors and a 16x2 I2C LCD
 *  - Counts objects moving in front of the sensors
 *  - Forward motion:  count++
 *  - Backward motion: count--
 */

#include <Wire.h>
#include <LiquidCrystal_I2C.h>

LiquidCrystal_I2C lcd(0x27, 16, 2);

// Pins
const int A_pin = 2;   // Sensor A
const int B_pin = 3;   // Sensor B
const int resetBtn = 4;

// Variables
long count = 0;
bool A_now, B_now;

// State machine for direction
enum State {IDLE, A_FIRST, B_FIRST};
State state = IDLE;

void setup() {
  pinMode(A_pin, INPUT);      // E18-D80NK: 0V (object), 5V (no object)
  pinMode(B_pin, INPUT);
  pinMode(resetBtn, INPUT_PULLUP);

  lcd.init();
  lcd.backlight();
  lcd.clear();
  lcd.print("BiDir Counter");
  lcd.setCursor(0, 1);
  lcd.print("by A. Mobasher");
  delay(1000);
  lcd.clear();
}

void loop() {
  // Active = LOW (object detected)
  A_now = (digitalRead(A_pin) == LOW);
  B_now = (digitalRead(B_pin) == LOW);

  // Display live sensor state (for debugging)
  lcd.setCursor(0, 0);
  lcd.print("A:");
  lcd.print(A_now);
  lcd.print(" B:");
  lcd.print(B_now);
  lcd.print("   ");

  switch (state) {
    case IDLE:
      // Start forward movement: A first
      if (A_now && !B_now) {
        state = A_FIRST;
      }
      // Start backward movement: B first
      else if (B_now && !A_now) {
        state = B_FIRST;
      }
      break;

    case A_FIRST:
      // A then B -> forward (+1)
      if (B_now) {
        count++;
        showCount();
        state = IDLE;
      }
      // Cancel if A is no longer active (noise or incomplete pass)
      else if (!A_now) {
        state = IDLE;
      }
      break;

    case B_FIRST:
      // B then A -> backward (-1)
      if (A_now) {
        count--;
        showCount();
        state = IDLE;
      }
      // Cancel if B is no longer active
      else if (!B_now) {
        state = IDLE;
      }
      break;
  }

  // Reset button
  if (digitalRead(resetBtn) == LOW) {
    delay(50);
    if (digitalRead(resetBtn) == LOW) {
      count = 0;
      showCount();
      while (digitalRead(resetBtn) == LOW); // wait until released
    }
  }
}

void showCount() {
  lcd.setCursor(0, 1);
  lcd.print("COUNT: ");
  lcd.print(count);
  lcd.print("      ");
}
````

---

## 📜 LICENSE (MIT with your name & website)

Put this into a file named `LICENSE`:

```text
MIT License

Copyright (c) 2025 Amir Mobasheraghdam (www.nivta.de)

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
```

---

