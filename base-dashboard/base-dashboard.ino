#include <TFT_eSPI.h>

TFT_eSPI tft = TFT_eSPI();

#define MAX_RPM   8000
#define RED_RPM   7000
#define YLW_RPM   5500
#define BAR_SEGS  20
#define BAR_X     2
#define BAR_Y     112
#define BAR_W     156
#define BAR_H     10

#define CHAR_W(size) (6 * (size))
#define SPEED_SIZE   5
#define SPEED_RIGHT_X 158
#define SPEED_Y      30

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
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextSize(SPEED_SIZE);
  char buf[4];
  snprintf(buf, sizeof(buf), "%3d", speed);
  int cursorX = SPEED_RIGHT_X - (3 * CHAR_W(SPEED_SIZE));
  tft.setCursor(cursorX, SPEED_Y);
  tft.print(buf);
}

void drawGear(String gear, uint16_t col) {
  tft.setTextColor(col, TFT_BLACK);
  tft.setTextSize(7);
  tft.setCursor(8, 18);
  char buf[3] = { gear[0], ' ', '\0' };
  tft.print(buf);
}

void setup() {
  Serial.begin(115200);
  tft.init();
  tft.setRotation(1);
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(0x4208, TFT_BLACK);
  tft.setTextSize(1);
  tft.setCursor(4, 4);
  tft.print("GEAR");
  tft.setCursor(95, 90);
  tft.print("KM/H");
  tft.setCursor(4, 102);
  tft.print("RPM");
  tft.drawFastVLine(63, 0, 100, 0x2104);
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
    uint16_t gearCol = TFT_WHITE;
    if (gear == "N") gearCol = 0x055F;
    if (gear == "R") gearCol = TFT_RED;
    drawGear(gear, gearCol);
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