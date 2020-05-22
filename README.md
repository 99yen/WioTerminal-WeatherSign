# WioTerminal-WeatherSign
WioTerminalで相鉄の駅にあった電光掲示板を再現するスケッチ

説明
http://y99.hateblo.jp/entry/2020/05/22/211736

# 使用ライブラリ
次のライブラリをインストールしてください。
- ArduinoJson https://arduinojson.org/

# 設定
環境に合わせて以下の定数を設定する
- WIFI_SSID 接続先APのSSID
- WIFI_PASSWORD 接続先APのパスワード
- API_RESOURCE city=以降を取得したい地域IDに変更する。デフォルトは東京(130010)
- UPDATE_PERIOD_MS 更新間隔をmsで指定。デフォルトは60分

# ライブラリの修正
2020/5/19現在のWioTerminalのWiFiライブラリには、液晶と同時に使用すると描画速度が悪化する問題があります。

ひとまずこの問題を無理やり回避するため、Arduino/libraries/esp-at-lib-develop/src/esp_ll_arduino.cppの90行目を以下のように変更してからビルドしてください。
```
// static esp_sys_thread_t usart_ll_thread_id;
esp_sys_thread_t usart_ll_thread_id;
```
