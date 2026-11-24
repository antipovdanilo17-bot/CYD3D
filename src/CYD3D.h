#ifndef CYD3D_H
#define CYD3D_H

#include <Arduino.h>
#include <TFT_eSPI.h>
#include <math.h>

#define CYD_T_CLK   25
#define CYD_T_MISO  39
#define CYD_T_MOSI  32
#define CYD_T_CS    33
#define CYD_T_IRQ   36
#define CYD_BL      21

struct Vec3 { float x, y, z; };
struct Tri3 { Vec3 v[3]; uint16_t color; };
struct Tri2 { int16_t x[3], y[3]; float depth; uint16_t color; };

class CYD3D {
public:
  TFT_eSPI tft;

private:
  TFT_eSprite spr;
  bool buffered = false;
  int bx = 0, by = 0, bw = 0, bh = 0;

  uint8_t rot = 1;
  int W = 320, H = 240;

  float fov = 380.0f, camZ = 320.0f;
  float rx = 0, ry = 0, rz = 0;
  float lx = 0.4f, ly = -0.6f, lz = -0.7f;

  static const int MAXF = 96;
  Tri2 buf[MAXF];
  int n = 0;

  int px0 = 0, py0 = 0, px1 = 319, py1 = 239;
  int cx0 = 9999, cy0 = 9999, cx1 = -1, cy1 = -1;
  bool first = true;

  int calX0 = 200, calX1 = 3900;
  int calY0 = 240, calY1 = 3800;
  bool invX = false, invY = false;

  uint16_t rd(uint8_t cmd) {
    digitalWrite(CYD_T_CS, LOW);
    digitalWrite(CYD_T_CLK, LOW);
    for (int8_t i = 7; i >= 0; i--) {
      digitalWrite(CYD_T_MOSI, (cmd >> i) & 1);
      delayMicroseconds(1);
      digitalWrite(CYD_T_CLK, HIGH); delayMicroseconds(1);
      digitalWrite(CYD_T_CLK, LOW);  delayMicroseconds(1);
    }
    digitalWrite(CYD_T_MOSI, LOW);
    digitalWrite(CYD_T_CLK, HIGH); delayMicroseconds(1);
    digitalWrite(CYD_T_CLK, LOW);  delayMicroseconds(1);

    uint16_t v = 0;
    for (uint8_t i = 0; i < 12; i++) {
      digitalWrite(CYD_T_CLK, HIGH); delayMicroseconds(1);
      v = (v << 1) | (digitalRead(CYD_T_MISO) ? 1 : 0);
      digitalWrite(CYD_T_CLK, LOW);  delayMicroseconds(1);
    }
    digitalWrite(CYD_T_CS, HIGH);
    return v;
  }

  static uint8_t e5(uint8_t v) { return (v << 3) | (v >> 2); }
  static uint8_t e6(uint8_t v) { return (v << 2) | (v >> 2); }

  int centerX() { return buffered ? bx + bw / 2 : W / 2; }
  int centerY() { return buffered ? by + bh / 2 : H / 2; }

public:
  CYD3D() : spr(&tft) {}

  void begin(uint8_t rotation = 1) {
    pinMode(CYD_BL, OUTPUT);
    digitalWrite(CYD_BL, HIGH);

    tft.init();
    tft.setRotation(rotation);
    tft.fillScreen(TFT_BLACK);
    rot = rotation;
    W = tft.width();
    H = tft.height();

    pinMode(CYD_T_CS, OUTPUT);   digitalWrite(CYD_T_CS, HIGH);
    pinMode(CYD_T_CLK, OUTPUT);  digitalWrite(CYD_T_CLK, LOW);
    pinMode(CYD_T_MOSI, OUTPUT); digitalWrite(CYD_T_MOSI, LOW);
    pinMode(CYD_T_MISO, INPUT);
    pinMode(CYD_T_IRQ, INPUT);

    px1 = W - 1; py1 = H - 1;
    Serial.printf("[CYD3D] %dx%d rot=%d, heap=%u\n", W, H, rot, ESP.getFreeHeap());
  }

  // Виділити буфер кадру. Повертає false, якщо RAM не вистачило.
  bool enableBuffer(int x, int y, int w, int h) {
    spr.setColorDepth(16);
    if (spr.createSprite(w, h) == nullptr) {
      Serial.printf("[CYD3D] буфер %dx%d НЕ виділився (heap=%u)\n", w, h, ESP.getFreeHeap());
      buffered = false;
      return false;
    }
    bx = x; by = y; bw = w; bh = h;
    spr.fillSprite(TFT_BLACK);
    buffered = true;
    Serial.printf("[CYD3D] буфер %dx%d OK, heap=%u\n", w, h, ESP.getFreeHeap());
    return true;
  }

  bool isBuffered() { return buffered; }
  int width()  { return W; }
  int height() { return H; }

  // ---------------- ТАЧ ----------------
  void setTouchCal(int x0, int x1, int y0, int y1) {
    calX0 = x0; calX1 = x1; calY0 = y0; calY1 = y1;
  }
  void invertTouch(bool x, bool y) { invX = x; invY = y; }

  bool getTouch(int &x, int &y) {
    if (rd(0xB3) < 300) return false;
    rd(0xD3);
    uint32_t sx = 0, sy = 0;
    for (uint8_t i = 0; i < 4; i++) { sx += rd(0xD3); sy += rd(0x93); }
    uint16_t rawX = sx / 4, rawY = sy / 4;
    if (rawX < 100 || rawY < 100) return false;

    int mx, my;
    if (rot & 1) {
      mx = map(rawY, calY0, calY1, 0, W);
      my = map(rawX, calX0, calX1, H, 0);
    } else {
      mx = map(rawX, calX0, calX1, 0, W);
      my = map(rawY, calY0, calY1, 0, H);
    }
    if (invX) mx = W - 1 - mx;
    if (invY) my = H - 1 - my;

    x = constrain(mx, 0, W - 1);
    y = constrain(my, 0, H - 1);
    return true;
  }

  bool touched() { int a, b; return getTouch(a, b); }

  // ---------------- ЕКРАН ----------------
  void clear(uint16_t bg = TFT_BLACK) {
    if (buffered) {
      spr.fillSprite(bg);
    } else if (first) {
      tft.fillScreen(bg);
    } else {
      int x = max(0, px0 - 2), y = max(0, py0 - 2);
      int w = min(W - 1, px1 + 2) - x + 1;
      int h = min(H - 1, py1 + 2) - y + 1;
      if (w > 0 && h > 0) tft.fillRect(x, y, w, h, bg);
    }
    cx0 = 9999; cy0 = 9999; cx1 = -1; cy1 = -1;
  }

  void text(const char* s, int x, int y, uint16_t c, uint8_t size = 2) {
    if (buffered && x >= bx && x < bx + bw && y >= by && y < by + bh) {
      spr.setTextDatum(TL_DATUM);
      spr.setTextColor(c);
      spr.drawString(s, x - bx, y - by, size);
    } else {
      tft.setTextDatum(TL_DATUM);
      tft.setTextColor(c, TFT_BLACK);
      tft.drawString(s, x, y, size);
    }
  }

  void textCenter(const char* s, int x, int y, uint16_t c, uint8_t size = 2) {
    if (buffered && x >= bx && x < bx + bw && y >= by && y < by + bh) {
      spr.setTextDatum(MC_DATUM);
      spr.setTextColor(c);
      spr.drawString(s, x - bx, y - by, size);
    } else {
      tft.setTextDatum(MC_DATUM);
      tft.setTextColor(c, TFT_BLACK);
      tft.drawString(s, x, y, size);
    }
  }

  bool button(int x, int y, int w, int h, const char* label, uint16_t col = TFT_NAVY) {
    int tx, ty;
    bool hit = false;
    if (getTouch(tx, ty))
      hit = (tx >= x && tx <= x + w && ty >= y && ty <= y + h);

    uint16_t bg = hit ? TFT_DARKGREY : col;
    bool inBuf = buffered && x >= bx && y >= by && (x + w) <= bx + bw && (y + h) <= by + bh;

    if (inBuf) {
      int dx = x - bx, dy = y - by;
      spr.fillRoundRect(dx, dy, w, h, 5, bg);
      spr.drawRoundRect(dx, dy, w, h, 5, TFT_WHITE);
      spr.setTextDatum(MC_DATUM);
      spr.setTextColor(TFT_WHITE);
      spr.drawString(label, dx + w / 2, dy + h / 2, 2);
    } else {
      tft.fillRoundRect(x, y, w, h, 5, bg);
      tft.drawRoundRect(x, y, w, h, 5, TFT_WHITE);
      tft.setTextDatum(MC_DATUM);
      tft.setTextColor(TFT_WHITE);
      tft.drawString(label, x + w / 2, y + h / 2, 2);
    }
    return hit;
  }

  // ---------------- 3D ----------------
  void setRotation(float ax, float ay, float az = 0) { rx = ax; ry = ay; rz = az; }
  void addRotation(float dx, float dy) { rx += dx; ry += dy; }
  void setLight(float x, float y, float z) { lx = x; ly = y; lz = z; }
  void setCamera(float dist, float focal) { camZ = dist; fov = focal; }

  Vec3 rotate(Vec3 p) {
    float cx = cos(rx), sx = sin(rx);
    float cy = cos(ry), sy = sin(ry);
    float cz = cos(rz), sz = sin(rz);
    float y1 = p.y * cx - p.z * sx;
    float z1 = p.y * sx + p.z * cx;
    float x2 = p.x * cy + z1 * sy;
    float z2 = -p.x * sy + z1 * cy;
    Vec3 r;
    r.x = x2 * cz - y1 * sz;
    r.y = x2 * sz + y1 * cz;
    r.z = z2;
    return r;
  }

  bool project(Vec3 p, int &sx, int &sy) {
    float z = p.z + camZ;
    if (z <= 1) return false;
    float f = fov / z;
    sx = (int)(p.x * f + centerX());
    sy = (int)(-p.y * f + centerY());
    return true;
  }

  void triangle(Tri3 t) {
    if (n >= MAXF) return;
    Vec3 r[3];
    for (int i = 0; i < 3; i++) r[i] = rotate(t.v[i]);

    Tri2 f;
    float sumZ = 0;
    for (int i = 0; i < 3; i++) {
      int sx, sy;
      if (!project(r[i], sx, sy)) return;
      f.x[i] = (int16_t)sx; f.y[i] = (int16_t)sy;
      sumZ += r[i].z;
      cx0 = min(cx0, sx); cx1 = max(cx1, sx);
      cy0 = min(cy0, sy); cy1 = max(cy1, sy);
    }
    f.depth = sumZ / 3.0f;

    Vec3 u = {r[1].x-r[0].x, r[1].y-r[0].y, r[1].z-r[0].z};
    Vec3 v = {r[2].x-r[0].x, r[2].y-r[0].y, r[2].z-r[0].z};
    Vec3 nn = {u.y*v.z - u.z*v.y, u.z*v.x - u.x*v.z, u.x*v.y - u.y*v.x};
    float len = sqrt(nn.x*nn.x + nn.y*nn.y + nn.z*nn.z);
    if (len > 0) { nn.x/=len; nn.y/=len; nn.z/=len; }

    float d = nn.x*lx + nn.y*ly + nn.z*lz;
    if (d < 0) d = -d;
    float br = 0.30f + 0.70f * d;
    if (br > 1.0f) br = 1.0f;

    uint8_t R = (uint8_t)(e5((t.color >> 11) & 0x1F) * br);
    uint8_t G = (uint8_t)(e6((t.color >>  5) & 0x3F) * br);
    uint8_t B = (uint8_t)(e5( t.color        & 0x1F) * br);
    f.color = tft.color565(R, G, B);

    buf[n++] = f;
  }

  void render() {
    for (int i = 0; i < n - 1; i++)
      for (int j = i + 1; j < n; j++)
        if (buf[i].depth < buf[j].depth) {
          Tri2 t = buf[i]; buf[i] = buf[j]; buf[j] = t;
        }

    if (buffered) {
      for (int i = 0; i < n; i++)
        spr.fillTriangle(buf[i].x[0] - bx, buf[i].y[0] - by,
                         buf[i].x[1] - bx, buf[i].y[1] - by,
                         buf[i].x[2] - bx, buf[i].y[2] - by, buf[i].color);
      spr.pushSprite(bx, by);          // ← весь кадр за один раз
    } else {
      for (int i = 0; i < n; i++)
        tft.fillTriangle(buf[i].x[0], buf[i].y[0],
                         buf[i].x[1], buf[i].y[1],
                         buf[i].x[2], buf[i].y[2], buf[i].color);
      if (cx1 >= cx0) {
        px0 = cx0; py0 = cy0; px1 = cx1; py1 = cy1;
        first = false;
      }
    }
    n = 0;
  }

  // ---------------- ФІГУРИ ----------------
  void cube(float s, uint16_t color = 0) {
    Vec3 v[8] = {
      {-s,-s,-s},{s,-s,-s},{s,s,-s},{-s,s,-s},
      {-s,-s, s},{s,-s, s},{s,s, s},{-s,s, s}};
    int id[12][3] = {
      {0,1,2},{0,2,3},{5,4,7},{5,7,6},
      {4,0,3},{4,3,7},{1,5,6},{1,6,2},
      {3,2,6},{3,6,7},{4,5,1},{4,1,0}};
    uint16_t pal[6] = {TFT_RED, TFT_GREEN, TFT_BLUE,
                       TFT_YELLOW, TFT_CYAN, TFT_MAGENTA};
    for (int i = 0; i < 12; i++) {
      Tri3 t;
      t.v[0]=v[id[i][0]]; t.v[1]=v[id[i][1]]; t.v[2]=v[id[i][2]];
      t.color = color ? color : pal[i/2];
      triangle(t);
    }
  }

  void pyramid(float s, uint16_t color = 0) {
    Vec3 v[5] = {{0,s,0},{-s,-s,-s},{s,-s,-s},{s,-s,s},{-s,-s,s}};
    int id[6][3] = {{0,1,2},{0,2,3},{0,3,4},{0,4,1},{1,4,3},{1,3,2}};
    uint16_t pal[6] = {TFT_RED, TFT_GREEN, TFT_BLUE,
                       TFT_YELLOW, TFT_CYAN, TFT_MAGENTA};
    for (int i = 0; i < 6; i++) {
      Tri3 t;
      t.v[0]=v[id[i][0]]; t.v[1]=v[id[i][1]]; t.v[2]=v[id[i][2]];
      t.color = color ? color : pal[i];
      triangle(t);
    }
  }

  void octahedron(float s, uint16_t color = 0) {
    Vec3 v[6] = {{0,s,0},{0,-s,0},{s,0,0},{-s,0,0},{0,0,s},{0,0,-s}};
    int id[8][3] = {
      {0,2,4},{0,4,3},{0,3,5},{0,5,2},
      {1,4,2},{1,3,4},{1,5,3},{1,2,5}};
    uint16_t pal[8] = {TFT_RED, TFT_GREEN, TFT_BLUE, TFT_YELLOW,
                       TFT_CYAN, TFT_MAGENTA, TFT_ORANGE, TFT_PURPLE};
    for (int i = 0; i < 8; i++) {
      Tri3 t;
      t.v[0]=v[id[i][0]]; t.v[1]=v[id[i][1]]; t.v[2]=v[id[i][2]];
      t.color = color ? color : pal[i];
      triangle(t);
    }
  }
};

#endif