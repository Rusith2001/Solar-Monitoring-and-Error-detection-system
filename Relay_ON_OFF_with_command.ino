#include <WiFi.h>
#include <FirebaseESP32.h>

#define WIFI_SSID "abcd"  
#define WIFI_PASSWORD "12345678"
#define FIREBASE_HOST "https://code-lab-app-461e1-default-rtdb.asia-southeast1.firebasedatabase.app/"
#define FIREBASE_AUTH "AIzaSyAqE7pe8ww06rVQay_D0kSzwSjSQxczPv4"

#define RELAY_PIN 26  // Pin connected to relay

FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;

void setup() {
    Serial.begin(115200);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    
    Serial.print("Connecting to WiFi");
    while (WiFi.status() != WL_CONNECTED) {
        Serial.print(".");
        delay(500);
    }
    Serial.println("\nConnected!");

    config.host = FIREBASE_HOST;
    config.signer.tokens.legacy_token = FIREBASE_AUTH;
    Firebase.begin(&config, &auth);
    Firebase.reconnectWiFi(true);

    pinMode(RELAY_PIN, OUTPUT);
    digitalWrite(RELAY_PIN, LOW); // Start with relay OFF

    Serial.println("Ready! Waiting for Firebase commands...");
}

void loop() {
    if (Firebase.getString(fbdo, "/RelayControl")) {  // Read value from Firebase
        String command = fbdo.stringData();
        Serial.println("Received: " + command);

        if (command == "OFF") {
            digitalWrite(RELAY_PIN, HIGH);
            Serial.println("Bulb is OFF");
        } 
        else if (command == "ON") {
            digitalWrite(RELAY_PIN, LOW);
            Serial.println("Bulb is ON");
        }
    } else {
        Serial.println("Failed to read Firebase data");
    }
    delay(1000); // Check every second
}
