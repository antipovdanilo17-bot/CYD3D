#include "CYD3D.h"

CYD3D cyd;
int shape = 0;
const char* names[] = {"CUBE", "PYRAMID", "OCTA"};
bool drag = false;
int lx = 0, ly = 0;
unsigned long lastFrame = 0;

void setup() {
  Serial.begin(115200);
  cyd.begin(1);

  // повний кадр; якщо RAM мало — менша область; якщо і це ні — без буфера
  if (!cyd.enableBuffer(0, 0, 320, 240))
    if (!cyd.enableBuffer(0, 25, 320, 165))
      Serial.println("працюємо без буфера");

  cyd.setRotation(0.4f, 0.6f);
}

void loop() {
  if (millis() - lastFrame < 25) return;
  lastFrame = millis();

  int tx, ty;
  bool down = cyd.getTouch(tx, ty);

  if (down && ty < 180) {
    if (!drag) { lx = tx; ly = ty; drag = true; }
    else {
      cyd.addRotation((ty - ly) * 0.01f, (tx - lx) * 0.01f);
      lx = tx; ly = ty;
    }
  } else {
    drag = false;
    cyd.addRotation(0.004f, 0.010f);
  }

  cyd.clear(TFT_BLACK);
  switch (shape) {
    case 0: cyd.cube(60); break;
    case 1: cyd.pyramid(65); break;
    case 2: cyd.octahedron(70); break;
  }

  cyd.textCenter(names[shape], 160, 12, TFT_CYAN, 2);
  if (cyd.button(248, 195, 64, 30, "NEXT")) {
    shape = (shape + 1) % 3;
    delay(220);
  }

  cyd.render();   // кнопка і текст теж потрапляють у буфер, якщо всередині нього
}