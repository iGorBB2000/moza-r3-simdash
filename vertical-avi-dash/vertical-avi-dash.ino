#include <TFT_eSPI.h>
#include <math.h>

TFT_eSPI tft = TFT_eSPI();
TFT_eSprite spr = TFT_eSprite(&tft);

#define W        160
#define H        128
#define TW        44    // tape width px
#define CX       (W/2)
#define CY       (H/2)

// Easing: how fast animated value chases target (0.0–1.0)
// At 60fps, 0.25 per frame feels sharp but smooth
#define EASE     0.25f

// ── State ─────────────────────────────────────────────────────────
int   targetSpeed = 0, targetRpm = 0, targetMaxRpm = 8000;
float animSpeed   = 0, animRpm   = 0;
String currentGear = "N";
unsigned long lastFrame = 0;

// ── Color helpers ─────────────────────────────────────────────────
uint16_t rpmColor(float rpm, int maxRpm) {
  float f = rpm / maxRpm;
  if (f >= 0.87f) return tft.color565(255, 34,   0);
  if (f >= 0.70f) return tft.color565(255, 170,  0);
  return                  tft.color565(0,   204, 102);
}

uint16_t gearColor(String g) {
  if (g == "N") return 0x07FF;
  if (g == "R") return TFT_RED;
  return TFT_WHITE;
}

// ── Draw one tape ─────────────────────────────────────────────────
// x, w       : tape bounding box
// val        : current (animated) value
// pxPerUnit  : screen pixels per 1 unit
// minorEvery : draw minor tick every N units
// majorEvery : draw major tick + label every N units
// accentCol  : pointer + hairline color
// side       : 0 = left tape (ticks on right), 1 = right tape (ticks on left)
// labelDiv   : divide val by this before formatting label (1 = raw, 1000 = "k" units)
void drawTape(TFT_eSprite &s,
              int x, int w, float val,
              float pxPerUnit, int minorEvery, int majorEvery,
              uint16_t accentCol, int side, int labelDiv) {

  s.fillRect(x, 0, w, H, 0x0841); // dark background

  // scan from top of tape downward
  float topVal = val + (float)CY / pxPerUnit;
  int firstTick = (int)ceilf(topVal / minorEvery) * minorEvery;

  for (int v = firstTick; ; v -= minorEvery) {
    float y = CY - (v - val) * pxPerUnit;
    if (y > H + 4) break;
    if (y < -4)    continue;

    bool isMajor = (v % majorEvery == 0);
    int  tickLen = isMajor ? 10 : 5;

    uint16_t tickCol = isMajor ? 0x8410 : 0x2965;
    if (side == 0) // left tape — ticks on right edge
      s.drawFastHLine(x + w - 1 - tickLen, (int)y, tickLen, tickCol);
    else           // right tape — ticks on left edge
      s.drawFastHLine(x, (int)y, tickLen, tickCol);

    if (isMajor) {
      char buf[6];
      int  labelVal = v / labelDiv;
      snprintf(buf, sizeof(buf), "%d", labelVal);
      s.setTextSize(1);
      s.setTextColor(0xC618, 0x0841); // light gray on dark
      s.setTextDatum(side == 0 ? ML_DATUM : MR_DATUM);
      int lx = (side == 0) ? x + 3 : x + w - 3;
      s.drawString(buf, lx, (int)y);
    }
  }

  s.drawFastHLine(x, CY, w, accentCol);

  // Pointer triangle — drawn ON TOP of ticks, no background rect
  if (side == 0) {
    // left tape: triangle points right, sits against right edge
    for (int i = 0; i < 6; i++)
      s.drawFastVLine(x + w - 1 - i, CY - (5 - i), (5 - i) * 2 + 1, accentCol);
  } else {
    // right tape: triangle points left, sits against left edge
    for (int i = 0; i < 6; i++)
      s.drawFastVLine(x + i, CY - (5 - i), (5 - i) * 2 + 1, accentCol);
  }

  // Edge divider
  uint16_t divCol = 0x2965;
  if (side == 0) s.drawFastVLine(x + w, 0, H, divCol);
  else           s.drawFastVLine(x,     0, H, divCol);

  // Current value label above hairline, centered in tape
  char fullBuf[8];
  snprintf(fullBuf, sizeof(fullBuf), "%d", (int)roundf(val)); // always full number
  s.setTextSize(1);
  s.setTextColor(accentCol, TFT_BLACK);
  s.setTextDatum(BC_DATUM);
  s.drawString(fullBuf, x + w / 2, CY - 13);
}

// ── Center panel ──────────────────────────────────────────────────
void drawCenter(TFT_eSprite &s, String gear) {
  int cw = W - TW * 2;
  s.fillRect(TW, 0, cw, H, TFT_BLACK);

  // Big gear character
  s.setTextDatum(MC_DATUM);
  s.setTextColor(gearColor(gear), TFT_BLACK);
  s.setTextSize(6);
  char gbuf[3];
  gear.toCharArray(gbuf, sizeof(gbuf));
  s.drawString(gbuf, CX, CY - 4);

  // "GEAR" label
  s.setTextDatum(BC_DATUM);
  s.setTextColor(0x2965, TFT_BLACK);
  s.setTextSize(1);
  s.drawString("GEAR", CX, H - 4);
}

// ── Full frame render ─────────────────────────────────────────────
void renderFrame() {
  // Speed tape: 3px per km/h, minor every 5, major every 20, raw labels
  float pxPerKmh = 3.0f;
  uint16_t spdCol = tft.color565(51, 153, 255);
  drawTape(spr, 0, TW, animSpeed, pxPerKmh, 5, 20, spdCol, 0, 1);

  // RPM tape: spread maxRpm over 75% of height, minor every 500, major every 1000
  float pxPerRpm = (H * 0.75f) / targetMaxRpm;
  uint16_t rCol  = rpmColor(animRpm, targetMaxRpm);
  drawTape(spr, W - TW, TW, animRpm, pxPerRpm, 500, 1000, rCol, 1, 1000);

  drawCenter(spr, currentGear);
  spr.pushSprite(0, 0);
}

// ── Setup ─────────────────────────────────────────────────────────
void setup() {
  Serial.begin(115200);
  tft.init();
  tft.setRotation(1);
  tft.fillScreen(TFT_BLACK);
  spr.createSprite(W, H);
  renderFrame();
}

// ── Loop ──────────────────────────────────────────────────────────
void loop() {
  // Parse incoming serial
  if (Serial.available()) {
    String data = Serial.readStringUntil('\n');
    if (data != "CONNECT" && data != "DISCONNECT") {
      int sep1 = data.indexOf(';');
      int sep2 = data.indexOf(';', sep1 + 1);
      int sep3 = data.indexOf(';', sep2 + 1);
      targetRpm    = data.substring(0, sep1).toInt();
      targetSpeed  = data.substring(sep1 + 1, sep2).toInt();
      currentGear  = data.substring(sep2 + 1, sep3);
      targetMaxRpm = data.substring(sep3 + 1).toInt();
      currentGear.trim();
      if (targetMaxRpm <= 0) targetMaxRpm = 8000;
    }
  }

  // Run animation at ~60fps regardless of serial rate
  unsigned long now = millis();
  if (now - lastFrame < 16) return; // cap at ~60fps
  lastFrame = now;

  // Ease animated values toward targets
  animSpeed += (targetSpeed - animSpeed) * EASE;
  animRpm   += (targetRpm   - animRpm)   * EASE;

  // Snap to target when close enough to avoid infinite crawl
  if (fabsf(animSpeed - targetSpeed) < 0.1f) animSpeed = targetSpeed;
  if (fabsf(animRpm   - targetRpm)   < 1.0f) animRpm   = targetRpm;

  renderFrame();
}