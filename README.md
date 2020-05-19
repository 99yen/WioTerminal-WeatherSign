# WioTerminal-BulbWeatherBoard
WioTerminalで相鉄の駅にあった電光掲示板を再現するスケッチ

https://twitter.com/103yen/status/1261860567997755392

# 使用ライブラリ
次のライブラリをインストールしてください。
- ArduinoJson https://arduinojson.org/

# 注意点
2020/5/19現在のWioTerminalのWiFiライブラリには、液晶と同時に使用すると描画速度が悪化する問題があります。

ひとまずこの問題を無理やり回避するため、Arduino/libraries/esp-at-lib-develop/src/esp_ll_arduino.cppの90行目を以下のように変更してからビルドしてください。
```
// static esp_sys_thread_t usart_ll_thread_id;
esp_sys_thread_t usart_ll_thread_id;
```
