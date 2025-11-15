/*********
  Rui Santos
  Complete project details at https://RandomNerdTutorials.com  
*********/

// Import required libraries
#ifdef ESP32
  #include <WiFi.h>
  #include <ESPAsyncWebServer.h>
  #include <HTTPClient.h>
  #include <WiFiClientSecure.h>
#else
  #include <Arduino.h>
  #include <ESP8266WiFi.h>
  #include <Hash.h>
  #include <ESPAsyncTCP.h>
  #include <ESPAsyncWebServer.h>
  #include <ESP8266HTTPClient.h>
  #include <WiFiClient.h>
#endif
#include <OneWire.h>
#include <DallasTemperature.h>
#include <PubSubClient.h>
#include "secrets.h"
#include "device_config.h"

// Data wire is connected to GPIO 4
#define ONE_WIRE_BUS 4

// Setup a oneWire instance to communicate with any OneWire devices
OneWire oneWire(ONE_WIRE_BUS);

// Pass our oneWire reference to Dallas Temperature sensor 
DallasTemperature sensors(&oneWire);

// Variables to store temperature values
String temperatureF = "--";
String temperatureC = "--";

// Timer variables
unsigned long lastTime = 0;  
unsigned long timerDelay = 30000;
unsigned long lastWiFiCheck = 0;
const unsigned long WIFI_CHECK_INTERVAL = 30000;  // Check WiFi every 30 seconds

// WiFi credentials are stored in include/secrets.h as WIFI_SSID and WIFI_PASSWORD

// Create AsyncWebServer object on port 80
AsyncWebServer server(80);

// MQTT for remote logging
WiFiClient espClient;
PubSubClient mqttClient(espClient);

String readDSTemperatureC() {
  // This function is kept for compatibility but calls the shared updater below.
  sensors.requestTemperatures();
  float tempC = sensors.getTempCByIndex(0);
  if (tempC == DEVICE_DISCONNECTED_C) {
    Serial.println("Failed to read from DS18B20 sensor (C)");
    return String("--");
  }
  return String(tempC);
}

String readDSTemperatureF() {
  sensors.requestTemperatures();
  float tempF = sensors.getTempFByIndex(0);
  if (int(tempF) == DEVICE_DISCONNECTED_F) {
    Serial.println("Failed to read from DS18B20 sensor (F)");
    return String("--");
  }
  return String(tempF);
}

// Simple logging function - just output to serial
void logMessage(String msg) {
  Serial.println(msg);
}

// Update both temperature globals in one call. Use this in loop() to refresh values.
void updateTemperatures() {
  sensors.requestTemperatures();
  float tC = sensors.getTempCByIndex(0);
  if (tC == DEVICE_DISCONNECTED_C) {
    temperatureC = "--";
    temperatureF = "--";
    Serial.println("DS18B20 read failed");
  } else {
    // Format with 2 decimal places
    char buf[16];
    dtostrf(tC, 0, 2, buf);
    temperatureC = String(buf);
    float tF = sensors.toFahrenheit(tC);
    dtostrf(tF, 0, 2, buf);
    temperatureF = String(buf);
    logMessage("Temperature C: " + temperatureC);
    logMessage("Temperature F: " + temperatureF);
  }
}

void sendToInfluxDB() {
  if (WiFi.status() == WL_CONNECTED && temperatureC != "--") {
    WiFiClientSecure client;
    // Disable SSL certificate verification (safe for trusted networks)
    client.setInsecure();
    
    HTTPClient http;
    String url = String(INFLUXDB_URL) + "/api/v2/write?bucket=" + String(INFLUXDB_BUCKET) + "&precision=s";
    
    logMessage("InfluxDB URL: " + url);
    
    http.begin(client, url);
    http.addHeader("Authorization", "Token " + String(INFLUXDB_TOKEN));
    http.addHeader("Content-Type", "text/plain");

    // Line protocol payload for InfluxDB
    String deviceTag = String(DEVICE_LOCATION);
    deviceTag.replace(" ", "_");
    String payload = "temperature,sensor=ds18b20,location=esp12f,device=" + deviceTag + " tempC=" + temperatureC + ",tempF=" + temperatureF;
    
    int httpCode = http.POST(payload);
    
    if (httpCode > 0) {
      if (httpCode == 204) {
        logMessage("InfluxDB: 204 OK");
      } else {
        String response = http.getString();
        logMessage("InfluxDB Code " + String(httpCode));
      }
    } else {
      logMessage("InfluxDB POST failed");
    }
    http.end();
  } else {
    Serial.println("WiFi not connected or no valid temperature");
  }
}

void sendToLambda() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi not connected, cannot send logs to Lambda");
    return;
  }

  // Don't send if we have no temperature data
  if (temperatureC == "--") {
    return;
  }

  WiFiClientSecure client;
  client.setInsecure();  // Disable SSL verification for ESP8266
  
  HTTPClient http;
  
  Serial.println("Sending temperature to Lambda endpoint...");
  
  http.begin(client, LAMBDA_ENDPOINT);
  http.addHeader("Content-Type", "application/json");
  
  // Build JSON payload - simplified to reduce memory usage
  // Format: {"device":"Location","tempC":22.5,"tempF":72.5}
  char jsonBuffer[256];
  snprintf(jsonBuffer, sizeof(jsonBuffer), 
    "{\"device\":\"%s\",\"tempC\":%s,\"tempF\":%s}",
    DEVICE_LOCATION, temperatureC.c_str(), temperatureF.c_str());
  
  String payload(jsonBuffer);
  
  Serial.println("Lambda payload size: " + String(payload.length()) + " bytes");
  
  int httpCode = http.POST(payload);
  Serial.println("Lambda HTTP Code: " + String(httpCode));
  
  if (httpCode > 0) {
    if (httpCode == 200 || httpCode == 204) {
      String response = http.getString();
      Serial.println("Lambda response: " + response);
      Serial.println("Data sent to Lambda successfully!");
    } else {
      String response = http.getString();
      Serial.println("Lambda Response " + String(httpCode) + ": " + response);
    }
  } else {
    Serial.println("Lambda POST error: " + String(http.errorToString(httpCode)));
  }
  
  http.end();
}

const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE HTML><html>
<head>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <link rel="stylesheet" href="https://use.fontawesome.com/releases/v5.7.2/css/all.css" integrity="sha384-fnmOCqbTlWIlj8LyTjo7mOUStjsKC4pOpQbqyi7RrhN7udi9RwhKkMHpvLbHG9Sr" crossorigin="anonymous">
  <style>
    html {
     font-family: Arial;
     display: inline-block;
     margin: 0px auto;
     text-align: center;
    }
    h2 { font-size: 2.0rem; }
    p { font-size: 2.0rem; }
    .units { font-size: 1.0rem; }
    .ds-labels{
      font-size: 1.2rem;
      vertical-align:middle;
      padding-bottom: 15px;
    }
  </style>
</head>
<body>
  <h2>%PAGE_TITLE%</h2>
  <p>
    <i class="fas fa-thermometer-half" style="color:#059e8a;"></i> 
    <span class="ds-labels"></span> 
    <span id="temperaturec">%TEMPERATUREC%</span>
    <sup class="units">&deg;C</sup>
  </p>
  <p>
    <i class="fas fa-thermometer-half" style="color:#059e8a;"></i> 
    <span class="ds-labels"></span> 
    <span id="temperaturef">%TEMPERATUREF%</span>
    <sup class="units">&deg;F</sup>
  </p>
</body>
<script>
setInterval(function ( ) {
  var xhttp = new XMLHttpRequest();
  xhttp.onreadystatechange = function() {
    if (this.readyState == 4 && this.status == 200) {
      document.getElementById("temperaturec").innerHTML = this.responseText;
    }
  };
  xhttp.open("GET", "/temperaturec", true);
  xhttp.send();
}, 10000) ;
setInterval(function ( ) {
  var xhttp = new XMLHttpRequest();
  xhttp.onreadystatechange = function() {
    if (this.readyState == 4 && this.status == 200) {
      document.getElementById("temperaturef").innerHTML = this.responseText;
    }
  };
  xhttp.open("GET", "/temperaturef", true);
  xhttp.send();
}, 10000) ;
</script>
</html>)rawliteral";

// Replaces placeholder with DS18B20 values
String processor(const String& var){
  //Serial.println(var);
  if(var == "TEMPERATUREC"){
    return temperatureC;
  }
  else if(var == "TEMPERATUREF"){
    return temperatureF;
  }
  else if(var == "PAGE_TITLE"){
    return DEVICE_LOCATION;
  }
  return String();
}

void setup(){
  // Serial port for debugging purposes
  Serial.begin(115200);
  Serial.println();
  
  // Start up the DS18B20 library
  sensors.begin();
  updateTemperatures();

  // Connect to Wi-Fi
  #ifdef ESP32
    WiFi.setSleep(false);  // Disable sleep for stable connection (ESP32)
  #else
    WiFi.setSleepMode(WIFI_NONE_SLEEP);  // Disable sleep for stable connection (ESP8266)
  #endif
  if (strlen(STATIC_IP) > 0) {
    IPAddress local_IP;
    IPAddress gateway;
    IPAddress subnet;
    IPAddress dns;
    local_IP.fromString(STATIC_IP);
    gateway.fromString(STATIC_GATEWAY);
    subnet.fromString(STATIC_SUBNET);
    dns.fromString(STATIC_DNS);
    WiFi.config(local_IP, gateway, subnet, dns);
  }
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.println("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println();
  
  // Print ESP Local IP Address
  Serial.println(WiFi.localIP());

  // Connect to MQTT broker (disabled to save RAM on ESP8266)
  // mqttClient.setServer(MQTT_BROKER, MQTT_PORT);
  // logMessage("MQTT client configured");

  // Route for root / web page
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send_P(200, "text/html", index_html, processor);
  });
  server.on("/temperaturec", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(200, "text/plain", temperatureC);
  });
  server.on("/temperaturef", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(200, "text/plain", temperatureF);
  });
  // Start server
  server.begin();
}
 
void loop(){
  // Non-blocking WiFi connection check
  unsigned long now = millis();
  if (now - lastWiFiCheck > WIFI_CHECK_INTERVAL) {
    lastWiFiCheck = now;
    
    if (WiFi.status() != WL_CONNECTED) {
      Serial.println("WiFi disconnected, attempting reconnection...");
      WiFi.reconnect();  // Non-blocking reconnect attempt
    } else {
      Serial.println("WiFi connected, signal: " + String(WiFi.RSSI()) + " dBm");
    }
  }

  // MQTT reconnect (disabled by default, not used in current setup)
  // if (!mqttClient.connected()) {
  //   logMessage("Attempting MQTT reconnection...");
  //   String clientId = "ESP12fClient_" + String(ESP.getChipId(), HEX);
  //   if (mqttClient.connect(clientId.c_str(), MQTT_USER, MQTT_PASSWORD)) {
  //     logMessage("MQTT connected with ID: " + clientId);
  //   } else {
  //     Serial.print("MQTT connection failed, rc=");
  //     Serial.print(mqttClient.state());
  //     Serial.println(" retrying in 5 seconds");
  //     delay(5000);
  //   }
  // }

  if ((millis() - lastTime) > timerDelay) {
    updateTemperatures();
    if (temperatureC != "--") {
      sendToLambda();  // Send to AWS Lambda endpoint
      sendToInfluxDB();  // Also send to InfluxDB
    }
    lastTime = millis();
  }
    //Serial.println(WiFi.localIP());
}
