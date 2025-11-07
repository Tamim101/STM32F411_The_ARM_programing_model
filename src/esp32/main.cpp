#include <Arduino.h>

// const int LED_PIN = 2;   // adjust if your board uses another LED GPIO

// void setup() {
//   Serial.begin(115200);
//   pinMode(LED_PIN, OUTPUT);
//   Serial.println("ESP32-C3 Arduino demo started");
// }

// void loop() {
//   if (Serial.available()) {
//     char key = Serial.read();
//     if (key == '1') {
//       digitalWrite(LED_PIN, HIGH);
//       Serial.println("LED ON");
//     } else {
//       digitalWrite(LED_PIN, LOW);
//       Serial.println("LED OFF");
//     }
//   }
//   delay(10);
// }
// ESP32-C3 USB CDC <-> UART0 bridge
// UART0 pins on C3: RX=GPIO20, TX=GPIO21
void setup() {
  Serial.begin(115200);                     // USB CDC
  Serial1.begin(115200, SERIAL_8N1, 20, 21);// UART0 to STM32
}
void loop() {
  while (Serial.available())  Serial1.write(Serial.read());
  while (Serial1.available()) Serial.write(Serial1.read());
}
