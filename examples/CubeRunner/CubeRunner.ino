/*
 ============================================================================
  КУБ-РАННЕР 3D
  3D — через CYD3D.h (без змін)
  Тач — власний, точно як у робочому проекті
 ============================================================================
*/

#include "CYD3D.h"
#include <SPI.h>
#include <XPT2046_Touchscreen.h>

// ---- власний тач (копія робочої ініціалізації) ----
SPIClass touchSpi = SPIClass(VSPI);
XPT2046_Touchscreen ts(33, 36);

CYD3D cyd;

bool readTouch(int &x, int &y) {
  if (!ts.touched()) return false;
  TS_Point p = ts.getPoint();
  x = constrain(map(p.x, 200, 3900, 0, 320), 0, 319);
  y = constrain(map(p.y, 240, 3800, 0, 240), 0, 239);
  return true;
}

// ---------------- стан ----------------
enum State { MENU, PLAY, OVER };
State state = MENU;
bool menuDrawn = false, overDrawn = false;

const int BUF_Y = 32, BUF_H = 164;

const float LANE_X[3] = {-60, 0, 60};
const float FLOOR_Y   = -70;
const float ROAD_HALF = 100;
const float ROAD_NEAR = 40;
const float ROAD_FAR  = 850;
const float PLAYER_Z  = 80;
const int   SEG       = 8;

int   playerLane = 1;
float playerX = 0;
int   score = 0, best = 0;

struct Obstacle { bool active; int lane; float z; uint8_t type; };
const int MAX_OBS = 4;
Obstacle obs[MAX_OBS];

float speed = 6.5f, spawnMs = 1000.0f, dashOffset = 0;
unsigned long lastSpawn = 0, lastFrame = 0;

bool leftPressed = false, rightPressed = false;
int  uiScore = -1;
bool uiL = false, uiR = false;

// ============================================================
//  Геометрія
// ============================================================
void quad(Vec3 a, Vec3 b, Vec3 c, Vec3 d, uint16_t col) {
  Tri3 t; t.color = col;
  t.v[0]=a; t.v[1]=b; t.v[2]=c; cyd.triangle(t);
  t.v[0]=a; t.v[1]=c; t.v[2]=d; cyd.triangle(t);
}

void boxAt(float cx, float cy, float cz, float s, uint16_t col) {
  Vec3 v[8] = {
    {cx-s,cy-s,cz-s},{cx+s,cy-s,cz-s},{cx+s,cy+s,cz-s},{cx-s,cy+s,cz-s},
    {cx-s,cy-s,cz+s},{cx+s,cy-s,cz+s},{cx+s,cy+s,cz+s},{cx-s,cy+s,cz+s}
  };
  const int id[12][3] = {
    {0,1,2},{0,2,3},{5,4,7},{5,7,6},
    {4,0,3},{4,3,7},{1,5,6},{1,6,2},
    {3,2,6},{3,6,7},{4,5,1},{4,1,0}
  };
  for (int i = 0; i < 12; i++) {
    Tri3 t;
    t.v[0]=v[id[i][0]]; t.v[1]=v[id[i][1]]; t.v[2]=v[id[i][2]];
    t.color = col;
    cyd.triangle(t);
  }
}

void pyramidAt(float cx, float cy, float cz, float s, uint16_t col) {
  Vec3 apex = {cx, cy+s, cz};
  Vec3 b[4] = {
    {cx-s,cy-s,cz-s},{cx+s,cy-s,cz-s},{cx+s,cy-s,cz+s},{cx-s,cy-s,cz+s}
  };
  Tri3 t; t.color = col;
  for (int i = 0; i < 4; i++) {
    t.v[0]=apex; t.v[1]=b[i]; t.v[2]=b[(i+1)%4];
    cyd.triangle(t);
  }
  quad(b[0], b[1], b[2], b[3], col);
}

void octaAt(float cx, float cy, float cz, float s, uint16_t col) {
  Vec3 v[6] = {
    {cx,cy+s,cz},{cx,cy-s,cz},{cx+s,cy,cz},{cx-s,cy,cz},{cx,cy,cz+s},{cx,cy,cz-s}
  };
  const int id[8][3] = {
    {0,2,4},{0,4,3},{0,3,5},{0,5,2},
    {1,4,2},{1,3,4},{1,5,3},{1,2,5}
  };
  for (int i = 0; i < 8; i++) {
    Tri3 t;
    t.v[0]=v[id[i][0]]; t.v[1]=v[id[i][1]]; t.v[2]=v[id[i][2]];
    t.color = col;
    cyd.triangle(t);
  }
}

// ============================================================
//  UI (поза буфером, малюється лише при зміні)
// ============================================================
void drawScore() {
  uiScore = score;
  cyd.tft.fillRect(0, 0, 320, 30, TFT_BLACK);
  cyd.tft.setTextDatum(MC_DATUM);
  cyd.tft.setTextColor(TFT_CYAN, TFT_BLACK);
  cyd.tft.drawString("РАХУНОК: " + String(score), 160, 15, 2);
}

void drawLeftBtn() {
  uiL = leftPressed;
  uint16_t c = uiL ? TFT_BLUE : TFT_NAVY;
  cyd.tft.fillRoundRect(6, 200, 140, 34, 6, c);
  cyd.tft.drawRoundRect(6, 200, 140, 34, 6, TFT_WHITE);
  cyd.tft.setTextDatum(MC_DATUM);
  cyd.tft.setTextColor(TFT_WHITE, c);
  cyd.tft.drawString("< ЛIВО", 76, 217, 2);
}

void drawRightBtn() {
  uiR = rightPressed;
  uint16_t c = uiR ? TFT_BLUE : TFT_NAVY;
  cyd.tft.fillRoundRect(174, 200, 140, 34, 6, c);
  cyd.tft.drawRoundRect(174, 200, 140, 34, 6, TFT_WHITE);
  cyd.tft.setTextDatum(MC_DATUM);
  cyd.tft.setTextColor(TFT_WHITE, c);
  cyd.tft.drawString("ПРАВО >", 244, 217, 2);
}

void refreshUI() {
  if (score != uiScore)      drawScore();
  if (leftPressed != uiL)    drawLeftBtn();
  if (rightPressed != uiR)   drawRightBtn();
}

void buildPlayUI() {
  cyd.tft.fillRect(0, 0, 320, 32, TFT_BLACK);
  cyd.tft.fillRect(0, 196, 320, 44, TFT_BLACK);
  uiScore = -1; uiL = !leftPressed; uiR = !rightPressed;
  refreshUI();
}

// ============================================================
//  Керування
// ============================================================
void handleControls(int tx, int ty, bool touched) {
  bool inL = touched && tx <  160;
  bool inR = touched && tx >= 160;

  if (inL) {
    if (!leftPressed) { leftPressed = true; if (playerLane > 0) playerLane--; }
  } else leftPressed = false;

  if (inR) {
    if (!rightPressed) { rightPressed = true; if (playerLane < 2) playerLane++; }
  } else rightPressed = false;
}

// ============================================================
//  Логіка
// ============================================================
void initGame() {
  playerLane = 1; playerX = 0;
  score = 0; speed = 6.5f; spawnMs = 1000.0f; dashOffset = 0;
  leftPressed = rightPressed = false;
  for (int i = 0; i < MAX_OBS; i++) obs[i].active = false;
  lastSpawn = millis();
}

void spawnObstacle() {
  for (int i = 0; i < MAX_OBS; i++) {
    if (!obs[i].active) {
      obs[i].active = true;
      obs[i].lane = random(0, 3);
      obs[i].z    = ROAD_FAR;
      obs[i].type = random(0, 3);
      return;
    }
  }
}

void update() {
  speed += 0.0025f;
  if (spawnMs > 420.0f) spawnMs -= 0.12f;

  for (int i = 0; i < MAX_OBS; i++) {
    if (!obs[i].active) continue;
    obs[i].z -= speed;
    if (obs[i].z < PLAYER_Z - 30) { obs[i].active = false; score += 10; continue; }
    if (fabs(obs[i].z - PLAYER_Z) < 25.0f &&
        fabs(LANE_X[obs[i].lane] - playerX) < 36.0f) {
      state = OVER; overDrawn = false;
      if (score > best) best = score;
    }
  }

  playerX += (LANE_X[playerLane] - playerX) * 0.45f;

  if (millis() - lastSpawn > (unsigned long)spawnMs) {
    spawnObstacle(); lastSpawn = millis();
  }

  float segLen = (ROAD_FAR - ROAD_NEAR) / SEG;
  dashOffset += speed;
  if (dashOffset > segLen) dashOffset -= segLen;
}

// ============================================================
//  Сцена
// ============================================================
void drawScene() {
  cyd.clear(TFT_BLACK);
  cyd.setRotation(-0.22f, 0, 0);

  float segLen = (ROAD_FAR - ROAD_NEAR) / SEG;
  for (int i = 0; i < SEG; i++) {
    float z0 = ROAD_NEAR + i * segLen - dashOffset;
    float z1 = z0 + segLen;
    if (z1 <= ROAD_NEAR) continue;
    if (z0 < ROAD_NEAR) z0 = ROAD_NEAR;
    quad({-ROAD_HALF,FLOOR_Y,z0},{ROAD_HALF,FLOOR_Y,z0},
         {ROAD_HALF,FLOOR_Y,z1},{-ROAD_HALF,FLOOR_Y,z1}, (i & 1) ? 0x2104 : 0x1082);
  }

  float hx = LANE_X[playerLane];
  quad({hx-28,FLOOR_Y+0.3f,PLAYER_Z-28},{hx+28,FLOOR_Y+0.3f,PLAYER_Z-28},
       {hx+28,FLOOR_Y+0.3f,PLAYER_Z+28},{hx-28,FLOOR_Y+0.3f,PLAYER_Z+28}, 0x0320);

  for (int k = 0; k < 2; k++) {
    float lx = (k == 0) ? -30.0f : 30.0f;
    for (int i = 0; i < SEG; i += 4) {
      float z0 = ROAD_NEAR + i * segLen - dashOffset;
      float z1 = z0 + segLen * 0.5f;
      if (z1 <= ROAD_NEAR) continue;
      if (z0 < ROAD_NEAR) z0 = ROAD_NEAR;
      quad({lx-2,FLOOR_Y+0.6f,z0},{lx+2,FLOOR_Y+0.6f,z0},
           {lx+2,FLOOR_Y+0.6f,z1},{lx-2,FLOOR_Y+0.6f,z1}, TFT_DARKGREY);
    }
  }

  for (int i = 0; i < MAX_OBS; i++) {
    if (!obs[i].active || obs[i].z < PLAYER_Z - 40) continue;
    float x = LANE_X[obs[i].lane], z = obs[i].z;
    switch (obs[i].type) {
      case 0: boxAt    (x, FLOOR_Y+24, z, 24, TFT_RED);    break;
      case 1: pyramidAt(x, FLOOR_Y+26, z, 26, TFT_ORANGE); break;
      case 2: octaAt   (x, FLOOR_Y+24, z, 24, TFT_PURPLE); break;
    }
  }

  boxAt(playerX, FLOOR_Y + 26, PLAYER_Z, 26, TFT_GREEN);
  cyd.render();
}

// ============================================================
//  SETUP
// ============================================================
void setup() {
  Serial.begin(115200);
  randomSeed(analogRead(34));

  cyd.begin(1);
  cyd.enableBuffer(0, BUF_Y, 320, BUF_H);
  cyd.setCamera(300, 350);
  cyd.setLight(0.4f, -0.6f, -0.7f);

  // власна ініціалізація тачу — як у робочому проекті
  touchSpi.begin(25, 39, 32, 33);
  ts.begin(touchSpi);
  ts.setRotation(1);

  cyd.tft.fillScreen(TFT_BLACK);
  initGame();
}

// ============================================================
//  LOOP
// ============================================================
void loop() {

  // ---------- МЕНЮ з діагностикою дотику ----------
  if (state == MENU) {
    if (!menuDrawn) {
      cyd.tft.fillScreen(TFT_BLACK);
      cyd.tft.setTextDatum(MC_DATUM);
      cyd.tft.setTextColor(TFT_CYAN, TFT_BLACK);
      cyd.tft.drawString("КУБ-РАННЕР 3D", 160, 40, 4);
      cyd.tft.setTextColor(TFT_YELLOW, TFT_BLACK);
      cyd.tft.drawString("Торкнись екрана - побачиш координати", 160, 80, 1);
      cyd.tft.fillRoundRect(110, 180, 100, 40, 6, TFT_DARKGREEN);
      cyd.tft.drawRoundRect(110, 180, 100, 40, 6, TFT_WHITE);
      cyd.tft.setTextColor(TFT_WHITE, TFT_DARKGREEN);
      cyd.tft.drawString("СТАРТ", 160, 200, 2);
      menuDrawn = true;
    }

    int tx, ty;
    if (readTouch(tx, ty)) {
      cyd.tft.fillRect(0, 110, 320, 30, TFT_BLACK);
      cyd.tft.setTextDatum(MC_DATUM);
      cyd.tft.setTextColor(TFT_GREEN, TFT_BLACK);
      cyd.tft.drawString("TOUCH  X=" + String(tx) + "  Y=" + String(ty), 160, 125, 2);
      cyd.tft.fillCircle(tx, ty, 3, TFT_RED);

      if (tx >= 110 && tx <= 210 && ty >= 180 && ty <= 220) {
        initGame(); state = PLAY; buildPlayUI();
        delay(200);
      }
    }
    delay(5);
    return;
  }

  // ---------- GAME OVER ----------
  if (state == OVER) {
    if (!overDrawn) {
      cyd.tft.fillScreen(TFT_BLACK);
      cyd.tft.setTextDatum(MC_DATUM);
      cyd.tft.setTextColor(TFT_RED, TFT_BLACK);
      cyd.tft.drawString("ЗАКIНЧЕНО!", 160, 50, 4);
      cyd.tft.setTextColor(TFT_YELLOW, TFT_BLACK);
      cyd.tft.drawString("Рахунок: " + String(score), 160, 95, 2);
      cyd.tft.setTextColor(TFT_WHITE, TFT_BLACK);
      cyd.tft.drawString("Рекорд: " + String(best), 160, 120, 2);

      cyd.tft.fillRoundRect(55, 180, 95, 40, 6, TFT_NAVY);
      cyd.tft.drawRoundRect(55, 180, 95, 40, 6, TFT_WHITE);
      cyd.tft.setTextColor(TFT_WHITE, TFT_NAVY);
      cyd.tft.drawString("ЗАНОВО", 102, 200, 2);

      cyd.tft.fillRoundRect(170, 180, 95, 40, 6, TFT_DARKGREY);
      cyd.tft.drawRoundRect(170, 180, 95, 40, 6, TFT_WHITE);
      cyd.tft.setTextColor(TFT_WHITE, TFT_DARKGREY);
      cyd.tft.drawString("МЕНЮ", 217, 200, 2);
      overDrawn = true;
    }

    int tx, ty;
    if (readTouch(tx, ty)) {
      cyd.tft.fillCircle(tx, ty, 3, TFT_RED);
      if (ty >= 180 && ty <= 220) {
        if (tx >= 55 && tx <= 150)  { initGame(); state = PLAY; buildPlayUI(); delay(200); }
        if (tx >= 170 && tx <= 265) { state = MENU; menuDrawn = false; delay(200); }
      }
    }
    delay(5);
    return;
  }

  // ---------- ГРА ----------
  // щільне опитування дотику між кадрами
  unsigned long t0 = millis();
  while (millis() - t0 < 16) {
    int tx, ty;
    bool touched = readTouch(tx, ty);
    handleControls(tx, ty, touched);
    refreshUI();
    delay(2);
  }

  if (millis() - lastFrame >= 35) {
    lastFrame = millis();
    update();
    if (state == PLAY) drawScene();
    refreshUI();
  }
}