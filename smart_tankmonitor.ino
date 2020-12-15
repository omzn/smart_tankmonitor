#include <M5Stack.h>

#include <ArduinoJson.h>
#include <ArduinoOTA.h>
#include <Preferences.h>
#include <RTClib.h>
#include <WebServer.h>
#include <WiFi.h>
#include <WiFiClient.h>
#include <WiFiManager.h> // https://github.com/tzapu/WiFiManager
#include <WiFiUdp.h>

#include "ntp.h"

// Include the header files that contain the icons
#include "DSEG14Classic-BoldItalic16pt7b.h"
#include "DSEG7Classic-BoldItalic9pt7b.h"
#include "board.h"

#define DEG2RAD 0.0174532925

#define FONT_7SEG16PT M5.Lcd.setFont(&DSEG14Classic_BoldItalic16pt7b)
#define FONT_7SEG9PT M5.Lcd.setFont(&DSEG7Classic_BoldItalic9pt7b)

#define TFT_DARKRED M5.Lcd.color565(140, 0, 0)
#define TFT_DARKYELLOW M5.Lcd.color565(180, 140, 0)
#define TFT_DARKDARKYELLOW M5.Lcd.color565(60, 60, 0)

WebServer webServer(80);
NTP ntp("ntp.nict.jp");
RTC_Millis rtc;
Preferences prefs;
WiFiManager wifimanager;
unsigned long etime;
int is_error = 0;

String aquaCoolURL =  "aquacool.water.dip.jp";
String aquaFeedURL =  "aquafeed.water.dip.jp";
String aquaLightURL = "aqualight.water.dip.jp";
//String aquaCoolURL = "192.168.68.103"; // "aquacool.water.dip.jp";
//String aquaFeedURL = "192.168.68.103"; // "aquafeed.water.dip.jp";
//String aquaLightURL = "192.168.68.73"; // "aqualight.water.dip.jp";

int drawLevelUnit(int posx, int posy, int width, int height, int color,
                  int grad) {
  int g = 0;
  for (int i = 0; i < height; i++) {
    g += (i % 3 == 0 && i > 0 ? grad : 0);
    M5.Lcd.drawLine(posx - g, posy + i, posx + width - g, posy + i, color);
  }
  return g;
}

void drawHorizontalLevelIndicator(int current_level, int posx, int posy,
                                  int width, int height, int levels,
                                  int levels_green, int levels_yellow,
                                  int levels_red) {
  int unitWidth, unitHeight, unitColor;
  unitWidth = width / levels;
  unitHeight = height;
  for (int i = 0; i < levels; i++) {
    if (levels_green <= levels_red) {
      unitColor = i < levels_green
                      ? TFT_DARKCYAN
                      : (i < levels_yellow ? TFT_DARKYELLOW : TFT_DARKRED);
    } else {
      unitColor = i < levels_red
                      ? TFT_DARKRED
                      : (i < levels_yellow ? TFT_DARKYELLOW : TFT_DARKCYAN);
    }
    if (i >= current_level) {
      unitColor = BGCOLOR;
    }
    drawLevelUnit(posx + i * unitWidth, posy, unitWidth - 2, unitHeight,
                  unitColor, 1);
  }
}

void drawVerticalLevelIndicator(int current_level, int posx, int posy,
                                int width, int height, int levels,
                                int levels_green, int levels_yellow,
                                int levels_red) {
  int unitWidth, unitHeight, unitColor, diff = 0;
  unitWidth = width;
  unitHeight = height / levels;
  for (int i = 0; i < levels; i++) {
    if (levels_green <= levels_red) {
      unitColor = i < levels_green
                      ? TFT_DARKGREEN
                      : (i < levels_yellow ? TFT_DARKYELLOW : TFT_DARKRED);
    } else {
      unitColor = i < levels_red
                      ? TFT_DARKRED
                      : (i < levels_yellow ? TFT_DARKYELLOW : TFT_DARKGREEN);
    }
    if (i >= current_level) {
      unitColor = BGCOLOR;
    }
    diff += drawLevelUnit(posx + diff, posy + (levels - i - 1) * unitHeight,
                          unitWidth, unitHeight - 2, unitColor, 1);
  }
}

void waterTempIndicator(float current, float min, float max, int posx,
                        int posy) {
  int current_level = 1;
  if (current <= min) {
    current_level = 1;
  } else if (current >= max) {
    current_level = 10;
  } else {
    current_level = ((current - min) / (max - min)) * 10 + 1;
  }
  drawHorizontalLevelIndicator(current_level, posx, posy, 100, 10, 10, 5, 9,
                               10);
}

void waterLevelIndicator(float current, float min, float max, int posx,
                         int posy) {
  int current_level = 1;
  if (current <= min) {
    current_level = 5;
  } else if (current >= max) {
    current_level = 1;
  } else {
    current_level = (1 - ((current - min) / (max - min))) * 5 + 1;
  }
  drawVerticalLevelIndicator(current_level, posx, posy, 30, 45, 5, 5, 4, 1);
}

void lightLevelIndicator(float current, float min, float max, int posx,
                         int posy) {
  int current_level = 1;
  if (current > max) {
    current_level = 8;
  } else if (current < min) {
    current_level = 1;
  } else {
    current_level = (((current - min) / (max - min))) * 8 + 1;
  }
  drawVerticalLevelIndicator(current_level, posx, posy, 20, 48, 8, 8, 7, 1);
}

void allLightsIndicator(int status[], int posx, int posy) {
  for (int i = 0; i < 8; i++) {
    lightLevelIndicator(status[i], 0, 100, posx + i * 22, posy);
  }
}

int fillSegment(int x, int y, int start_angle, int sub_angle, int r,
                unsigned int colour) {
  // Calculate first pair of coordinates for segment start
  float sx = cos((start_angle - 90) * DEG2RAD);
  float sy = sin((start_angle - 90) * DEG2RAD);
  uint16_t x1 = sx * r + x;
  uint16_t y1 = sy * r + y;

  // Draw colour blocks every inc degrees
  for (int i = start_angle; i < start_angle + sub_angle; i++) {

    // Calculate pair of coordinates for segment end
    int x2 = cos((i + 1 - 90) * DEG2RAD) * r + x;
    int y2 = sin((i + 1 - 90) * DEG2RAD) * r + y;

    M5.Lcd.fillTriangle(x1, y1, x2, y2, x, y, colour);

    // Copy segment end to sgement start for next segment
    x1 = x2;
    y1 = y2;
  }
}

void drawPiChart(int current, int max, int posx, int posy,
                 bool showCurrent = false) {
  float angle;
  angle = ((current * 1.0) / (max * 1.0)) * 360;
  fillSegment(posx, posy, 0, angle, 23, FGCOLOR);
  fillSegment(posx, posy, 0, angle, 22, TFT_DARKYELLOW);
  fillSegment(posx, posy, angle, 360 - angle, 22, TFT_DARKDARKYELLOW);
  fillSegment(posx, posy, 0, 360, 12, BGCOLOR);
  if (showCurrent) {
    FONT_7SEG9PT;
    M5.Lcd.drawNumber(current, posx - 6, posy - 8);
  }
}

void drawWaterTemp(float watertemp, float watertemp_high) {
  FONT_7SEG16PT;
  M5.Lcd.setTextColor(BGCOLOR, BGCOLOR);
//  M5.Lcd.drawFloat(88.8, 1, 130, 24);
  M5.Lcd.setCursor(130,52);
  M5.Lcd.printf("~~.~");

  M5.Lcd.setTextColor(FGCOLOR, BGCOLOR);
  M5.Lcd.setCursor(130,52);
  M5.Lcd.printf("%3.1f",watertemp);
//  M5.Lcd.drawFloat(watertemp, 1, 130, 24);

  waterTempIndicator(watertemp, watertemp_high-7, watertemp_high+1 , 120, 8);
}

void drawWaterLevel(float waterlevel_rel, float waterlevel_low) {
  FONT_7SEG16PT;
  M5.Lcd.setTextColor(BGCOLOR, BGCOLOR);
  M5.Lcd.setCursor(150,158);
  M5.Lcd.printf("~~");
//  M5.Lcd.drawNumber(88, 150, 130);
  M5.Lcd.setTextColor(FGCOLOR, BGCOLOR);
  M5.Lcd.setCursor(150,158);
  M5.Lcd.printf("%+d",int(waterlevel_rel));
//  M5.Lcd.drawNumber(waterlevel_rel, 150, 130);

  waterLevelIndicator(waterlevel_low - waterlevel_rel, waterlevel_low-2, waterlevel_low+2, 115, 118);
}

void drawFanStatus(int fanstatus, float watertemp_high) {
  FONT_7SEG9PT;
  M5.Lcd.drawFloat(watertemp_high, 1, 160, 63);
  // 冷却状態
  if (fanstatus) {
    M5.Lcd.pushImage(128, 87, panelWidth, panelHeight, fan_active);
  } else {
    M5.Lcd.pushImage(128, 87, panelWidth, panelHeight, fan_stop);
  }
}

void drawFeedStatus(int feedcount, int feedrest, int rotated) {
  FONT_7SEG9PT;
  drawPiChart(feedcount, 3, 285, 80, true);
  drawPiChart(feedrest, feedrest + rotated, 285, 130, false);
}

int getStatus(String server, WiFiClient *client) {
  if (!client->connect(server.c_str(), 80)) {
    Serial.println(F("Connection failed"));
    return 1;
  }
  Serial.println(F("Connected!"));
  // Send HTTP request
  client->println(F("GET /status HTTP/1.0"));
  client->println("Host: " + server);
  client->println(F("Connection: close"));
  if (client->println() == 0) {
    Serial.println(F("Failed to send request"));
    return 1;
  }
  // Check HTTP status
  char status[32] = {0};
  client->readBytesUntil('\r', status, sizeof(status));
  if (strcmp(status, "HTTP/1.0 200 OK") != 0) {
    Serial.print(F("Unexpected response: "));
    Serial.println(status);
    return 1;
  }
  // Skip HTTP headers
  char endOfHeaders[] = "\r\n\r\n";
  if (!client->find(endOfHeaders)) {
    Serial.println(F("Invalid response"));
    return 1;
  }
  return 0;
}

int getLightStatus(String server) {
  WiFiClient client;
  client.setTimeout(10000);
  if (getStatus(server,&client)) {
    return 1;
  }
  const size_t capacity =
      JSON_OBJECT_SIZE(3) * 8 + JSON_ARRAY_SIZE(1) + JSON_OBJECT_SIZE(2) + 120;
  DynamicJsonBuffer jsonBuffer(capacity);
  JsonObject &root = jsonBuffer.parseObject(client);
  if (!root.success()) {
    Serial.println(F("Parsing failed!"));
    return 1;
  } else {
    int brightness[8];
    for (int i = 0; i < 8; ++i) {
      brightness[i] = root["lights"][i]["brightness"];
      Serial.println(brightness[i]);
    }
    allLightsIndicator(brightness, 115, 176);
    return 0;
  }
}

int getWaterCoolerStatus(String server) {
  WiFiClient client;
  client.setTimeout(10000);
  if (getStatus(server,&client)) {
    return 1;
  }
  const size_t capacity = JSON_OBJECT_SIZE(10) + 120;
  DynamicJsonBuffer jsonBuffer(capacity);
  JsonObject &root = jsonBuffer.parseObject(client);
  if (!root.success()) {
    Serial.println(F("Parsing failed!"));
    return 1;
  } else {
    float water_temp = root["water_temp"];
    float water_temp_rel = root["water_temp_rel"];
    float water_level = root["water_level"];
    float water_level_rel = root["water_level_rel"];
    int fan_status = root["fan_status"];
    drawWaterTemp(water_temp, water_temp + water_temp_rel);
    drawWaterLevel(water_level_rel, water_level + water_level_rel);
    drawFanStatus(fan_status, water_temp + water_temp_rel);
    return 0;
  }
}

int getFeederStatus(String server) {
  WiFiClient client;
  client.setTimeout(10000);
  if (getStatus(server,&client)) {
    return 1;
  }
  const size_t capacity = JSON_OBJECT_SIZE(10) + 120;
  DynamicJsonBuffer jsonBuffer(capacity);
  JsonObject &root = jsonBuffer.parseObject(client);
  if (!root.success()) {
    Serial.println(F("Parsing failed!"));
    return 1;
  } else {
    int num_feed = root["fed"];
    int remaining = root["remaining"];
    int rotated = root["rotated"];
    drawFeedStatus(num_feed, remaining, rotated);
    return 0;
  }
}
bool is_power_control;

void setup() {
  M5.begin();
  Serial.begin(115200);
  Serial.println("Booting");
  rtc.begin(DateTime(2019, 5, 1, 0, 0, 0));
  prefs.begin("aquatan", false);

  M5.Power.begin();
  if(!M5.Power.canControl()) {
    is_power_control = false;
  }
  if (is_power_control) {
    M5.Power.setPowerVin(false);
  //  M5.Power.setLowPowerShutdownTime(ShutdownTime::SHUTDOWNTIME_64S);
  }

  wifimanager.autoConnect("tankmonitor");
  ArduinoOTA.setHostname("tankmonitor");

  ArduinoOTA
      .onStart([]() {
        String type;
        if (ArduinoOTA.getCommand() == U_FLASH)
          type = "sketch";
        else // U_SPIFFS
          type = "filesystem";

        // NOTE: if updating SPIFFS this would be the place to unmount SPIFFS
        // using SPIFFS.end()
        Serial.println("Start updating " + type);
//        pinMode(2, OUTPUT);
      })
      .onEnd([]() {
//        digitalWrite(2, LOW);
        Serial.println("\nEnd");
      })
      .onProgress([](unsigned int progress, unsigned int total) {
        //        digitalWrite(2, (progress / (total / 100)) % 2);
        Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
      })
      .onError([](ota_error_t error) {
        Serial.printf("Error[%u]: ", error);
        if (error == OTA_AUTH_ERROR)
          Serial.println("Auth Failed");
        else if (error == OTA_BEGIN_ERROR)
          Serial.println("Begin Failed");
        else if (error == OTA_CONNECT_ERROR)
          Serial.println("Connect Failed");
        else if (error == OTA_RECEIVE_ERROR)
          Serial.println("Receive Failed");
        else if (error == OTA_END_ERROR)
          Serial.println("End Failed");
      });
  ArduinoOTA.begin();

  M5.Lcd.fillScreen(BGCOLOR);
  M5.Lcd.setSwapBytes(true);
  // Draw the icons
  M5.Lcd.pushImage(0, 0, boardWidth, boardHeight, board);
  FONT_7SEG16PT;
  M5.Lcd.setTextColor(FGCOLOR, BGCOLOR);
  //  M5.Lcd.setCursor(120,15);
  // 水温
  // float watertemp = 99.9;
  // drawWaterTemp(watertemp);
  M5.Lcd.pushImage(102, 10, noresponse01Width, noresponse01Height,
                   noresponse01);
  M5.Lcd.pushImage(102, 65, noresponse01Width, noresponse01Height,
                   noresponse01);
  M5.Lcd.pushImage(102, 119, noresponse01Width, noresponse01Height,
                   noresponse01);

  // 餌
  int feedcount = 3;
  int feedrest = 80;
  //  drawFeedStatus(feedcount, feedrest, 19);
  M5.Lcd.pushImage(231, 65, noresponse03Width, noresponse03Height,
                   noresponse03);

  //照明
  M5.Lcd.pushImage(102, 176, noresponse02Width, noresponse02Height,
                   noresponse02);
  //  int lightstatus[8] = {100, 100, 66, 66, 33, 33, 0, 0};
  //  allLightsIndicator(lightstatus, 115, 176);
  is_error = 1;

  ntp.begin();
  //  webServer.begin();
  uint32_t epoch = ntp.getTime();
  if (epoch > 0) {
    rtc.adjust(DateTime(epoch + SECONDS_UTC_TO_JST));
  }
  etime = millis();
}

void loop() {

  //  webServer.handleClient();
  ArduinoOTA.handle();

  if (millis() > etime + 30000) {
    etime = millis();
    if (is_error > 0) {
      M5.Lcd.pushImage(0, 0, boardWidth, boardHeight, board);
    }
    is_error = 0;
    if (getWaterCoolerStatus(aquaCoolURL)) {
      M5.Lcd.pushImage(105, 10, noresponse01Width, noresponse01Height,
                       noresponse01);
      M5.Lcd.pushImage(105, 65, noresponse01Width, noresponse01Height,
                       noresponse01);
      M5.Lcd.pushImage(105, 119, noresponse01Width, noresponse01Height,
                       noresponse01);
      is_error ++;
    }
    if (getFeederStatus(aquaFeedURL)) {
      M5.Lcd.pushImage(233, 62, noresponse03Width, noresponse03Height,
                       noresponse03);

      is_error ++;
    }
    if (getLightStatus(aquaLightURL)) {
      M5.Lcd.pushImage(105, 176, noresponse02Width, noresponse02Height,
                       noresponse02);
      is_error ++;
    }

  }
  delay(10);
}