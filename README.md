# CYD3D

Lightweight 3D software rendering engine for **ESP32 Cheap Yellow Display (CYD / ESP32-2432S028R)**.

## Features
* **3D Triangle Rasterization:** Depth sorting (Painter's algorithm) and directional lighting.
* **Built-in Primitives:** Ready-to-use `cube()`, `pyramid()`, and `octahedron()` meshes, plus custom `triangle()` support.
* **Flicker-Free Rendering:** Optional sprite-based double buffering via `TFT_eSprite` (`enableBuffer()`).
* **Touchscreen & UI:** Built-in resistive touch driver (XPT2046 bit-banged SPI) with calibration and interactive `button()` helper.

## Hardware Pinout (CYD Default)
The library comes pre-configured for standard ESP32-2432S028R boards:
* **Backlight:** GPIO 21
* **Touch CLK:** GPIO 25
* **Touch MISO:** GPIO 39
* **Touch MOSI:** GPIO 32
* **Touch CS:** GPIO 33
* **Touch IRQ:** GPIO 36

## Installation
1. Install **TFT_eSPI** via the Arduino Library Manager and configure its `User_Setup.h` for your CYD display.
2. Search for **CYD3D** in the Arduino Library Manager and click **Install**.

## Quick Example

```cpp
#include <CYD3D.h>

CYD3D engine;

void setup() {
  Serial.begin(115200);
  engine.begin(1); // Landscape mode (320x240)
  
  // Enable sprite buffer for smooth animation
  engine.enableBuffer(0, 0, 320, 240);
}

void loop() {
  engine.clear(TFT_BLACK);
  
  engine.addRotation(0.03f, 0.05f);
  engine.cube(55.0f);
  
  engine.render();
  delay(16);
}
```

## API Overview
* `begin(rotation)` — Initializes display, backlight, and touch pins.
* `enableBuffer(x, y, w, h)` — Allocates RAM sprite buffer to prevent screen tearing.
* `setCamera(dist, focal)` / `setLight(x, y, z)` — Configures 3D projection and light direction.
* `setRotation(ax, ay, az)` / `addRotation(dx, dy)` — Controls object rotation in radians.
* `cube(size, color)` / `pyramid(size, color)` / `octahedron(size, color)` — Draws 3D shapes (pass `0` for default multi-color faces).
* `getTouch(x, y)` / `button(x, y, w, h, label, color)` — Handles touch coordinates and UI buttons.

## License
This project is licensed under the MIT License — see the [LICENSE](LICENSE) file for details.
