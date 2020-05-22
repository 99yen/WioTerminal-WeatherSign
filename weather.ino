/*
 * WioTerminal WeatherSign
 * 2020/5/19 @103yen
 */

#include <string.h>
#include <TFT_eSPI.h>
#include <AtWiFi.h>

#define ARDUINOJSON_DECODE_UNICODE 1
#include <ArduinoJson.h>

#include "weather_graphic.h"

/*
 * usart_ll_thread_idにアクセスするため、Arduino/libraries/esp-at-lib-develop/src/esp_ll_arduino.cppの
 * 90行目を以下のように変更する(staticを取る)
 * 
 * esp_sys_thread_t usart_ll_thread_id;
 */
extern esp_sys_thread_t usart_ll_thread_id;

// WiFi設定
#define WIFI_SSID "SSID"
#define WIFI_PASSWORD "PASSWORD"

// Livedoor天気情報API
// 解説 http://weather.livedoor.com/weather_hacks/webservice
// city=以降が地域ID
// ID一覧 http://weather.livedoor.com/forecast/rss/primary_area.xml
#define API_RESOURCE "/forecast/webservice/json/v1?city=130010"
#define API_SERVER "weather.livedoor.com"

// 更新間隔(ms)
#define UPDATE_PERIOD_MS (1UL * 60 * 60 * 1000)
#define UPDATE_RETRY 3

// タイムアウト
#define CLIENT_TIMEOUT 10000
#define AP_TIMEOUT 20000
#define HTTP_HEADER_TIMEOUT 10000

// 描画位置
#define OFFSET_X 25
#define OFFSET_Y 20
#define CIRCLE_RADIUS 3
#define CIRCLE_OFFSET 7

// ランプ描画色
#define TFT_LAMP 0xFBE4

// WiFi通信スレッド制御
#define WIFI_THREAD_RESTORE_PRIOTIRY()    vTaskPrioritySet(usart_ll_thread_id, ESP_SYS_THREAD_PRIO - 1)
#define WIFI_THREAD_LOW_PRIORITY()        vTaskPrioritySet(usart_ll_thread_id, ESP_SYS_THREAD_PRIO - 2)

// 戻り値
#define WEATHER_SUCCESS 0
#define WEATHER_AP_ERROR -1
#define WEATHER_CONNECTION_ERROR -2
#define WEATHER_TIMEOUT -3
#define WEATHER_JSON_ERROR -4
#define WEATHER_PARSE_ERROR -5

#define FORECAST_PARSE_SUCCESS 0
#define FORECAST_PARSE_ERROR -1

enum class Weather {
  NONE,
  HARE,
  KUMORI,
  AME,
  YUKI
};

enum class WeatherChange {
  NONE,
  TOKIDOKI,
  NOCHI
};

struct Forecast {
  Weather first;
  WeatherChange change;
  Weather second;
};

TFT_eSPI tft;
TFT_eSprite sprite1 = TFT_eSprite(&tft);
unsigned long lastupdate;
Forecast forecast;

int parseWeatherStr(const char* telop, Forecast* forecast) {
  forecast->first = Weather::NONE;
  forecast->change = WeatherChange::NONE;
  forecast->second = Weather::NONE;

  // 最初の予報
  char* hare1 = strstr(telop, u8"晴");
  char* kumori1 = strstr(telop, u8"曇");
  char* ame1 = strstr(telop, u8"雨");
  char* yuki1 = strstr(telop, u8"雪");

  // 最初の位置に文字列があるか？
  if(hare1 == telop)         forecast->first = Weather::HARE;
  else if(kumori1 == telop)  forecast->first = Weather::KUMORI;
  else if(ame1 == telop)     forecast->first = Weather::AME;
  else if(yuki1 == telop)    forecast->first = Weather::YUKI;
  else return FORECAST_PARSE_ERROR;
  // 時々or のち
  char* tokidoki = strstr(telop, u8"時");
  char* nochi = strstr(telop, u8"の");
  if(tokidoki != NULL)    forecast->change = WeatherChange::TOKIDOKI;
  else if(nochi != NULL)  forecast->change = WeatherChange::NOCHI;
  else return FORECAST_PARSE_SUCCESS;
  
  // 次の予報 検索位置をずらして再検索
  char* hare2 = strstr(telop + 1, u8"晴");
  char* kumori2 = strstr(telop + 1, u8"曇");
  char* ame2 = strstr(telop + 1, u8"雨");
  char* yuki2 = strstr(telop + 1, u8"雪");
  
  if(hare2 != NULL)         forecast->second = Weather::HARE;
  else if(kumori2 != NULL)  forecast->second = Weather::KUMORI;
  else if(ame2 != NULL)     forecast->second = Weather::AME;
  else if(yuki2 != NULL)    forecast->second = Weather::YUKI;
  else return FORECAST_PARSE_ERROR;
  
  return FORECAST_PARSE_SUCCESS;
}

int getWeather(Forecast* forecast) {
  int returncode = WEATHER_SUCCESS;
  WiFiClient client;
  StaticJsonDocument<64> filter;
  StaticJsonDocument<512> doc;
  DeserializationError error;
  int parseerror;
  unsigned long starttime;

  tft.setCursor(0, 0);
  tft.println("Begin WiFi");
  
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  starttime = millis();
  while ((WiFi.status() != WL_CONNECTED) && (millis() - starttime < AP_TIMEOUT)) {
      delay(100);
  }
  // AP接続タイムアウト
  if(millis() - starttime >= AP_TIMEOUT) {
    returncode = WEATHER_AP_ERROR;
    goto EXIT;
  }

  tft.println("Connect " API_SERVER);

  client.setTimeout(CLIENT_TIMEOUT);
  if (!client.connect(API_SERVER, 80)) {
      tft.println("Connection failed!");
      returncode = WEATHER_CONNECTION_ERROR;
      goto EXIT;
  }

  tft.println("Request " API_RESOURCE);
  // Make a HTTP request:
  client.println("GET " API_RESOURCE " HTTP/1.0");
  client.println("Host: " API_SERVER);
  client.println("User-Agent: WioTerminal/1.0");
  client.println();

  tft.println("Connected");
  // ヘッダを飛ばす
  starttime = millis();;
  while (client.connected() && (millis() - starttime < HTTP_HEADER_TIMEOUT)) {
    delay(1);
    String line = client.readStringUntil('\n');
    if (line == "\r") {
      break;
    }
  }
  // ヘッダタイムアウト
  if(millis() - starttime >= HTTP_HEADER_TIMEOUT) {
      returncode = WEATHER_TIMEOUT;
      goto EXIT;
  }
  tft.println("Header end");

  // JSONのうち残す項目
  filter["forecasts"][0]["dateLabel"] = true;
  filter["forecasts"][0]["telop"] = true;
  filter["forecasts"][0]["date"] = true;
  filter["publicTime"] = true;

  error = deserializeJson(doc, client, DeserializationOption::Filter(filter));
  if (error) {
    tft.print("deserializeJson() failed: ");
    tft.println(error.c_str());
    returncode = WEATHER_JSON_ERROR;
    goto EXIT;
  }
  
  // 予報文字列をパース
  if(parseWeatherStr(doc["forecasts"][1]["telop"], forecast) 
          != FORECAST_PARSE_SUCCESS) {
    returncode = WEATHER_PARSE_ERROR;
    goto EXIT;
  }

EXIT:
  client.stop();
  WiFi.disconnect();
  return returncode;
}

void drawImage(const uint8_t image[GRAPHIC_HEIGHT][GRAPHIC_WIDTH]){
  // 描画中だけWiFi通信スレッドの優先順位を変更する
  WIFI_THREAD_LOW_PRIORITY();
  
  sprite1.createSprite(320, 240);
  int x = 0;
  for(int y=0; y < GRAPHIC_HEIGHT; y++){
    x = 0;
    for(int x_byte=0; x_byte < GRAPHIC_WIDTH; x_byte++){
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
  
  WIFI_THREAD_RESTORE_PRIOTIRY();
}


void drawWeather(Weather weather) {
  switch(weather) {
    case Weather::HARE:
      drawImage(WEATHER_HARE);
      break;
    case Weather::KUMORI:
      drawImage(WEATHER_KUMORI);
      break;
    case Weather::AME:
      drawImage(WEATHER_AME);
      break;
    case Weather::YUKI:
      drawImage(WEATHER_YUKI);
      break;
  }
}


void drawWeather(WeatherChange change) {
  switch(change) {
     case WeatherChange::TOKIDOKI:
      drawImage(WEATHER_TOKIDOKI);
      break;
     case WeatherChange::NOCHI:
      drawImage(WEATHER_NOCHI);
      break;
  }
}


void updateWeather(bool forceupdate=false) {
  if ((millis() - lastupdate) > UPDATE_PERIOD_MS || forceupdate) {
    drawImage(WEATHER_UPDATING);
    
    for(int i=0; i < UPDATE_RETRY; i++) {
      int error = getWeather(&forecast);
      if(error == WEATHER_SUCCESS) {
        lastupdate = millis();
        break;
      }
      delay(3000);
    }
  }
}


void setup() {
  tft.begin();
  tft.setRotation(3);
  
  sprite1.setColorDepth(1);
  tft.setBitmapColor(TFT_LAMP, TFT_BLACK);
  tft.fillScreen(TFT_BLACK);

  updateWeather(true);
}


void loop() {
  updateWeather();

  if(forecast.first != Weather::NONE) {
    drawImage(WEATHER_ASUNOTENKI);
    delay(1500);
  
    drawWeather(forecast.first);
    delay(1500);
  
    if(forecast.change != WeatherChange::NONE) {
      drawWeather(forecast.change);
      delay(1000);
    }
    
    if(forecast.second != Weather::NONE) {
      drawWeather(forecast.second);
      delay(1500);
    }
  }
  else {
    drawImage(WEATHER_ERROR);
    delay(UPDATE_PERIOD_MS);
  }
}
