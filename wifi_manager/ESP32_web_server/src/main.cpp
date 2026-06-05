#include <WiFi.h>
#include <WebServer.h>
#include <Preferences.h>
#include <DNSServer.h>

#include <WiFiClientSecure.h>
#include <PubSubClient.h>

// init components
Preferences preferences;
WebServer server(80);
DNSServer dnsServer;

// network
const byte DNS_PORT = 53;
const char* apName = "glasshouse-setup";
const char* apPass = "admin123";

// MQTT
const char* mqttServer = "24d796bf3e444971acbc9fef04cf1ef7.s1.eu.hivemq.cloud";
const int mqttPort = 8883;
const char* mqttUser = "tester";
const char* mqttPass = "tester123T";

const char* deviceID = "C3D4";
const char* subTopic = "glasshouse/C3D4/commands";
const char* pubTopic = "glasshouse/C3D4/moisture";

WiFiClientSecure secureClient;
PubSubClient mqttClient(secureClient);

// state machine
enum SystemState {
  BOOT,
  SETUP_PORTAL,
  WAITING_FOR_CREDENTIALS,
  START_WIFI_CONNECTION,
  WAITING_FOR_WIFI,
  CONNECT_MQTT,
  OPERATIONAL
};

SystemState state = BOOT;

// timer
unsigned long lastMqttRetry = 0;
unsigned long wifiStartTime = 0;
const unsigned long WIFI_TIMEOUT_MS = 10000;
unsigned long opPrintTime = 0;

// debug flags (REPLACE WITH PHYSICAL BUTTON!)
const bool DEBUG_FORCE_PORTAL = false;

// captive portal page
const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>Device Setup</title>
  <style>
    body { font-family: -apple-system, sans-serif; background-color: #f4f7f6; display: flex; justify-content: center; align-items: center; height: 100vh; margin: 0; }
    .card { background: white; padding: 2rem; border-radius: 12px; box-shadow: 0 4px 15px rgba(0,0,0,0.1); width: 100%; max-width: 320px; box-sizing: border-box; }
    h2 { margin-top: 0; color: #333; text-align: center; }
    p { color: #666; font-size: 0.9rem; text-align: center; margin-bottom: 1.5rem; }
    input { width: 100%; padding: 12px; margin-bottom: 15px; border: 1px solid #ddd; border-radius: 8px; box-sizing: border-box; font-size: 16px; }
    button { width: 100%; padding: 12px; background-color: #4CAF50; color: white; border: none; border-radius: 8px; font-size: 16px; font-weight: bold; cursor: pointer; }
  </style>
</head>
<body>
  <div class="card">
    <h2>Smart Glasshouse</h2>
    <p>Connect your device to Wi-Fi</p>
    <form action="/connect" method="POST">
      <input type="text" name="ssid" placeholder="Wi-Fi Name (SSID)" required>
      <input type="password" name="pass" placeholder="Password" required>
      <button type="submit">Connect</button>
    </form>
  </div>
</body>
</html>
)rawliteral";

void mqttCallback(char* topic, byte* payload, unsigned int length) {
  String message;
  for (unsigned int i = 0; i < length; i++) {
    message += (char)payload[i];
  }

  Serial.println("\n[MQTT_RX] topic: " + String(topic));
  Serial.println("[MQTT_RX] command: " + String(message));
  
  if (message == "PUMP_ON") {
    Serial.println("[MQTT_RX] action: TURNING PUMP ON!");
  } else if (message == "PUMP_OFF") {
    Serial.println("[MQTT_RX] action: TURNING PUMP OFF!");
  }
}

void setup() {
  Serial.begin(115200);
  delay(1000);

  secureClient.setInsecure();

  mqttClient.setServer(mqttServer, mqttPort);
  mqttClient.setCallback(mqttCallback);

  // HW PIN SETUP
  //
  //
  //
  //

  // set init state
  state = BOOT;
}

void loop() {
  switch (state) {

    case BOOT: {
      preferences.begin("wifi", false);

      // debug (REPLACE WITH PHYSICAL BUTTON!)
      if (DEBUG_FORCE_PORTAL) {
        Serial.println("\n[BOOT] [DEBUG_MODE] wiping saved wifi credentials!");
        preferences.clear();
      }

      if (preferences.getString("ssid", "") == "") {
        Serial.println("\n[BOOT] no wifi saved, moving to portal setup...");
        state = SETUP_PORTAL;
      } else {
        Serial.println("\n[BOOT] saved credentials found!");
        state = START_WIFI_CONNECTION;
      }

      break;
    }
    
    case SETUP_PORTAL: {
      Serial.println("\n[SETUP_PORTAL] starting access point...");
      
      WiFi.mode(WIFI_AP);
      WiFi.softAP(apName, apPass);
      
      dnsServer.start(DNS_PORT, "*", WiFi.softAPIP());

      server.on("/", HTTP_GET, []() {
        server.send(200, "text/html", index_html);
      });

      server.on("/connect", HTTP_POST, []() {
        String newSSID = server.arg("ssid");
        String newPass = server.arg("pass");

        Serial.println("\n[SETUP_PORTAL] received credentials, saving...");
        
        preferences.putString("ssid", newSSID);
        preferences.putString("pass", newPass);

        server.send(200, "text/html", "<h2>Credentials Saved! Restarting...</h2><p>You can close this page.</p>");
        delay(2000);
        ESP.restart();
      });

      server.onNotFound([]() {
        server.send(200, "text/html", index_html);
      });

      server.begin();
      Serial.println("\n[SETUP_PORTAL] setup network ready, connect to:");
      Serial.println(apName);
      
      state = WAITING_FOR_CREDENTIALS;
      break;
    }
    
    case WAITING_FOR_CREDENTIALS: {
      dnsServer.processNextRequest();
      server.handleClient();
      
      if (millis() - opPrintTime >= 1000) {
        Serial.println("\n[WAITING_FOR_CREDENTIALS] waiting...");
        opPrintTime = millis();
      }

      break;
    }
    
    case START_WIFI_CONNECTION: {
      Serial.println("\n[START_WIFI_CONNECTION] attempting to connect to router...");
      
      String savedSSID = preferences.getString("ssid", "");
      String savedPass = preferences.getString("pass", "");

      WiFi.mode(WIFI_STA);
      WiFi.disconnect();
      WiFi.begin(savedSSID.c_str(), savedPass.c_str());

      wifiStartTime = millis();
      state = WAITING_FOR_WIFI;
      break;
    }
    
    case WAITING_FOR_WIFI: {
      if (WiFi.status() == WL_CONNECTED) {
        Serial.println("\n[WAITING_FOR_WIFI] success! connected to wifi!");
        Serial.print("IP Address: ");
        Serial.println(WiFi.localIP());
        
        state = CONNECT_MQTT;
        break;
      }
      
      if (millis() - wifiStartTime >= WIFI_TIMEOUT_MS) {
        Serial.println("\n[WAITING_FOR_WIFI] wifi connection timed out, clearing memory and restarting...");
        preferences.clear();
        delay(500);
        ESP.restart();
      }

      break;
    }
    
    case CONNECT_MQTT: {
      if (millis() - lastMqttRetry >= 5000) {
        Serial.println("\n[CONNECT_MQTT] connecting to HiveMQ...");
        
        if (mqttClient.connect(deviceID, mqttUser, mqttPass)) {
          Serial.println("[CONNECT_MQTT] success! connected to broker!");
          
          mqttClient.subscribe(subTopic);
          
          state = OPERATIONAL;
        } else {
          Serial.print("[CONNECT_MQTT] failed! rc=");
          Serial.print(mqttClient.state());
          Serial.println(" | trying again in 5 seconds...");
          lastMqttRetry = millis();
        }
      }
      break;
    }

    case OPERATIONAL: {
      if (!mqttClient.connected()) {
        Serial.println("\n[OPERATIONAL] MQTT client connection lost! reconnecting...");
        state = CONNECT_MQTT;
        break;
      }

      mqttClient.loop();

      if (millis() - opPrintTime >= 1000) {
        int fakeMoisture = random(30, 80);
        String payload = String(fakeMoisture);

        mqttClient.publish(pubTopic, payload.c_str());

        Serial.println("\n[OPERATIONAL] published moisture: " + payload + "%");
        opPrintTime = millis();
      }
      break;
    }
  }
}