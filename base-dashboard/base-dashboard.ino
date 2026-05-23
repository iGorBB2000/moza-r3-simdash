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

int lastSpeed = -1, lastRpm = -1;
String lastGear = "";

void drawRpmBar(int rpm, int maxRpm) {
  if (maxRpm <= 0) maxRpm = 8000; // fallback if SimHub sends 0
  int redRpm = maxRpm * 0.87;     // redline at 87% of max
  int ylwRpm = maxRpm * 0.70;     // yellow at 70% of max

  int active = map(constrain(rpm, 0, maxRpm), 0, maxRpm, 0, BAR_SEGS);
  int segW = (BAR_W - (BAR_SEGS - 1) * 2) / BAR_SEGS;

  for (int i = 0; i < BAR_SEGS; i++) {
    int x = BAR_X + i * (segW + 2);
    uint16_t col;
    if (i >= active) {
      col = 0x2104;
    } else {
      float ratio = (float)i / BAR_SEGS;
      if (ratio >= (float)redRpm / maxRpm)      col = TFT_RED;
      else if (ratio >= (float)ylwRpm / maxRpm) col = TFT_YELLOW;
      else                                        col = TFT_GREEN;
    }
    tft.fillRoundRect(x, BAR_Y, segW, BAR_H, 1, col);
  }
}

void setup() {
  Serial.begin(115200);
  tft.init();
  tft.setRotation(1);
  tft.fillScreen(TFT_BLACK);

  // Static labels
  tft.setTextColor(0x4208, TFT_BLACK); // dark grey
  tft.setTextSize(1);
  tft.setCursor(4, 4);
  tft.print("GEAR");
  tft.setCursor(110, 90);
  tft.print("KM/H");
  tft.setCursor(4, 102);
  tft.print("RPM");

  // Divider line
  tft.drawFastVLine(78, 0, 100, 0x2104);
}

void loop() {
  if (!Serial.available()) return;

  String data = Serial.readStringUntil('\n');
  if (data == "CONNECT" || data == "DISCONNECT") return;

  int sep1 = data.indexOf(';');
  int sep2 = data.indexOf(';', sep1 + 1);
  int sep3 = data.indexOf(';', sep2 + 1);

  int rpm  = data.substring(0, sep1).toInt();
  int speed    = data.substring(sep1 + 1, sep2).toInt();
  String gear = data.substring(sep2 + 1, sep3);
  int maxRpm = data.substring(sep3 + 1).toInt();
  gear.trim();

  // Gear — large, left
  if (gear != lastGear) {
    uint16_t gearCol = TFT_WHITE;
    if (gear == "N") gearCol = 0x055F; // blue
    if (gear == "R") gearCol = TFT_RED;

    tft.setTextColor(gearCol, TFT_BLACK);
    tft.setTextSize(7);
    tft.setCursor(8, 18);
    tft.print(gear == lastGear ? gear : "  "); // clear old
    tft.setCursor(8, 18);
    tft.print(gear);
    lastGear = gear;
  }

  // Speed — right side
  if (speed != lastSpeed) {
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.setTextSize(5);
    tft.setCursor(85, 30);
    tft.printf("%-3d", speed);
    lastSpeed = speed;
  }

  if (abs(rpm - lastRpm) > 50) {
    drawRpmBar(rpm, maxRpm);
    lastRpm = rpm;
  }
}