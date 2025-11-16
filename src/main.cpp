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
#include <ArduinoJson.h>
#include "secrets.h"
#include "device_config.h"

// Exponential backoff class for network operations
class NetworkBackoff {
private:
    unsigned long lastFailure = 0;
    int consecutiveFailures = 0;
    
public:
    unsigned long getBackoffDelay() {
        if (consecutiveFailures == 0) return 0;
        
        // Exponential backoff: 1s, 2s, 4s, 8s, 16s, 32s, 64s, 128s, 256s, 300s (capped)
        unsigned long delay = min((unsigned long)pow(2, consecutiveFailures - 1) * 1000, MAX_BACKOFF_MS);
        return max(delay, MIN_BACKOFF_MS);
    }
    
    void recordFailure() {
        consecutiveFailures = min(consecutiveFailures + 1, MAX_CONSECUTIVE_FAILURES);
        lastFailure = millis();
    }
    
    void recordSuccess() {
        consecutiveFailures = 0;
    }
    
    bool shouldRetry() {
        if (consecutiveFailures == 0) return true;
        return (millis() - lastFailure) >= getBackoffDelay();
    }
    
    int getConsecutiveFailures() {
        return consecutiveFailures;
    }
};

// Device metrics structure for monitoring
struct DeviceMetrics {
    unsigned long bootTime;
    unsigned int wifiReconnects;
    unsigned int sensorReadFailures;
    unsigned int lambdaSendFailures;
    unsigned int influxSendFailures;
    float minTempC;
    float maxTempC;
    unsigned long lastSuccessfulLambdaSend;
    unsigned long lastSuccessfulInfluxSend;
    
    DeviceMetrics() : bootTime(0), wifiReconnects(0), sensorReadFailures(0), 
                     lambdaSendFailures(0), influxSendFailures(0),
                     minTempC(999.0f), maxTempC(-999.0f),
                     lastSuccessfulLambdaSend(0), lastSuccessfulInfluxSend(0) {}
    
    void updateTemperature(float tempC) {
        if (tempC > -100.0f && tempC < 100.0f) {  // Valid temperature range
            minTempC = min(minTempC, tempC);
            maxTempC = max(maxTempC, tempC);
        }
    }
};

// Global instances
NetworkBackoff lambdaBackoff;
NetworkBackoff influxBackoff;
DeviceMetrics metrics;

// Data wire is connected to GPIO 4
OneWire oneWire(ONE_WIRE_PIN);

// Pass our oneWire reference to Dallas Temperature sensor 
DallasTemperature sensors(&oneWire);

// Variables to store temperature values
String temperatureF = "--";
String temperatureC = "--";

// Timer variables
unsigned long lastTime = 0;  
unsigned long timerDelay = TEMPERATURE_READ_INTERVAL_MS;
unsigned long lastWiFiCheck = 0;
const unsigned long WIFI_CHECK_INTERVAL = WIFI_CHECK_INTERVAL_MS;  // Check WiFi every 30 seconds

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

// Validate that temperature reading is valid
bool isValidTemperature(const String& temp) {
  return temp.length() > 0 && temp != "--";
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
    metrics.sensorReadFailures++;
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
    
    // Update metrics with valid temperature
    metrics.updateTemperature(tC);
  }
}

void sendToInfluxDB() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi not connected, skipping InfluxDB");
    return;
  }

  // Check if we should retry based on exponential backoff
  if (!influxBackoff.shouldRetry()) {
    Serial.println("InfluxDB backoff active, skipping send");
    return;
  }

  if (!isValidTemperature(temperatureC)) {
    Serial.println("Invalid temperature, skipping InfluxDB");
    return;
  }

  WiFiClientSecure client;
  // Disable SSL certificate verification (safe for trusted networks)
  client.setInsecure();
  
  HTTPClient http;
  http.setTimeout(HTTP_TIMEOUT_MS);  // Use constant
  String url = String(INFLUXDB_URL) + "/api/v2/write?bucket=" + String(INFLUXDB_BUCKET) + "&precision=s";
  
  Serial.println("InfluxDB URL: " + url);
  
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
      Serial.println("InfluxDB: 204 OK");
      
      // Record success
      influxBackoff.recordSuccess();
      metrics.lastSuccessfulInfluxSend = millis();
    } else {
      String response = http.getString();
      Serial.println("InfluxDB Code " + String(httpCode));
      
      // Record failure for backoff
      influxBackoff.recordFailure();
      metrics.influxSendFailures++;
    }
  } else {
    Serial.println("InfluxDB POST failed: " + String(http.errorToString(httpCode)));
    
    // Record failure for backoff
    influxBackoff.recordFailure();
    metrics.influxSendFailures++;
  }
  http.end();
}

void sendToLambda() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi not connected, skipping Lambda");
    return;
  }

  // Check if we should retry based on exponential backoff
  if (!lambdaBackoff.shouldRetry()) {
    Serial.println("Lambda backoff active, skipping send");
    return;
  }

  // Don't send if we have no valid temperature data
  if (!isValidTemperature(temperatureC)) {
    Serial.println("Invalid temperature, skipping Lambda");
    return;
  }

  WiFiClientSecure client;
  client.setInsecure();  // Disable SSL verification for ESP8266
  
  HTTPClient http;
  http.setTimeout(HTTP_TIMEOUT_MS);  // Use constant
  
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
      Serial.println("✅ Data sent to Lambda successfully!");
      
      // Record success
      lambdaBackoff.recordSuccess();
      metrics.lastSuccessfulLambdaSend = millis();
    } else {
      String response = http.getString();
      Serial.println("❌ Lambda Error " + String(httpCode) + ": " + response);
      
      // Record failure for backoff
      lambdaBackoff.recordFailure();
      metrics.lambdaSendFailures++;
    }
  } else {
    Serial.println("❌ Lambda POST error: " + String(http.errorToString(httpCode)));
    
    // Record failure for backoff
    lambdaBackoff.recordFailure();
    metrics.lambdaSendFailures++;
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

// Health check endpoint response
String getHealthStatus() {
  // Use JsonDocument for memory efficiency (replaces deprecated StaticJsonDocument)
  JsonDocument doc;
  
  doc["status"] = "ok";
  doc["device"] = DEVICE_LOCATION;
  doc["board"] = DEVICE_BOARD;
  doc["uptime_seconds"] = (millis() - metrics.bootTime) / 1000;
  doc["wifi_connected"] = (WiFi.status() == WL_CONNECTED);
  doc["wifi_rssi"] = WiFi.RSSI();
  doc["temperature_valid"] = isValidTemperature(temperatureC);
  doc["current_temp_c"] = temperatureC;
  doc["current_temp_f"] = temperatureF;
  doc["last_reading_ms"] = lastTime;
  
  // Metrics
  doc["metrics"]["wifi_reconnects"] = metrics.wifiReconnects;
  doc["metrics"]["sensor_read_failures"] = metrics.sensorReadFailures;
  doc["metrics"]["lambda_send_failures"] = metrics.lambdaSendFailures;
  doc["metrics"]["influx_send_failures"] = metrics.influxSendFailures;
  
  // Handle min/max temperature (avoid null values in JSON)
  if (metrics.minTempC < 900.0f) {
    doc["metrics"]["min_temp_c"] = metrics.minTempC;
  } else {
    doc["metrics"]["min_temp_c"] = nullptr;
  }
  
  if (metrics.maxTempC > -900.0f) {
    doc["metrics"]["max_temp_c"] = metrics.maxTempC;
  } else {
    doc["metrics"]["max_temp_c"] = nullptr;
  }
  
  // Backoff status
  doc["backoff"]["lambda_active"] = (lambdaBackoff.getConsecutiveFailures() > 0);
  doc["backoff"]["lambda_failures"] = lambdaBackoff.getConsecutiveFailures();
  doc["backoff"]["influx_active"] = (influxBackoff.getConsecutiveFailures() > 0);
  doc["backoff"]["influx_failures"] = influxBackoff.getConsecutiveFailures();
  
  // Last successful sends
  if (metrics.lastSuccessfulLambdaSend > 0) {
    doc["last_success"]["lambda_seconds_ago"] = (millis() - metrics.lastSuccessfulLambdaSend) / 1000;
  }
  if (metrics.lastSuccessfulInfluxSend > 0) {
    doc["last_success"]["influx_seconds_ago"] = (millis() - metrics.lastSuccessfulInfluxSend) / 1000;
  }
  
  String response;
  serializeJson(doc, response);
  return response;
}

void setup(){
  // Initialize metrics
  metrics.bootTime = millis();
  
  // Serial port for debugging purposes
  Serial.begin(115200);
  Serial.println();
  
  // Start up the DS18B20 library
  sensors.begin();
  updateTemperatures();

  // Connect to Wi-Fi with fallback support
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
  
  // Try connecting to each WiFi network in order
  bool wifiConnected = false;
  for (int netIndex = 0; netIndex < NUM_WIFI_NETWORKS && !wifiConnected; netIndex++) {
    Serial.println("Attempting WiFi: " + String(wifi_networks[netIndex].ssid));
    WiFi.begin(wifi_networks[netIndex].ssid, wifi_networks[netIndex].password);
    
    int wifiAttempts = 0;
    const int MAX_WIFI_ATTEMPTS = 20;  // ~10 seconds at 500ms intervals
    
    while (WiFi.status() != WL_CONNECTED && wifiAttempts < MAX_WIFI_ATTEMPTS) {
      delay(500);
      Serial.print(".");
      wifiAttempts++;
    }
    
    Serial.println();
    
    if (WiFi.status() == WL_CONNECTED) {
      wifiConnected = true;
      Serial.println("✅ WiFi connected to: " + String(wifi_networks[netIndex].ssid));
      Serial.println("IP: " + WiFi.localIP().toString());
      Serial.println("Signal strength: " + String(WiFi.RSSI()) + " dBm");
    } else {
      Serial.println("❌ Failed to connect to " + String(wifi_networks[netIndex].ssid));
      if (netIndex < NUM_WIFI_NETWORKS - 1) {
        Serial.println("Trying next network...");
      }
    }
  }
  
  if (!wifiConnected) {
    Serial.println("❌ Failed to connect to any WiFi network");
    Serial.println("Continuing with offline mode - will retry in main loop");
  }

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
  
  // Health check endpoint
  server.on("/health", HTTP_GET, [](AsyncWebServerRequest *request){
    String response = getHealthStatus();
    request->send(200, "application/json", response);
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
      metrics.wifiReconnects++;
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
