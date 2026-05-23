#include <TFT_eSPI.h>
#include "Fonts/ChakraPetch_Bold8pt7b.h"
#include "Fonts/ChakraPetch_Bold21pt7b.h"
#include "Fonts/ChakraPetch_Bold28pt7b.h"

TFT_eSPI tft = TFT_eSPI();
TFT_eSprite sprSpeed = TFT_eSprite(&tft);
TFT_eSprite sprGear  = TFT_eSprite(&tft);

#define BAR_SEGS      20
#define BAR_X         2
#define BAR_Y         112
#define BAR_W         156
#define BAR_H         10
#define SPEED_RIGHT_X 158
#define SPEED_Y       18

#define GEAR_SPR_X    0
#define GEAR_SPR_Y    20
#define GEAR_SPR_W    62
#define GEAR_SPR_H    70

#define SPEED_SPR_X   65
#define SPEED_SPR_Y   14
#define SPEED_SPR_W   93
#define SPEED_SPR_H   70 

int lastSpeed = -1, lastRpm = -1, lastActive = -1;
String lastGear = "";

void drawRpmBar(int rpm, int maxRpm) {
  if (maxRpm <= 0) maxRpm = 8000;
  int redRpm = maxRpm * 0.87;
  int ylwRpm = maxRpm * 0.70;
  int active = map(constrain(rpm, 0, maxRpm), 0, maxRpm, 0, BAR_SEGS);
  if (active == lastActive) return;
  int segW = (BAR_W - (BAR_SEGS - 1) * 2) / BAR_SEGS;
  for (int i = 0; i < BAR_SEGS; i++) {
    bool wasActive = (i < lastActive);
    bool isActive  = (i < active);
    if (wasActive == isActive) continue;
    int x = BAR_X + i * (segW + 2);
    uint16_t col;
    if (!isActive) {
      col = 0x2104;
    } else {
      float ratio = (float)i / BAR_SEGS;
      if      (ratio >= (float)redRpm / maxRpm) col = TFT_RED;
      else if (ratio >= (float)ylwRpm / maxRpm) col = TFT_YELLOW;
      else                                       col = TFT_GREEN;
    }
    tft.fillRoundRect(x, BAR_Y, segW, BAR_H, 1, col);
  }
  lastActive = active;
}

void drawSpeed(int speed) {
  sprSpeed.fillSprite(TFT_BLACK);
  sprSpeed.setFreeFont(&ChakraPetch_Bold21pt7b);
  sprSpeed.setTextDatum(TR_DATUM);
  sprSpeed.setTextColor(TFT_WHITE, TFT_BLACK);
  char buf[4];
  snprintf(buf, sizeof(buf), "%3d", speed);
  sprSpeed.drawString(buf, SPEED_SPR_W - 1, 4);
  sprSpeed.pushSprite(SPEED_SPR_X, SPEED_SPR_Y);
}

void drawGear(String gear) {
  sprGear.fillSprite(TFT_BLACK);
  uint16_t gearCol = TFT_WHITE;
  if (gear == "N") gearCol = 0x055F;
  if (gear == "R") gearCol = TFT_RED;
  sprGear.setFreeFont(&ChakraPetch_Bold28pt7b);
  sprGear.setTextDatum(TL_DATUM);
  sprGear.setTextColor(gearCol, TFT_BLACK);
  sprGear.drawString(gear, 6, 4);
  sprGear.pushSprite(GEAR_SPR_X, GEAR_SPR_Y);
}

void setup() {
  Serial.begin(115200);
  tft.init();
  tft.setRotation(1);
  tft.fillScreen(TFT_BLACK);

  sprGear.createSprite(GEAR_SPR_W, GEAR_SPR_H);
  sprSpeed.createSprite(SPEED_SPR_W, SPEED_SPR_H);

  tft.setFreeFont(&ChakraPetch_Bold8pt7b);
  tft.setTextColor(0x4208, TFT_BLACK);
  tft.setTextDatum(TL_DATUM);
  tft.drawString("GEAR", 4, 2);
  tft.drawString("KM/H", 95, 88);
  tft.drawString("RPM", 4, 95);
  tft.drawFastVLine(63, 0, 108, 0x2104);
}

void loop() {
  if (!Serial.available()) return;
  String data = Serial.readStringUntil('\n');
  if (data == "CONNECT" || data == "DISCONNECT") return;

  int sep1 = data.indexOf(';');
  int sep2 = data.indexOf(';', sep1 + 1);
  int sep3 = data.indexOf(';', sep2 + 1);
  int rpm     = data.substring(0, sep1).toInt();
  int speed   = data.substring(sep1 + 1, sep2).toInt();
  String gear = data.substring(sep2 + 1, sep3);
  int maxRpm  = data.substring(sep3 + 1).toInt();
  gear.trim();

  if (gear != lastGear) {
    drawGear(gear);
    lastGear = gear;
  }
  if (speed != lastSpeed) {
    drawSpeed(speed);
    lastSpeed = speed;
  }
  if (abs(rpm - lastRpm) > 50) {
    drawRpmBar(rpm, maxRpm);
    lastRpm = rpm;
  }
}