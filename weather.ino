/*
 * WioTerminal bulb weather board
 * 2020/5/18 @103yen
 */
 
#include <string.h>
#include <TFT_eSPI.h>
#include <AtWiFi.h>

#define ARDUINOJSON_DECODE_UNICODE 1
#include <ArduinoJson.h>

#include "weather_graphic.h"

// WiFi設定
#define WIFI_SSID "SSID"
#define WIFI_PASSWORD "PASSWORD"

// Livedoor天気情報API
// http://weather.livedoor.com/weather_hacks/webservice
#define API_SERVER "weather.livedoor.com"

// city=以降が都市ID
// http://weather.livedoor.com/forecast/rss/primary_area.xml
#define API_RESOURCE "/forecast/webservice/json/v1?city=130010"

// 描画位置
#define OFFSET_X 25
#define OFFSET_Y 20
#define CIRCLE_RADIUS 3
#define CIRCLE_OFFSET 7

// ランプ描画色
#define TFT_LAMP 0xFBE4

// APIから取得した天気予報の種類
#define FORECAST_NULL 0x0000
#define FORECAST_FIRST_HARE 0x1000
#define FORECAST_FIRST_KUMORI 0x2000
#define FORECAST_FIRST_AME 0x4000
#define FORECAST_FIRST_YUKI 0x8000

#define FORECAST_NOCHI 0x0100
#define FORECAST_TOKIDOKI 0x0200

#define FORECAST_SECOND_HARE 0x0010
#define FORECAST_SECOND_KUMORI 0x0020
#define FORECAST_SECOND_AME 0x0040
#define FORECAST_SECOND_YUKI 0x0080

#define FORECAST_CONNECT_ERROR 0x0001
#define FORECAST_JSON_ERROR 0x0002

#define FORECAST_MASK_FIRST 0xF000
#define FORECAST_MASK_NOCHI_TOKIDOKI 0x0F00
#define FORECAST_MASK_SECOND 0x00F0

TFT_eSPI tft;
TFT_eSprite sprite1 = TFT_eSprite(&tft);
int weather;

int parseWeatherStr(const char* telop) {
  int returncode = FORECAST_NULL;
  
  // 最初の予報
  char* hare1 = strstr(telop, u8"晴");
  char* kumori1 = strstr(telop, u8"曇");
  char* ame1 = strstr(telop, u8"雨");
  char* yuki1 = strstr(telop, u8"雪");

  // 最初の位置に文字列があるか？
  if(hare1 == telop)         returncode = FORECAST_FIRST_HARE;
  else if(kumori1 == telop)  returncode = FORECAST_FIRST_KUMORI;
  else if(ame1 == telop)     returncode = FORECAST_FIRST_AME;
  else if(yuki1 == telop)    returncode = FORECAST_FIRST_YUKI;
  
  // 時々or のち
  char* tokidoki = strstr(telop, u8"時");
  char* nochi = strstr(telop, u8"の");
  if(tokidoki != NULL)    returncode |= FORECAST_TOKIDOKI;
  else if(nochi != NULL)  returncode |= FORECAST_NOCHI;
  
  // 次の予報 検索位置をずらして再検索
  char* hare2 = strstr(telop + 1, u8"晴");
  char* kumori2 = strstr(telop + 1, u8"曇");
  char* ame2 = strstr(telop + 1, u8"雨");
  char* yuki2 = strstr(telop + 1, u8"雪");
  
  if(hare2 != NULL)         returncode |= FORECAST_SECOND_HARE;
  else if(kumori1 != NULL)  returncode |= FORECAST_SECOND_KUMORI;
  else if(ame1 != NULL)     returncode |= FORECAST_SECOND_AME;
  else if(yuki1 != NULL)    returncode |= FORECAST_SECOND_YUKI;

  return returncode;
}

int getWeather() {
  int returncode = FORECAST_NULL;
  
  WiFiClient client;
  StaticJsonDocument<64> filter;
  StaticJsonDocument<512> doc;
  DeserializationError error;
  
  tft.println("Start WiFi Connecting");
  
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  while (WiFi.status() != WL_CONNECTED) {
      delay(100);
  }
  tft.println("WiFi Connected");
  
  if (!client.connect(API_SERVER, 80)) {
      tft.println("Connection failed!");
      returncode = FORECAST_CONNECT_ERROR;
      goto EXIT;
  }

  // Make a HTTP request:
  client.println("GET " API_RESOURCE " HTTP/1.0");
  client.println("Host: " API_SERVER);
  client.println("Connection: close");
  client.println();

  while (client.connected()) {
    String line = client.readStringUntil('\n');
    if (line == "\r") {
      // end of header
      break;
    }
  }

  // JSONのうち残す項目
  filter["forecasts"][0]["dateLabel"] = true;
  filter["forecasts"][0]["telop"] = true;
  filter["forecasts"][0]["date"] = true;
  filter["publicTime"] = true;

  error = deserializeJson(doc, client, DeserializationOption::Filter(filter));
  if (error) {
    tft.print("deserializeJson() failed: ");
    tft.println(error.c_str());
    returncode = FORECAST_JSON_ERROR;
    goto EXIT;
  }
  
  // 予報文字列をパース
  returncode = parseWeatherStr(doc["forecasts"][1]["telop"]);
  
EXIT:
  client.stop();
  WiFi.disconnect();

  tft.println(returncode, HEX);
  
  return returncode;
}

void drawImage(const uint8_t image[][5]){
  int x = 0;

  sprite1.createSprite(320, 240);
  
  for(int y=0; y < 28; y++){
    x = 0;
    for(int x_byte=0; x_byte < 5; x_byte++){
      for(uint8_t x_bit=0x80; x_bit > 0; x_bit >>= 1){
        if((image[y][x_byte] & x_bit) != 0){
          sprite1.fillCircle(OFFSET_X + x * CIRCLE_OFFSET , OFFSET_Y + y * CIRCLE_OFFSET, CIRCLE_RADIUS, TFT_WHITE);
        }
        x++;
      }
    }
  }
  
  sprite1.pushSprite(0, 0);
  sprite1.deleteSprite();
}

void drawWeather(int weather){
  switch(weather){
    case FORECAST_FIRST_HARE:
      drawImage(WEATHER_HARE);
      break;
    case FORECAST_FIRST_KUMORI:
      drawImage(WEATHER_KUMORI);
      break;
    case FORECAST_FIRST_AME:
      drawImage(WEATHER_AME);
      break;
    case FORECAST_FIRST_YUKI:
      drawImage(WEATHER_YUKI);
      break;
    case FORECAST_SECOND_HARE:
      drawImage(WEATHER_HARE);
      break;
    case FORECAST_SECOND_KUMORI:
      drawImage(WEATHER_KUMORI);
      break;
    case FORECAST_SECOND_AME:
      drawImage(WEATHER_AME);
      break;
    case FORECAST_SECOND_YUKI:
      drawImage(WEATHER_YUKI);
      break;
     case FORECAST_TOKIDOKI:
      drawImage(WEATHER_TOKIDOKI);
      break;
     case FORECAST_NOCHI:
      drawImage(WEATHER_NOCHI);
      break;
  }
}

void setup() {
  tft.begin();
  tft.setRotation(3);
  
  sprite1.setColorDepth(1);
  tft.setBitmapColor(TFT_LAMP, TFT_BLACK);
  tft.fillScreen(TFT_BLACK);
  
  weather = getWeather();
}

void loop() {
  drawImage(WEATHER_ASUNOTENKI);
  delay(1500);

  drawWeather(weather & FORECAST_MASK_FIRST);
  delay(1500);

  if(weather & FORECAST_MASK_NOCHI_TOKIDOKI) {
    drawWeather(weather & FORECAST_MASK_NOCHI_TOKIDOKI);
    delay(1000);
  }
  
  if(weather & FORECAST_MASK_SECOND) {
    drawWeather(weather & FORECAST_MASK_SECOND);
    delay(1500);
  }
}
