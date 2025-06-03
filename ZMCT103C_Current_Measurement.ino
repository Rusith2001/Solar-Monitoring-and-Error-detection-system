#define SENSOR_PIN 34  // GPIO for analog input from ZMCT103C
#define SAMPLES 1000   // Number of samples to average

float calibrationFactor = 0.395;  // Adjust based on your sensor & burden resistor

void setup() {
  Serial.begin(115200);
  analogReadResolution(12); // ESP32 has 12-bit ADC (0-4095)
  delay(1000);
}

void loop() {
  float sum = 0;
  for (int i = 0; i < SAMPLES; i++) {
    int raw = analogRead(SENSOR_PIN);
    float voltage = ((float)raw / 4095.0) * 3.3;  // Convert ADC to voltage
    float offset = 3.3 / 2;  // Assuming midpoint bias
    float current = (voltage - offset);  // Remove DC bias
    sum += current * current;
    delayMicroseconds(100);  // ~10kHz sample rate
  }

  float rmsVoltage = sqrt(sum / SAMPLES);
  float rmsCurrent = rmsVoltage * calibrationFactor;  // Convert to Amps
  
  Serial.print("RMS Current: ");
  Serial.print(rmsCurrent, 3);
  Serial.println(" A");

  delay(1000);
}
