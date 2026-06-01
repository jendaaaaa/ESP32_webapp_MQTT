#include <WiFi.h>
#include <WebServer.h>
#include <Preferences.h>
#include <DNSServer.h>

// Initialize core components
Preferences preferences;
WebServer server(80);
DNSServer dnsServer;

const byte DNS_PORT = 53;
const char* apName = "Glasshouse-Setup";
const char* apPass = "admin123";

// The HTML/CSS String Literal
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

void setup() {
  Serial.begin(115200);
  delay(1000);

  // Open 'wifi' namespace in read/write mode
  preferences.begin("wifi", false); 
  
  String savedSSID = preferences.getString("ssid", "");
  String savedPass = preferences.getString("pass", "");

  if (savedSSID == "") {
    // ---- NO SAVED WIFI: START CAPTIVE PORTAL ----
    Serial.println("\nNo Wi-Fi saved. Starting Setup Network...");
    
    // Start Access Point
    WiFi.mode(WIFI_AP);
    WiFi.softAP(apName, apPass);
    
    // Start DNS server to redirect all requests to the ESP32 IP
    dnsServer.start(DNS_PORT, "*", WiFi.softAPIP());

    // Serve the HTML page
    server.on("/", HTTP_GET, []() {
      server.send(200, "text/html", index_html);
    });

    // Handle Form Submission
    server.on("/connect", HTTP_POST, []() {
      String newSSID = server.arg("ssid");
      String newPass = server.arg("pass");

      Serial.println("Received new credentials:");
      Serial.println("SSID: " + newSSID);
      
      // Save to memory
      preferences.putString("ssid", newSSID);
      preferences.putString("pass", newPass);

      // Send success message before restarting
      server.send(200, "text/html", "<h2>Credentials Saved! Restarting...</h2><p>You can close this page.</p>");
      delay(2000);
      ESP.restart();
    });

    // Catch-all for Captive Portal routing
    server.onNotFound([]() {
      server.send(200, "text/html", index_html);
    });

    server.begin();
    Serial.print("Setup network ready. Connect to: ");
    Serial.println(apName);

  } else {
    // ---- WIFI FOUND: CONNECT TO ROUTER ----
    Serial.println("\nSaved credentials found! Connecting to router...");
    WiFi.mode(WIFI_STA);
    WiFi.begin(savedSSID.c_str(), savedPass.c_str());

    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 20) {
      delay(500);
      Serial.print(".");
      attempts++;
    }

    if (WiFi.status() == WL_CONNECTED) {
      Serial.println("\nSuccess! Connected to Wi-Fi.");
      Serial.print("IP Address: ");
      Serial.println(WiFi.localIP());
      // --> FUTURE MQTT CODE GOES HERE <--
    } else {
      Serial.println("\nFailed to connect. Clearing memory and restarting...");
      preferences.clear(); // Wipe bad password
      delay(1000);
      ESP.restart(); // Will reboot into Setup Mode
    }
  }
}

void loop() {
  // Only process web and DNS requests if we are in Setup Mode
  if (WiFi.getMode() == WIFI_AP) {
    dnsServer.processNextRequest();
    server.handleClient();
  }
}