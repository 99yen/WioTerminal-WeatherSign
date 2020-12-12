/*
 * WioTerminal WeatherSign
 * 2020/7/26 @103yen
 * 2020/12/12 ルート証明書を更新
 */

#include <string.h>
#include <TFT_eSPI.h>
#include <WiFiClientSecure.h>

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

// Yahoo!天気・災害
// https://weather.yahoo.co.jp/weather/rss/
#define API_RESOURCE "/rss/days/4410.xml"
#define API_SERVER "rss-weather.yahoo.co.jp"

// XMLに出てくる<description>タグのうち、先頭から何番目のものが目的の天気予報か
#define API_DESCRIPTION_LEVEL 3

// Baltimore CyberTrust Root
const char* cybertrust_root_ca = 
R"(-----BEGIN CERTIFICATE-----
MIIE7jCCA9agAwIBAgIJIrmxYwzstDwuMA0GCSqGSIb3DQEBCwUAMF0xCzAJBgNV
BAYTAkpQMSUwIwYDVQQKExxTRUNPTSBUcnVzdCBTeXN0ZW1zIENPLixMVEQuMScw
JQYDVQQLEx5TZWN1cml0eSBDb21tdW5pY2F0aW9uIFJvb3RDQTIwHhcNMTkwOTI3
MDE1NDIzWhcNMjkwNTI5MDUwMDM5WjBeMQswCQYDVQQGEwJKUDEjMCEGA1UEChMa
Q3liZXJ0cnVzdCBKYXBhbiBDby4sIEx0ZC4xKjAoBgNVBAMTIUN5YmVydHJ1c3Qg
SmFwYW4gU3VyZVNlcnZlciBDQSBHNDCCASIwDQYJKoZIhvcNAQEBBQADggEPADCC
AQoCggEBAMtunFmosT8IxBkVFP+OnkGkcVmbui+hdVBlGZhniniVluAhigm2WUxx
p4X5V3B/QKJLZmeAswmzxGKXaDCzcomYxXTygNXcTLI+IMyRisEO7V1NXFHUjSEl
KaY1LzCA9/emldnmRjX6B9Zt5xXK5q12WOIWkJECEwwKku77tvtKZPRKaCNCGsZ5
Hja7PBs07jLoE0rMuZLQZNQEB0W63attKGCGzEk50lDj+wQ0UlUbQk3zAEsvdE6X
o1qZy9l783Va40vSx3VqhGYb4jWQrg2CrAtJcKQNSJ0m9yxJVVQDwpQQwGxHO5Em
Qv1LGJExASegOXzhzqCr5yiwECfSrOsCAwEAAaOCAa4wggGqMB0GA1UdDgQWBBRi
p9La3oW2kvGFvPbolZ11oPpOHzAfBgNVHSMEGDAWgBQKhal3ZQWYfECB+A+XLDjx
Cuw8zzASBgNVHRMBAf8ECDAGAQH/AgEAMA4GA1UdDwEB/wQEAwIBBjBJBgNVHR8E
QjBAMD6gPKA6hjhodHRwOi8vcmVwb3NpdG9yeS5zZWNvbXRydXN0Lm5ldC9TQy1S
b290Mi9TQ1Jvb3QyQ1JMLmNybDBSBgNVHSAESzBJMEcGCiqDCIybG2SHBQQwOTA3
BggrBgEFBQcCARYraHR0cHM6Ly9yZXBvc2l0b3J5LnNlY29tdHJ1c3QubmV0L1ND
LVJvb3QyLzCBhQYIKwYBBQUHAQEEeTB3MDAGCCsGAQUFBzABhiRodHRwOi8vc2Ny
b290Y2EyLm9jc3Auc2Vjb210cnVzdC5uZXQwQwYIKwYBBQUHMAKGN2h0dHA6Ly9y
ZXBvc2l0b3J5LnNlY29tdHJ1c3QubmV0L1NDLVJvb3QyL1NDUm9vdDJjYS5jZXIw
HQYDVR0lBBYwFAYIKwYBBQUHAwEGCCsGAQUFBwMCMA0GCSqGSIb3DQEBCwUAA4IB
AQDFUXkuQAZKY5Pb/JvgufizJ8uZKpN++fiI8kOTgkrSriNYNjO9iEdTwVV3sW3P
abr3lAdCGrJ+wgArLLUnBZ7VGM//UlDhTRe4A19c0TmV1udTSbxabwQJbN1zIGbG
3KQsumg9/70PaIkawnihx9/TxGCrJkAH5W+9pkX7OWefsahylwiPkKRkJRv2em/v
rMRyb3LxRU5t84k1xQqCnMxTEWOAiWoOtatExZ38a8kn8kqiE1NEvtPOzSrVjSvD
7pH56AlvO7hzGzng60kyWnAz6O5rQ0tsFIgW9xloWTQQVcfEtzvjc8ptuP9onkbA
jMRDJaqLXrIVtB7GMek7S6AO
-----END CERTIFICATE-----)";

// 更新間隔(ms)
#define UPDATE_PERIOD_MS (1UL * 60 * 60 * 1000)
#define UPDATE_RETRY 3

// タイムアウト
#define CLIENT_TIMEOUT 10000
#define AP_TIMEOUT 20000
#define HTTP_HEADER_TIMEOUT 10000
#define HTTP_CONTENTS_TIMEOUT 10000

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

WiFiClientSecure client;
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
  int parseerror;
  unsigned long starttime;
  // XMLパース用
  bool tag = false;
  char tagname[32];
  int tagname_cnt = 0;
  int description_cnt = 0;
  char telop[64];
  int telop_cnt = 0;

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
  if (!client.connect(API_SERVER, 443)) {
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
  starttime = millis();
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

  // パケットを受信しながらXMLをパース
  // <description>タグを階層によらずにチェックするだけ
  starttime = millis();
  while ((millis() - starttime < HTTP_CONTENTS_TIMEOUT)) {
    if (client.available()) {
      int c = client.read();

      if (c == '<') {
        // タグ開始
        tag = true;
        // <description>タグの終わり
        if (description_cnt == API_DESCRIPTION_LEVEL){
          telop[telop_cnt++] = '\0';
          break;
        }
        telop_cnt = 0;
      }
      else if (c == '>') {
        // タグ終了
        // タグは<description>タグか？
        if (strstr(tagname, "description") == tagname) {
          description_cnt++;
        }
        tag = false;
        tagname_cnt = 0;
      }
      else if(tag) {
        // タグ名を溜め込む
        tagname[tagname_cnt++] = c;
      }
      else if (description_cnt == API_DESCRIPTION_LEVEL) {
        // <description>タグ中なら文字列を溜め込む
        telop[telop_cnt++] = c;
      }
    }
  }
  // コンテンツタイムアウト
  if(millis() - starttime >= HTTP_CONTENTS_TIMEOUT) {
      returncode = WEATHER_TIMEOUT;
      goto EXIT;
  }
  if (telop_cnt == 0) {
      returncode = WEATHER_TIMEOUT;
      goto EXIT;
  }

  // 予報文字列をパース
  if(parseWeatherStr(telop, forecast) 
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

  client.setCACert(cybertrust_root_ca);
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
