#include <TFT_eSPI.h>
#include <math.h>

// ── Font includes (generate with truetype2gfx from Orbitron-Bold.ttf) ────────
// truetype2gfx -f Orbitron-Bold.ttf -s 20 -o Saira_Bold12pt7b.h
// truetype2gfx -f Orbitron-Bold.ttf -s 10 -o Saira_Bold9pt7b.h
#include "Fonts/Saira_Bold12pt7b.h"   // speed display
#include "Fonts/Saira_Bold9pt7b.h"   // gear display
#include "Fonts/Saira_Bold6pt7b.h"   // gear display

TFT_eSPI tft = TFT_eSPI();

// ── screen / gauge geometry ───────────────────────────────────────────────────
#define SCREEN_W      160
#define SCREEN_H      128
#define CX            80
#define CY            77
#define R             75
#define ARC_START_DEG 145
#define ARC_SWEEP_DEG 250
#define HOLE_R        36

#define SPR_W         SCREEN_W
#define SPR_H         SCREEN_H
#define SPR_OX        0
#define SPR_OY        0
#define SCX           CX
#define SCY           CY

// ── needle shape ─────────────────────────────────────────────────────────────
#define NEEDLE_LEN    (R - 14)
#define NEEDLE_BASE_W 6
#define NEEDLE_TIP_W  2
#define NEEDLE_TAIL   8

// ── colours ───────────────────────────────────────────────────────────────────
#define C_BG          TFT_BLACK
#define C_TRACK       0x18C3
#define C_RED_DIM     0x4000
#define C_TICK        0xAD55
#define C_TICK_RED    0xC180
#define C_LABEL       0x8C51
#define C_NEEDLE      0xEF7D
#define C_NEEDLE_R    0xF340
#define C_NEEDLE_OUT  0x2945
#define C_HUB         0xCE59
#define C_HUB2        0x39E7
#define C_HOLE        0x0841
#define C_DIVIDER     0x2104
#define C_SPD         TFT_WHITE
#define C_GEAR        TFT_WHITE
#define C_DIM         0x4208

// ── sprite + background buffer ───────────────────────────────────────────────
TFT_eSprite spr = TFT_eSprite(&tft);
uint16_t* bgBuf  = nullptr;
uint16_t* sprBuf = nullptr;
const int BUF_PIXELS = SPR_W * SPR_H;

// ── state ─────────────────────────────────────────────────────────────────────
int    lastRpm    = -999;
int    lastMaxRpm = -1;
int    lastSpeed  = -1;
String lastGear   = "";
float  smoothRpm  = 0.0f;

// ── helpers ───────────────────────────────────────────────────────────────────
float rpmToRad(float rpm, int maxRpm) {
  float frac = rpm / (float)maxRpm;
  frac = frac < 0 ? 0 : (frac > 1 ? 1 : frac);
  return (ARC_START_DEG + frac * ARC_SWEEP_DEG) * (float)M_PI / 180.0f;
}

String formatGear(const String &raw) {
  if (raw == "N" || raw == "R") return raw;
  if (raw.length() == 1 && raw[0] >= '1' && raw[0] <= '9') {
    return "M" + raw + "S";
  }
  return raw;
}

// ── thick arc ────────────────────────────────────────────────────────────────
void sprArc(TFT_eSprite &s, float startDeg, float sweepDeg, int r, int thick, uint16_t col) {
  int steps = max(6, (int)(sweepDeg * 1.8f));
  float sd = sweepDeg / steps;
  for (int i = 0; i <= steps; i++) {
    float a  = (startDeg + i * sd)       * (float)M_PI / 180.0f;
    float an = (startDeg + (i + 1) * sd) * (float)M_PI / 180.0f;
    for (int dr = 0; dr < thick; dr++) {
      int ri = r - dr;
      s.drawLine(
        SCX + (int)(cosf(a)  * ri), SCY + (int)(sinf(a)  * ri),
        SCX + (int)(cosf(an) * ri), SCY + (int)(sinf(an) * ri),
        col);
    }
  }
}

// ── static background ────────────────────────────────────────────────────────
void buildBgBuf(int maxRpm) {
  spr.fillSprite(C_BG);

  int redRpm = (int)(maxRpm * 0.875f);
  maxRpm = ((maxRpm + 999) / 1000) * 1000;

  sprArc(spr, ARC_START_DEG, ARC_SWEEP_DEG, R, 8, C_TRACK);

  float redFrac  = (float)redRpm / maxRpm;
  float redStart = ARC_START_DEG + redFrac * ARC_SWEEP_DEG;
  float redSweep = ARC_SWEEP_DEG * (1.0f - redFrac);
  sprArc(spr, redStart, redSweep, R, 8, C_RED_DIM);

  int kSteps = maxRpm / 1000;
  for (int i = 0; i < kSteps; i++) {
    for (int j = 1; j < 5; j++) {
      float rv = i * 1000 + j * 200;
      if (rv >= maxRpm) continue;
      float ang = rpmToRad(rv, maxRpm);
      bool  isR = (rv >= redRpm);
      spr.drawLine(
        SCX + (int)(cosf(ang) * (R - 6)), SCY + (int)(sinf(ang) * (R - 6)),
        SCX + (int)(cosf(ang) *  R),      SCY + (int)(sinf(ang) *  R),
        isR ? 0x5000 : 0x2104);
    }
  }

  for (int i = 0; i <= kSteps; i++) {
    float rv  = (float)(i * 1000);
    if (rv > maxRpm) break;
    float ang = rpmToRad(rv, maxRpm);
    bool  isR = (rv >= redRpm);

    spr.drawLine(
      SCX + (int)(cosf(ang) * (R - 14)), SCY + (int)(sinf(ang) * (R - 14)),
      SCX + (int)(cosf(ang) *  R),       SCY + (int)(sinf(ang) *  R),
      isR ? C_TICK_RED : C_TICK);

    // Label — Orbitron 8pt, centred on the tick ray at R-26
    char lbuf[3];
    snprintf(lbuf, sizeof(lbuf), "%d", i);
    spr.setFreeFont(&Saira_Bold6pt7b);
    spr.setTextColor(isR ? C_TICK_RED : C_LABEL);
    int tw = spr.textWidth(lbuf);
    int th = spr.fontHeight();
    // Centre of label sits at R-26 along the tick ray
    float lr = R - 24;
    int lx = SCX + (int)(cosf(ang) * lr) - tw / 2;
    int ly = SCY + (int)(sinf(ang) * lr) + th / 2;
    spr.setCursor(lx, ly);
    spr.print(lbuf);
  }

  memcpy(bgBuf, sprBuf, BUF_PIXELS * sizeof(uint16_t));
  lastMaxRpm = maxRpm;
}

// ── draw trapezoid needle ─────────────────────────────────────────────────────
void drawNeedle(TFT_eSprite &s, float na, uint16_t col) {
  float pa = na + (float)M_PI / 2.0f;

  int tx1 = SCX + (int)(cosf(na) * NEEDLE_LEN + cosf(pa) * NEEDLE_TIP_W);
  int ty1 = SCY + (int)(sinf(na) * NEEDLE_LEN + sinf(pa) * NEEDLE_TIP_W);
  int tx2 = SCX + (int)(cosf(na) * NEEDLE_LEN - cosf(pa) * NEEDLE_TIP_W);
  int ty2 = SCY + (int)(sinf(na) * NEEDLE_LEN - sinf(pa) * NEEDLE_TIP_W);

  int bx1 = SCX + (int)(cosf(pa) * NEEDLE_BASE_W);
  int by1 = SCY + (int)(sinf(pa) * NEEDLE_BASE_W);
  int bx2 = SCX - (int)(cosf(pa) * NEEDLE_BASE_W);
  int by2 = SCY - (int)(sinf(pa) * NEEDLE_BASE_W);

  int lx1 = SCX + (int)(-cosf(na) * NEEDLE_TAIL + cosf(pa) * (NEEDLE_BASE_W - 1));
  int ly1 = SCY + (int)(-sinf(na) * NEEDLE_TAIL + sinf(pa) * (NEEDLE_BASE_W - 1));
  int lx2 = SCX + (int)(-cosf(na) * NEEDLE_TAIL - cosf(pa) * (NEEDLE_BASE_W - 1));
  int ly2 = SCY + (int)(-sinf(na) * NEEDLE_TAIL - sinf(pa) * (NEEDLE_BASE_W - 1));

  // Outline
  s.fillTriangle(tx1, ty1, bx1, by1, bx2, by2, C_NEEDLE_OUT);
  s.fillTriangle(tx1, ty1, tx2, ty2, bx2, by2, C_NEEDLE_OUT);
  s.fillTriangle(lx1, ly1, lx2, ly2, bx1, by1, C_NEEDLE_OUT);
  s.fillTriangle(lx2, ly2, bx1, by1, bx2, by2, C_NEEDLE_OUT);

  // Inner needle (1px inset)
  int itx1 = SCX + (int)(cosf(na) * (NEEDLE_LEN - 1) + cosf(pa) * (NEEDLE_TIP_W - 1));
  int ity1 = SCY + (int)(sinf(na) * (NEEDLE_LEN - 1) + sinf(pa) * (NEEDLE_TIP_W - 1));
  int itx2 = SCX + (int)(cosf(na) * (NEEDLE_LEN - 1) - cosf(pa) * (NEEDLE_TIP_W - 1));
  int ity2 = SCY + (int)(sinf(na) * (NEEDLE_LEN - 1) - sinf(pa) * (NEEDLE_TIP_W - 1));
  int ibx1 = SCX + (int)(cosf(pa) * (NEEDLE_BASE_W - 1));
  int iby1 = SCY + (int)(sinf(pa) * (NEEDLE_BASE_W - 1));
  int ibx2 = SCX - (int)(cosf(pa) * (NEEDLE_BASE_W - 1));
  int iby2 = SCY - (int)(sinf(pa) * (NEEDLE_BASE_W - 1));

  s.fillTriangle(itx1, ity1, ibx1, iby1, ibx2, iby2, col);
  s.fillTriangle(itx1, ity1, itx2, ity2, ibx2, iby2, col);
  s.fillTriangle(lx1, ly1, lx2, ly2, ibx1, iby1, C_NEEDLE_OUT);
  s.fillTriangle(lx2, ly2, ibx1, iby1, ibx2, iby2, C_NEEDLE_OUT);
}

// ── render one frame ──────────────────────────────────────────────────────────
void pushFrame(float rpm, int maxRpm, int speed, const String &rawGear) {
  memcpy(sprBuf, bgBuf, BUF_PIXELS * sizeof(uint16_t));

  // Needle
  float na     = rpmToRad(rpm, maxRpm);
  int   redRpm = (int)(maxRpm * 0.875f);
  uint16_t ncol = ((int)rpm >= redRpm) ? C_NEEDLE_R : C_NEEDLE;
  drawNeedle(spr, na, ncol);

  spr.fillCircle(SCX, SCY, 7, C_HUB);
  spr.fillCircle(SCX, SCY, 3, C_HUB2);

  // Centre hole
  spr.fillCircle(SCX, SCY, HOLE_R, C_HOLE);
  spr.drawCircle(SCX, SCY, HOLE_R, C_DIVIDER);
  spr.drawFastHLine(SCX - HOLE_R + 2, SCY, (HOLE_R - 2) * 2, C_DIVIDER);

  // Speed — centred via textWidth()
  {
    char sbuf[4];
    snprintf(sbuf, sizeof(sbuf), "%d", speed);

    spr.setFreeFont(&Saira_Bold12pt7b);
    spr.setTextColor(C_SPD, C_HOLE);

    int tw = spr.textWidth(sbuf);
    int th = spr.fontHeight();

    int sx = SCX - tw / 2;
    int sy = SCY - HOLE_R / 2 - th / 2 + th;
    sy -= 10;
    spr.setCursor(sx, sy);
    spr.print(sbuf);
  }

  // Gear — formatted + centred
  {
    String gearStr = formatGear(rawGear);

    spr.setFreeFont(&Saira_Bold9pt7b);
    spr.setTextColor(C_GEAR, C_HOLE);

    int tw = spr.textWidth(gearStr.c_str());
    int th = spr.fontHeight();

    int gx = SCX - tw / 2;
    int gy = SCY + th / 2 + 2;
    gy += 5;
    spr.setCursor(gx, gy);
    spr.print(gearStr);
  }

  spr.pushSprite(SPR_OX, SPR_OY);
}

// ── setup ─────────────────────────────────────────────────────────────────────
void setup() {
  Serial.begin(115200);
  tft.init();
  tft.setRotation(1);
  tft.fillScreen(C_BG);

  spr.createSprite(SPR_W, SPR_H);
  spr.setColorDepth(16);
  sprBuf = (uint16_t*)spr.getPointer();

  bgBuf = (uint16_t*)malloc(BUF_PIXELS * sizeof(uint16_t));

  buildBgBuf(8000);
  pushFrame(0, 8000, 0, "N");
}

// ── loop ──────────────────────────────────────────────────────────────────────
void loop() {
  bool needRedraw = false;

  float diff = (float)lastRpm - smoothRpm;
  if (fabsf(diff) > 0.5f) {
    smoothRpm += diff * 0.25f;
    needRedraw = true;
  }

  if (Serial.available()) {
    String data = Serial.readStringUntil('\n');
    data.trim();
    if (data != "CONNECT" && data != "DISCONNECT") {
      int sep1 = data.indexOf(';');
      int sep2 = data.indexOf(';', sep1 + 1);
      int sep3 = data.indexOf(';', sep2 + 1);
      if (sep1 >= 0 && sep2 >= 0 && sep3 >= 0) {
        int    rpm    = data.substring(0, sep1).toInt();
        int    speed  = data.substring(sep1 + 1, sep2).toInt();
        String gear   = data.substring(sep2 + 1, sep3);
        int    maxRpm = data.substring(sep3 + 1).toInt();
        gear.trim();
        if (maxRpm <= 0) maxRpm = 8000;

        if (maxRpm != lastMaxRpm)    { buildBgBuf(maxRpm); needRedraw = true; }
        if (abs(rpm - lastRpm) > 20) { lastRpm   = rpm;   needRedraw = true; }
        if (speed != lastSpeed)      { lastSpeed = speed; needRedraw = true; }
        if (gear  != lastGear)       { lastGear  = gear;  needRedraw = true; }
      }
    }
  }

  if (needRedraw) {
    pushFrame(smoothRpm, lastMaxRpm > 0 ? lastMaxRpm : 8000, lastSpeed, lastGear);
  }
}