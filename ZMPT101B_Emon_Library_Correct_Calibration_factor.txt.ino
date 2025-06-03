// Include the EmonLib library
#include "EmonLib.h" // Energy Monitoring Library

EnergyMonitor emon1; // Create an instance for voltage monitoring

void setup() {
  Serial.begin(9600); // Start serial communication
  delay(1000); // Stabilize the setup

  Serial.println("Voltage Monitoring System");

  // Initialize the voltage sensor
  // Voltage: ADC pin, calibration constant, phase shift (set to 0 for voltage-only measurements)
  emon1.voltage(35, 71.5, 0); // Adjust calibration constant (e.g., 366) based on sensor calibration
}

void loop() {
  // Calculate voltage RMS using emon1.calcVI()
  emon1.calcVI(20, 2000); // Sampling 20 cycles with 2000 ms timeout
  double Vrms = emon1.Vrms; // Get RMS voltage

  // Print voltage value to the serial monitor
  Serial.print("Measured Voltage: ");
  Serial.print(Vrms, 2); // Print voltage with 2 decimal places
  Serial.println(" V");

  delay(500); // Wait for 1 second before the next reading
}
