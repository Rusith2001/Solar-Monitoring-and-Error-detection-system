#include "EmonLib.h" // Include the EmonLib library

EnergyMonitor emon1; // Create an instance of EnergyMonitor

void setup() {
  Serial.begin(9600);
  Serial.println("ACS712 Current Measurement with Dynamic Calibration");

  // Initialize the ACS712 current sensor with default calibration factor 1.5
  emon1.current(35, 1.5); // ADC pin 35, initial calibration factor
}

void loop() {
  double Irms = emon1.calcIrms(1480); // Initial current measurement with 1.5

  // If the initial measurement is below 0.1A, print 0A
  if (Irms < 0.1) {
    Serial.println("Measured Current: 0 A");
  } 
  else {
    // Check if recalibration is needed
    if (Irms >= 0.1 && Irms <= 0.107) {
      emon1.current(35, 2.1); // Change calibration factor to 2.1
      Irms = emon1.calcIrms(1480);
    } 
    else if (Irms >= 0.186 && Irms <= 0.197) {
      emon1.current(35, 2.0); // Change calibration factor to 2.0
      Irms = emon1.calcIrms(1480);
    } 
    else if (Irms >= 0.28 && Irms <= 0.31) {
      emon1.current(35, 1.6); // Change calibration factor to 1.6
      Irms = emon1.calcIrms(1480);
    }

    // Print the recalculated current value
    Serial.print("Measured Current: ");
    Serial.print(Irms, 3); // Print current with 3 decimal places
    Serial.println(" A");
  }

  delay(5000); // Wait 1 second before the next reading
}
