#include <WiFi.h>
#include <FirebaseESP32.h>
#include "EmonLib.h" // For voltage measurement with ZMPT101B
#include "time.h"

// Wi-Fi Credentials
#define WIFI_SSID "abcd"
#define WIFI_PASSWORD "12345678"

// Firebase Credentials
#define FIREBASE_HOST "code-lab-app-461e1-default-rtdb.asia-southeast1.firebasedatabase.app/"
#define FIREBASE_AUTH "AIzaSyAqE7pe8ww06rVQay_D0kSzwSjSQxczPv4"

// Firebase objects
FirebaseConfig firebaseConfig;
FirebaseAuth firebaseAuth;
FirebaseData firebaseData;

// Voltage sensor (ZMPT101B)
EnergyMonitor emon_V;

// Current sensor (ZMCT103C)
#define CURRENT_SENSOR_PIN 34
#define NUM_SAMPLES 1000
#define ADC_RESOLUTION 4095.0
#define VREF 3.3
#define CALIBRATION_FACTOR 0.8 // Use your actual factor

// Energy calculation
float totalEnergy_kWh = 0.0;
unsigned long lastMillis = 0;
unsigned long lastRelayCheck = 0;

// Relay control
#define RELAY_PIN 26

// NTP time config
const char* ntpServer = "pool.ntp.org";
const long gmtOffset_sec = 5 * 3600 + 1800;
const int daylightOffset_sec = 0;

void setup() {
  Serial.begin(115200);
  analogReadResolution(12); // 12-bit ADC for ESP32
  delay(1000);
  Serial.println("Energy Monitoring System - ESP32");

  // Initialize voltage sensor
  emon_V.voltage(35, 110.5, 0);

  // Wi-Fi setup
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Connecting to Wi-Fi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.print(".");
  }
  Serial.println("\nConnected to Wi-Fi!");

  // Firebase setup
  firebaseConfig.host = FIREBASE_HOST;
  firebaseConfig.signer.tokens.legacy_token = FIREBASE_AUTH;
  Firebase.begin(&firebaseConfig, &firebaseAuth);
  Firebase.reconnectWiFi(true);

  // Restore previous energy
  restoreEnergyFromFirebase();

  // NTP
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  Serial.println("NTP Time synchronized!");

  lastMillis = millis();
  lastRelayCheck = millis();

  // Relay pin setup
  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, LOW);
}

void loop() {
  unsigned long currentMillis = millis();

  if (currentMillis - lastRelayCheck >= 1000) {
    checkRelayStatus();
    lastRelayCheck = currentMillis;
  }

  if (currentMillis - lastMillis >= 1000) {
    measureAndSendEnergy();
    lastMillis = currentMillis;
  }
}

void restoreEnergyFromFirebase() {
  if (Firebase.getFloat(firebaseData, "/TotalEnergy_kWh")) {
    totalEnergy_kWh = firebaseData.floatData();
    Serial.print("Recovered Energy from Firebase: ");
    Serial.print(totalEnergy_kWh, 5);
    Serial.println(" kWh");
  } else {
    Serial.println("No previous kWh data found, starting from 0.");
    totalEnergy_kWh = 0.0;
  }
}

String getSriLankanTime() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    Serial.println("Failed to obtain time");
    return "UnknownTime";
  }
  char timeStr[30];
  strftime(timeStr, sizeof(timeStr), "%Y-%m-%d_%H-%M-%S", &timeinfo);
  return String(timeStr);
}

void measureAndSendEnergy() {
  // Voltage measurement
  emon_V.calcVI(20, 2000);
  float Vrms = emon_V.Vrms;

  // Current measurement using ZMCT103C
  float voltageOffset = 0;
  long offsetSum = 0;
  for (int i = 0; i < NUM_SAMPLES; i++) {
    offsetSum += analogRead(CURRENT_SENSOR_PIN);
  }
  voltageOffset = (float)offsetSum / NUM_SAMPLES;

  float sum = 0;
  for (int i = 0; i < NUM_SAMPLES; i++) {
    int rawADC = analogRead(CURRENT_SENSOR_PIN);
    float centered = rawADC - voltageOffset;
    float voltage = (centered * VREF) / ADC_RESOLUTION;
    sum += voltage * voltage;
  }

  float Vrms_current = sqrt(sum / NUM_SAMPLES);
  float Irms = Vrms_current * CALIBRATION_FACTOR;

  // Threshold for noise
  if (Irms < 0.03) Irms = 0.0;

  float power_W = Vrms * Irms;
  float timeElapsed_h = 1.0 / 3600.0;
  totalEnergy_kWh += power_W * timeElapsed_h;

  String timestamp = getSriLankanTime();
  String path = "/EnergyConsumption";

  Serial.print("Voltage: ");
  Serial.print(Vrms, 2);
  Serial.print(" V, Current: ");
  Serial.print(Irms, 3);
  Serial.print(" A, Power: ");
  Serial.print(power_W, 2);
  Serial.print(" W, Energy: ");
  Serial.print(totalEnergy_kWh, 5);
  Serial.println(" kWh");

  if (!Firebase.pathExist(firebaseData, path)) {
    Firebase.set(firebaseData, path, "{}");
    Serial.println("Created EnergyConsumption node.");
  }

  String energyPath = path + "/" + timestamp;
  if (Firebase.setFloat(firebaseData, energyPath, totalEnergy_kWh)) {
    Serial.println("Sent to Firebase: " + timestamp + " - " + String(totalEnergy_kWh, 5) + " kWh");
  } else {
    Serial.println("Failed to send energy: " + firebaseData.errorReason());
  }

  if (Firebase.setFloat(firebaseData, "/TotalEnergy_kWh", totalEnergy_kWh)) {
    Serial.println("Updated TotalEnergy_Kwh: " + String(totalEnergy_kWh, 5) + " kWh");
  } else {
    Serial.println("Failed to update TotalEnergy_Kwh: " + firebaseData.errorReason());
  }
}

void checkRelayStatus() {
  if (Firebase.getString(firebaseData, "/RelayControl")) {
    String command = firebaseData.stringData();
    Serial.println("Relay Command: " + command);

    if (command == "OFF") {
      digitalWrite(RELAY_PIN, HIGH);
      Serial.println("Bulb is OFF");
    } else if (command == "ON") {
      digitalWrite(RELAY_PIN, LOW);
      Serial.println("Bulb is ON");
    }
  } else {
    Serial.println("Failed to read Firebase RelayControl data");
  }
}
