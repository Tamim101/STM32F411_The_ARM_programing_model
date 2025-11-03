// #include <Arduino.h>

// // ---------- CONFIGURATION ----------
// const int ADC_PIN = 4;        // pick any ADC-capable pin (GPIO0–GPIO5 for ESP32-C3)
// const int LED_PIN = 2;        // optional LED pin
// int sensor_value = 0;

// // ---------- SETUP ----------
// void setup() {
//   Serial.begin(115200);        // UART communication
//   delay(200);                  // wait for Serial Monitor
//   Serial.println();
//   Serial.println("ESP32-C3 ADC + UART demo started");

//   pinMode(LED_PIN, OUTPUT);
//   digitalWrite(LED_PIN, LOW);
// }

// // ---------- MAIN LOOP ----------
// void loop() {
//   sensor_value = analogRead(ADC_PIN);     // range: 0–4095 for 12-bit ADC


//   Serial.print("Sensor value : ");
//   Serial.println(sensor_value);


//   // Example: fade LED based on potentiometer position
//   int brightness = map(sensor_value, 0, 4095, 0, 255);
//   analogWrite(LED_PIN, brightness);


//   delay(200);
// }
