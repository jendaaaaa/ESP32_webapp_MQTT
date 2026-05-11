# ESP32 WiFi Manager (AsyncWebServer)
Running a WiFi manager locally on the ESP32 to create a connection to an internet enabled wireless network. When the ESP is powered, it acts as an AP. When connected, a list of WiFi networks is shown. When selected, a required password is entered and stored inside the ESP. Now the ESP is exposed to the internet and can send data using MQTT.

[Source](https://randomnerdtutorials.com/esp32-wi-fi-manager-asyncwebserver/)

## Prerequisities
* **ESP32** (DevBoard)
* **VS Code with PlatformIO / ArduinoIDE** for coding
* **LittleFS Uploader** inside chosen IDE for uploading files to the ESP32 (should be included in PlatformIO/ESP32 core)

### Libraries
* ESPAsyncWebServer
* AsyncTCP