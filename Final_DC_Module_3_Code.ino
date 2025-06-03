#include <WiFi.h>
#include <Wire.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <INA226_WE.h>
#include <HTTPClient.h>
#include <time.h>
#include <FirebaseESP32.h>

// --------------------------
// Wi-Fi Credentials
// --------------------------
const char* ssid = "Dialog 4G 512";
const char* password = "744e38d1";

// --------------------------
// Firebase Configuration
// --------------------------
#define FIREBASE_HOST "https://fault-monitoring-system-default-rtdb.asia-southeast1.firebasedatabase.app/"
#define FIREBASE_AUTH "aXfaL7b6kL0GG3TfBhVHtW39MXMNVsPk9zLfQMrb"

// --------------------------
// Google Sheets Web App URL
// --------------------------
const char* googleScriptURL = "https://script.google.com/macros/s/AKfycby7wb4jVYCL5Rog96rIV8YfBuAcNymTn4NO4z9z8FUcUjXj46zI-XXIIuyD4M1jx8zTVg/exec";

// --------------------------
// NTP Time Settings
// --------------------------
const char* ntpServer = "pool.ntp.org";
const long gmtOffset_sec = 19800;
const int daylightOffset_sec = 0;

// --------------------------
// DS18B20 Setup
// --------------------------
#define DS18B20_PIN 4
OneWire oneWire(DS18B20_PIN);
DallasTemperature temperatureSensor(&oneWire);

// --------------------------
// INA226 Setup
// --------------------------
#define I2C_ADDRESS_SENSOR 0x40
#define SHUNT_RESISTOR 0.1
INA226_WE ina226(I2C_ADDRESS_SENSOR);

// --------------------------
// Relay Setup
// --------------------------
#define RELAY_PIN 15
bool relayStatus = true;

// Firebase objects
FirebaseData fbdo;
FirebaseConfig config;
FirebaseAuth auth;

// Function declarations
String getFormattedTime();
String getDate();
void sendToGoogleSheets(float voltage, float current, float power, float temperature);
void sendToFirebase(float voltage, float current, float power, float temperature, bool relayState);
void updateRelayState();

void setup() {
  Serial.begin(115200);
  delay(1000);

  // Wi-Fi
  WiFi.begin(ssid, password);
  Serial.print("Connecting to Wi-Fi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println();
  Serial.println("WiFi Connected!");
  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);

  // Firebase Setup
  config.host = FIREBASE_HOST;
  config.signer.tokens.legacy_token = FIREBASE_AUTH;
  Firebase.begin(&config, &auth);
  Firebase.reconnectWiFi(true);
  Serial.println("Firebase initialized");

  // Sensors Init
  Wire.begin();
  if (!ina226.init()) Serial.println("INA226 not found!");
  ina226.setAverage(AVERAGE_16);
  ina226.setConversionTime(CONV_TIME_8244);
  ina226.setResistorRange(SHUNT_RESISTOR, 3.2);

  temperatureSensor.begin();
  pinMode(RELAY_PIN, OUTPUT);

  // Set initial relay state after reset
  if (esp_reset_reason() == ESP_RST_POWERON) {
    relayStatus = true;
    digitalWrite(RELAY_PIN, LOW); // Relay ON (active-low)
    Serial.println("Relay turned ON after power-on reset");
  } else {
    relayStatus = false;
    digitalWrite(RELAY_PIN, HIGH); // Relay OFF
    Serial.println("Relay set to OFF after non-power-on reset");
  }
}

void loop() {
  // Reconnect Wi-Fi if disconnected
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi disconnected, reconnecting...");
    WiFi.reconnect();
    delay(1000);
    return;
  }

  // Update relay state from Firebase
  updateRelayState();

  // Read sensor values
  temperatureSensor.requestTemperatures();
  float temperature = temperatureSensor.getTempCByIndex(0);
  float voltage = ina226.getBusVoltage_V();
  float current = ina226.getCurrent_mA();
  float power = voltage * current / 1000.0;

  // If relay is off, set values to 0
  if (!relayStatus) {
    voltage = 0.0;
    current = 0.0;
    power = 0.0;
    temperature = 0.0;
    Serial.println("System is OFF, setting all values to 0");
  }

  // Read temperature threshold (hardcoded, as in original)
  float threshold = 50.0;
  Serial.print("✅ Using Threshold: ");
  Serial.println(threshold);

  // Auto-shutdown if temperature exceeds threshold (only if relay is on)
  if (relayStatus && temperature > threshold) {
    relayStatus = false;
    digitalWrite(RELAY_PIN, HIGH); // Relay OFF (active-low)
    Serial.println("🔥 System shut down due to high temperature!");
  }

  // Send data to Firebase and Google Sheets
  sendToFirebase(voltage, current, power, temperature, relayStatus);
  sendToGoogleSheets(voltage, current, power, temperature);
  delay(5000);
}

String getFormattedTime() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) return "Unknown";
  char buffer[30];
  strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", &timeinfo);
  return String(buffer);
}

String getDate() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) return "Unknown";
  char buffer[11];
  strftime(buffer, sizeof(buffer), "%Y-%m-%d", &timeinfo);
  return String(buffer);
}

void sendToGoogleSheets(float voltage, float current, float power, float temperature) {
  String timestamp = getFormattedTime();
  String date = getDate();

  Serial.println("===============================");
  Serial.println("Sending to Google Sheets:");
  Serial.print("🔌 Voltage:   "); Serial.print(voltage, 2); Serial.println(" V");
  Serial.print("⚡ Current:   "); Serial.print(current, 2); Serial.println(" mA");
  Serial.print("🔋 Power:     "); Serial.print(power, 2); Serial.println(" W");
  Serial.print("🌡 Temp:      "); Serial.print(temperature, 2); Serial.println(" °C");
  Serial.print("🖲 Relay:     "); Serial.println(relayStatus ? "ON" : "OFF");
  Serial.print("📅 Timestamp: "); Serial.println(timestamp);
  Serial.println("===============================\n");

  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    http.begin(googleScriptURL);
    http.addHeader("Content-Type", "application/json");

    String jsonPayload = "{\"timestamp\":\"" + timestamp + "\",";
    jsonPayload += "\"voltage\":" + String(voltage, 2) + ",";
    jsonPayload += "\"current\":" + String(current, 2) + ",";
    jsonPayload += "\"power\":" + String(power, 2) + ",";
    jsonPayload += "\"temperature\":" + String(temperature, 2) + ",";
    jsonPayload += "\"relayState\":" + String(relayStatus ? "true" : "false") + "}";

    int httpResponseCode = http.POST(jsonPayload);

    if (httpResponseCode > 0) {
      String response = http.getString();
      Serial.println("✅ Data sent to Google Sheets, response: " + response);
    } else {
      Serial.println("⚠ Error sending data: " + String(httpResponseCode));
    }

    http.end();
  } else {
    Serial.println("⚠ WiFi not connected");
  }
}

void sendToFirebase(float voltage, float current, float power, float temperature, bool relayState) {
  String date = getDate();
  String timestamp = getFormattedTime();
  String path = "/modules/dc_module_3";

  Serial.println("===============================");
  Serial.println("Sending to Firebase:");
  Serial.print("🔌 Voltage:   "); Serial.print(voltage, 2); Serial.println(" V");
  Serial.print("⚡ Current:   "); Serial.print(current, 2); Serial.println(" mA");
  Serial.print("🔋 Power:     "); Serial.print(power, 2); Serial.println(" W");
  Serial.print("🌡 Temp:      "); Serial.print(temperature, 2); Serial.println(" °C");
  Serial.print("🖲 Relay:     "); Serial.println(relayStatus ? "ON" : "OFF");
  Serial.print("📅 Timestamp: "); Serial.println(timestamp);
  Serial.println("===============================\n");

  Firebase.setFloat(fbdo, path + "/voltage", voltage);
  Firebase.setFloat(fbdo, path + "/current", current);
  Firebase.setFloat(fbdo, path + "/power", power);
  Firebase.setFloat(fbdo, path + "/temperature", temperature);
  Firebase.setBool(fbdo, path + "/relayState", relayState);
  Firebase.setString(fbdo, path + "/timestamp", timestamp);

  if (fbdo.errorReason().length() > 0) {
    Serial.println("⚠ Firebase error: " + fbdo.errorReason());
  } else {
    Serial.println("✅ Data sent to Firebase: " + path);
  }
}

void updateRelayState() {
  String path = "/modules/dc_module_3/relayState";
  if (Firebase.getBool(fbdo, path)) {
    bool newRelayState = fbdo.boolData();
    if (newRelayState != relayStatus) {
      relayStatus = newRelayState;
      digitalWrite(RELAY_PIN, relayStatus ? LOW : HIGH); // Active-low relay
      Serial.println("🖲 Relay state updated from Firebase: " + String(relayStatus ? "ON" : "OFF"));
    }
  } else {
    Serial.println("⚠ Error reading relay state from Firebase: " + fbdo.errorReason());
  }
}