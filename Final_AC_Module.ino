#include <WiFi.h>
#include <FirebaseESP32.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <HTTPClient.h>
#include <time.h>

// --------------------------
// Wi-Fi Credentials
// --------------------------
#define WIFI_SSID "Dialog 4G 512"
#define WIFI_PASSWORD "744e38d1"

// --------------------------
// Firebase Credentials
// --------------------------
#define FIREBASE_HOST "https://fault-monitoring-system-default-rtdb.asia-southeast1.firebasedatabase.app/"
#define FIREBASE_AUTH "aXfaL7b6kL0GG3TfBhVHtW39MXMNVsPk9zLfQMrb"

// --------------------------
// Google Sheets Web App URL
// --------------------------
const char* googleScriptURL = "https://script.google.com/macros/s/AKfycbwRszkEtz-PSr4TwEpkeFDo_FCOvNj7nik3CCGICCrviBgBHfwU6bxNZPCF1wfM9A_X/exec";

// --------------------------
// NTP Time Settings
// --------------------------
const char* ntpServer = "pool.ntp.org";
const long gmtOffset_sec = 19800;
const int daylightOffset_sec = 0;

// --------------------------
// Sensor Pins
// --------------------------
#define ZMCT_PIN 32     // ZMCT103C analog pin (GPIO 33)
#define ZMPT_PIN 35     // ZMPT101B analog pin
#define ONE_WIRE_BUS 4  // DS18B20 data pin
#define RELAY_PIN 15    // Relay control pin

// DS18B20 instance
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);

// Firebase objects
FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;

// Calibration constants
const float CURRENT_CAL = 0.3166; // Calibrated for 35mA with 7W bulb at 220V
const float VOLTAGE_CAL = 0.5782; // Calibrated for ~220V
const float CURRENT_THRESHOLD = 1.0; // mA
const float VOLTAGE_THRESHOLD = 50.0; // V
const float POWER_FACTOR = 0.795;

// Global relay status
bool relayStatus = true;

// Function declarations
float readACCurrent();
float readACVoltage();
String getFormattedTime();
String getDate();
void sendToFirebase(float voltage, float current, float power, float temperature, bool relayState);
void sendToGoogleSheets(float voltage, float current, float power, float temperature, bool relayState);
void updateRelayState();

void setup() {
  Serial.begin(115200);
  delay(1000);

  // Set ADC resolution and attenuation
  analogReadResolution(12); // 12-bit ADC (0–4095)
  analogSetAttenuation(ADC_11db); // Full 0–3.3V range

  // Test ADC on ZMCT_PIN
  Serial.println("🔍 Testing ADC on GPIO " + String(ZMCT_PIN));
  for (int i = 0; i < 20; i++) {
    int adcValue = analogRead(ZMCT_PIN);
    Serial.print("ADC Sample ");
    Serial.print(i);
    Serial.print(": ");
    Serial.println(adcValue);
    delay(100);
  }

  // Initialize relay pin
  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, HIGH); // Start with relay OFF (active-low)

  // Connect to Wi-Fi
  Serial.print("Connecting to Wi-Fi");
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println();
  Serial.println("✅ WiFi connected");
  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);

  // Initialize DS18B20
  sensors.begin();

  // Firebase setup
  config.host = FIREBASE_HOST;
  config.signer.tokens.legacy_token = FIREBASE_AUTH;
  Firebase.begin(&config, &auth);
  Firebase.reconnectWiFi(true);
  Serial.println("✅ Firebase connected");

  // Set initial relay state after reset
  if (esp_reset_reason() == ESP_RST_POWERON) {
    relayStatus = true;
    digitalWrite(RELAY_PIN, LOW); // Relay ON (active-low)
    Firebase.setBool(fbdo, "/modules/ac1/relayState", true);
    Serial.println("🖲 Relay turned ON after power-on reset");
  } else {
    if (Firebase.getBool(fbdo, "/modules/ac1/relayState")) {
      relayStatus = fbdo.boolData();
      digitalWrite(RELAY_PIN, relayStatus ? LOW : HIGH); // Active-low
      Serial.println("🖲 Relay state restored from Firebase: " + String(relayStatus ? "ON" : "OFF"));
    } else {
      relayStatus = false;
      digitalWrite(RELAY_PIN, HIGH); // Relay OFF
      Firebase.setBool(fbdo, "/modules/ac1/relayState", false);
      Serial.println("⚠ Failed to get relay state, defaulting to OFF: " + fbdo.errorReason());
    }
  }
}

void loop() {
  // Reconnect Wi-Fi if disconnected
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("⚠ WiFi disconnected, reconnecting...");
    WiFi.reconnect();
    delay(1000);
    return;
  }

  // Update relay state from Firebase
  updateRelayState();

  // Declare variables
  float voltage, current, power, temp;

  // Check relay status and set values
  Serial.println("🔍 Relay Status: " + String(relayStatus ? "ON" : "OFF"));
  if (!relayStatus) {
    voltage = 0.0;
    current = 0.0;
    power = 0.0;
    temp = 0.0;
    Serial.println("System is OFF, setting all values to 0");
    Serial.print("Current forced to: ");
    Serial.println(current); // Debug
  } else {
    Serial.println("System is ON, reading sensor values");
    voltage = readACVoltage();
    current = readACCurrent();
    power = voltage * (current / 1000.0) * POWER_FACTOR; // Current in A
    sensors.requestTemperatures();
    temp = sensors.getTempCByIndex(0);
  }

  // Read threshold from Firebase
  float threshold = 50.0;
  if (Firebase.getFloat(fbdo, "/settings/threshold")) {
    threshold = fbdo.floatData();
  } else {
    Firebase.setFloat(fbdo, "/settings/threshold", threshold);
    Serial.println("⚠ Threshold not found, default set to 50.0");
  }
  Serial.println("✅ Using Threshold: " + String(threshold));

  // Auto-shutdown if temperature exceeds threshold
  if (relayStatus && temp > threshold) {
    relayStatus = false;
    digitalWrite(RELAY_PIN, HIGH); // Relay OFF
    Firebase.setBool(fbdo, "/modules/ac1/relayState", false);
    Firebase.setString(fbdo, "/shutdown_event", "AC1: Temp exceeded threshold");
    Serial.println("🔥 System shut down due to high temperature!");
  }

  // Send data to Firebase and Google Sheets
  sendToFirebase(voltage, current, power, temp, relayStatus);
  sendToGoogleSheets(voltage, current, power, temp, relayStatus);
  delay(5000);
}

float readACCurrent() {
  if (!relayStatus) {
    Serial.println("🖲 Relay is OFF, returning 0mA for current");
    return 0.0;
  }

  int maxValue = 0;
  int minValue = 4095;
  int sensorValue;
  Serial.println("🔬 Reading current on GPIO " + String(ZMCT_PIN));
  Serial.println("Raw ADC Values (first 10 samples):");
  for (int i = 0; i < 2000; i++) {
    long sum = 0;
    int numReads = 10; // Average 10 readings
    for (int j = 0; j < numReads; j++) {
      sum += analogRead(ZMCT_PIN);
      delayMicroseconds(10);
    }
    sensorValue = sum / numReads;
    if (i < 10) {
      Serial.print("Sample ");
      Serial.print(i);
      Serial.print(": ");
      Serial.println(sensorValue);
    }
    if (sensorValue > maxValue) maxValue = sensorValue;
    if (sensorValue < minValue) minValue = sensorValue;
    delayMicroseconds(50);
  }
  Serial.print("ZMCT Max Value: ");
  Serial.println(maxValue);
  Serial.print("ZMCT Min Value: ");
  Serial.println(minValue);
  float peakValue = (maxValue - minValue) / 2.0;
  Serial.print("ZMCT Peak Value: ");
  Serial.println(peakValue);
  float peakCurrent = peakValue * CURRENT_CAL;
  float rmsCurrent = peakCurrent / sqrt(2); // in milliamps
  if (rmsCurrent < 5.0) { // Lowered noise threshold
    Serial.println("Current below 5mA, returning 0mA");
    return 0.0;
  }
  Serial.print("🔬 Peak Current (mA): ");
  Serial.println(peakCurrent);
  Serial.print("🔬 RMS Current (mA): ");
  Serial.println(rmsCurrent);
  return rmsCurrent;
}

float readACVoltage() {
  if (!relayStatus) {
    Serial.println("🖲 Relay is OFF, returning 0V for voltage");
    return 0.0;
  }

  float totalPeakValue = 0.0;
  int samples = 5;
  for (int j = 0; j < samples; j++) {
    int maxValue = 0;
    int minValue = 4095;
    for (int i = 0; i < 1000; i++) {
      int sensorValue = analogRead(ZMPT_PIN);
      if (sensorValue > maxValue) maxValue = sensorValue;
      if (sensorValue < minValue) minValue = sensorValue;
      delayMicroseconds(100);
    }
    float peakValue = (maxValue - minValue) / 2.0;
    totalPeakValue += peakValue;
  }
  float avgPeakValue = totalPeakValue / samples;
  Serial.print("ZMPT Avg Peak Value: ");
  Serial.println(avgPeakValue);
  float peakVoltage = avgPeakValue * VOLTAGE_CAL;
  float rmsVoltage = peakVoltage / sqrt(2);
  Serial.print("RMS Voltage: ");
  Serial.println(rmsVoltage);
  return (rmsVoltage < VOLTAGE_THRESHOLD) ? 0.0 : rmsVoltage;
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

void sendToFirebase(float voltage, float current, float power, float temperature, bool relayState) {
  String timestamp = getFormattedTime();
  String path = "/modules/ac1";

  if (!relayState) {
    voltage = 0.0;
    current = 0.0;
    power = 0.0;
    temperature = 0.0;
    Serial.println("🖲 Relay OFF: Forcing all values to 0 in Firebase");
  }

  Serial.println("===============================");
  Serial.println("Sending to Firebase:");
  Serial.print("🔌 Voltage:   "); Serial.print(voltage, 2); Serial.println(" V");
  Serial.print("⚡ Current:   "); Serial.print(current, 2); Serial.println(" mA");
  Serial.print("🔋 Power:     "); Serial.print(power, 2); Serial.println(" W");
  Serial.print("🌡 Temp:      "); Serial.print(temperature, 2); Serial.println(" °C");
  Serial.print("🖲 Relay:     "); Serial.println(relayState ? "ON" : "OFF");
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

void sendToGoogleSheets(float voltage, float current, float power, float temperature, bool relayState) {
  String timestamp = getFormattedTime();
  String date = getDate();

  if (!relayState) {
    voltage = 0.0;
    current = 0.0;
    power = 0.0;
    temperature = 0.0;
    Serial.println("🖲 Relay OFF: Forcing all values to 0 in Google Sheets");
  }

  Serial.println("===============================");
  Serial.println("Sending to Google Sheets:");
  Serial.print("🔌 Voltage:   "); Serial.print(voltage, 2); Serial.println(" V");
  Serial.print("⚡ Current:   "); Serial.print(current, 2); Serial.println(" mA");
  Serial.print("🔋 Power:     "); Serial.print(power, 2); Serial.println(" W");
  Serial.print("🌡 Temp:      "); Serial.print(temperature, 2); Serial.println(" °C");
  Serial.print("🖲 Relay:     "); Serial.println(relayState ? "ON" : "OFF");
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
    jsonPayload += "\"relayState\":" + String(relayState ? "true" : "false") + "}";

    Serial.print("Sending JSON payload: ");
    Serial.println(jsonPayload);

    int httpResponseCode = http.POST(jsonPayload);

    if (httpResponseCode > 0) {
      String response = http.getString();
      Serial.print("HTTP Response code: ");
      Serial.println(httpResponseCode);
      Serial.print("Response: ");
      Serial.println(response);
    } else {
      Serial.print("⚠ Error sending data to Google Sheets, HTTP code: ");
      Serial.println(httpResponseCode);
    }

    http.end();
  } else {
    Serial.println("⚠ WiFi not connected, cannot send to Google Sheets");
  }
}

void updateRelayState() {
  String path = "/modules/ac1/relayState";
  Serial.println("🔍 Reading relay state from Firebase: " + path);
  int retries = 3;
  bool success = false;
  for (int i = 0; i < retries && !success; i++) {
    if (Firebase.getBool(fbdo, path)) {
      bool newRelayState = fbdo.boolData();
      Serial.print("Firebase relay state: ");
      Serial.println(newRelayState ? "ON" : "OFF");
      if (newRelayState != relayStatus) {
        relayStatus = newRelayState;
        digitalWrite(RELAY_PIN, relayStatus ? LOW : HIGH);
        Serial.println("🖲 Relay state updated: " + String(relayStatus ? "ON" : "OFF"));
      } else {
        digitalWrite(RELAY_PIN, relayStatus ? LOW : HIGH);
        Serial.println("🖲 Relay state unchanged: " + String(relayStatus ? "ON" : "OFF"));
      }
      success = true;
    } else {
      Serial.println("⚠ Firebase error (attempt " + String(i + 1) + "): " + fbdo.errorReason());
      delay(1000);
    }
  }
  if (!success) {
    Serial.println("⚠ Failed to read relay state after " + String(retries) + " attempts");
    digitalWrite(RELAY_PIN, relayStatus ? LOW : HIGH);
    Serial.println("🖲 Retaining last relay state: " + String(relayStatus ? "ON" : "OFF"));
  }
}