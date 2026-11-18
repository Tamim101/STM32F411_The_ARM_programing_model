// #include <Arduino.h>
// #include <SPI.h>
// // MicroQuad v2 — STM32 Black Pill: IMU + battery -> ESP32, motor + LED commands from the phone.
// // UART2: PA2 TX -> XIAO D7,  PA3 RX <- XIAO D8  (115200), common GND
// // GY-91: SCL PB13, SDA PB15, SDO PB14, NCS PB12, CSB PB0.  Battery 10k/10k divider on PA1.
// // Motors: M1 PB6, M2 PB7, M3 PB8, M4 PB9 (AO3400A, 16 kHz PWM)
// // Commands: "HB" heartbeat (missing 1 s -> motors STOP), "M <pct>" motor speed, "LED" toggle
// // IMU: tries SPI first. If SPI does not answer but I2C does, runs the IMU over I2C
// //      and tells the phone which wire is bad.

// #if defined(STM32_CORE_VERSION_MAJOR) && (STM32_CORE_VERSION_MAJOR >= 3)
// Uart link(PA_3, PA_2);            // RX, TX
// #else
// HardwareSerial link(PA3, PA2);
// #endif
// SPIClass spiImu(PB15, PB14, PB13); // MOSI, MISO, SCK
// const SPISettings IMU_SPI(1000000, MSBFIRST, SPI_MODE3);
// #define IMU_CS  PB12
// #define BARO_CS PB0
// #define P_SCK   PB13   // = I2C SCL
// #define P_MOSI  PB15   // = I2C SDA
// #define P_MISO  PB14   // = SDO (I2C address pin AD0)
// #define LED     PC13
// #define VBAT    PA1
// const uint32_t MOTOR_PIN[4] = {PB6, PB7, PB8, PB9};
// const uint32_t MOTOR_MAX_MS = 10000;
// const uint32_t LINK_TIMEOUT_MS = 1000;

// enum { BUS_NONE, BUS_SPI, BUS_I2C } imuBus = BUS_NONE;
// uint8_t imuId = 0, i2cAddr = 0x68;
// String imuDiagMsg = "";
// bool ledOn = false;
// int motorTarget = 0;
// float motorNow = 0;
// uint32_t motorStartMs = 0, lastCmdMs = 0;

// // ================= hardware SPI =================
// uint8_t spiRead(uint8_t reg) {
//   spiImu.beginTransaction(IMU_SPI); digitalWrite(IMU_CS, LOW);
//   spiImu.transfer(reg | 0x80); uint8_t v = spiImu.transfer(0);
//   digitalWrite(IMU_CS, HIGH); spiImu.endTransaction();
//   return v;
// }
// void spiWrite(uint8_t reg, uint8_t v) {
//   spiImu.beginTransaction(IMU_SPI); digitalWrite(IMU_CS, LOW);
//   spiImu.transfer(reg & 0x7F); spiImu.transfer(v);
//   digitalWrite(IMU_CS, HIGH); spiImu.endTransaction(); delay(1);
// }
// bool idOk(uint8_t id) { return id == 0x71 || id == 0x73 || id == 0x70; }

// // ================= software (bit-bang) SPI, slow, mode 3 =================
// uint8_t bbSpiRead(uint8_t reg) {
//   pinMode(P_SCK, OUTPUT); pinMode(P_MOSI, OUTPUT); pinMode(P_MISO, INPUT);
//   digitalWrite(P_SCK, HIGH);
//   digitalWrite(IMU_CS, LOW); delayMicroseconds(5);
//   uint8_t out = reg | 0x80, in = 0;
//   for (int i = 7; i >= 0; i--) {
//     digitalWrite(P_SCK, LOW); digitalWrite(P_MOSI, (out >> i) & 1); delayMicroseconds(5);
//     digitalWrite(P_SCK, HIGH); delayMicroseconds(5);
//   }
//   for (int i = 7; i >= 0; i--) {
//     digitalWrite(P_SCK, LOW); delayMicroseconds(5);
//     digitalWrite(P_SCK, HIGH); in = (in << 1) | digitalRead(P_MISO); delayMicroseconds(5);
//   }
//   digitalWrite(IMU_CS, HIGH);
//   return in;
// }

// // ================= software I2C on PB13 (SCL) / PB15 (SDA) =================
// static void sdaH() { pinMode(P_MOSI, INPUT_PULLUP); }
// static void sdaL() { pinMode(P_MOSI, OUTPUT); digitalWrite(P_MOSI, LOW); }
// static void sclH() { pinMode(P_SCK, INPUT_PULLUP); }
// static void sclL() { pinMode(P_SCK, OUTPUT); digitalWrite(P_SCK, LOW); }
// static void dly() { delayMicroseconds(4); }
// static void i2cStart() { sdaH(); sclH(); dly(); sdaL(); dly(); sclL(); dly(); }
// static void i2cStop() { sdaL(); dly(); sclH(); dly(); sdaH(); dly(); }
// static bool i2cWrite(uint8_t b) {
//   for (int i = 7; i >= 0; i--) { if ((b >> i) & 1) sdaH(); else sdaL(); dly(); sclH(); dly(); sclL(); }
//   sdaH(); dly(); sclH(); dly(); bool ack = !digitalRead(P_MOSI); sclL(); dly();
//   return ack;
// }
// static uint8_t i2cRead(bool ack) {
//   uint8_t b = 0; sdaH();
//   for (int i = 0; i < 8; i++) { dly(); sclH(); dly(); b = (b << 1) | digitalRead(P_MOSI); sclL(); }
//   if (ack) sdaL(); else sdaH();
//   dly(); sclH(); dly(); sclL(); sdaH();
//   return b;
// }
// bool i2cReadRegs(uint8_t addr, uint8_t reg, uint8_t *buf, int n) {
//   i2cStart();
//   if (!i2cWrite(addr << 1)) { i2cStop(); return false; }
//   i2cWrite(reg);
//   i2cStart();
//   if (!i2cWrite((addr << 1) | 1)) { i2cStop(); return false; }
//   for (int i = 0; i < n; i++) buf[i] = i2cRead(i < n - 1);
//   i2cStop();
//   return true;
// }
// bool i2cWriteReg(uint8_t addr, uint8_t reg, uint8_t v) {
//   i2cStart();
//   bool ok = i2cWrite(addr << 1) && i2cWrite(reg) && i2cWrite(v);
//   i2cStop(); delay(1);
//   return ok;
// }
// bool i2cAnswers(uint8_t addr) { uint8_t v; return i2cReadRegs(addr, 0x75, &v, 1); }

// // ================= find the IMU and test each wire =================
// void imuSetup() {
//   spiImu.begin();
//   uint8_t hwId = spiRead(0x75);          // 1) hardware SPI
//   spiImu.end();
//   uint8_t bbId = bbSpiRead(0x75);        // 2) slow software SPI

//   // 3) I2C tests (NCS + CSB high = chips in I2C mode)
//   digitalWrite(IMU_CS, HIGH);
//   sclH(); sdaH(); delay(2);
//   // SDO test: the MPU's I2C address follows the SDO pin (LOW -> 0x68, HIGH -> 0x69)
//   pinMode(P_MISO, OUTPUT);
//   digitalWrite(P_MISO, LOW);  delay(2); bool at68 = i2cAnswers(0x68);
//   digitalWrite(P_MISO, HIGH); delay(2); bool at69 = i2cAnswers(0x69);
//   digitalWrite(P_MISO, LOW);  delay(2); bool lowGives69 = i2cAnswers(0x69);
//   digitalWrite(P_MISO, HIGH); delay(2); bool highGives68 = i2cAnswers(0x68);
//   pinMode(P_MISO, INPUT);
//   bool i2cFound = at68 || at69 || lowGives69 || highGives68;
//   bool sdoOk = at68 && at69 && !lowGives69 && !highGives68;
//   // NCS test: with NCS LOW the chip leaves I2C mode -> it must STOP answering
//   delay(2);
//   i2cAddr = i2cAnswers(0x68) ? 0x68 : 0x69;
//   digitalWrite(IMU_CS, LOW); delay(2);
//   bool answersWithCsLow = i2cAnswers(i2cAddr);
//   digitalWrite(IMU_CS, HIGH); delay(2);
//   bool ncsOk = i2cFound && !answersWithCsLow;

//   String w = String("SDO ") + (i2cFound ? (sdoOk ? "OK" : "BAD") : "?") + ", NCS " + (i2cFound ? (ncsOk ? "OK" : "BAD") : "?");

//   if (idOk(hwId) || idOk(bbId)) {
//     spiImu.begin();
//     spiWrite(0x6B, 0x80); delay(100);
//     spiWrite(0x6A, 0x10); spiWrite(0x6B, 0x01);
//     spiWrite(0x1A, 0x03); spiWrite(0x1B, 0x18); spiWrite(0x1C, 0x18);
//     imuId = spiRead(0x75);
//     imuBus = BUS_SPI;
//     imuDiagMsg = "IMU OK over SPI";
//   } else if (i2cFound) {
//     uint8_t id = 0;
//     i2cReadRegs(i2cAddr, 0x75, &id, 1);
//     i2cWriteReg(i2cAddr, 0x6B, 0x80); delay(100);
//     i2cWriteReg(i2cAddr, 0x6B, 0x01);
//     i2cWriteReg(i2cAddr, 0x1A, 0x03); i2cWriteReg(i2cAddr, 0x1B, 0x18); i2cWriteReg(i2cAddr, 0x1C, 0x18);
//     imuId = id; imuBus = BUS_I2C;
//     imuDiagMsg = "SPI no reply (0x" + String(hwId, HEX) + "). " + w + ". Using I2C for now";
//   } else {
//     imuBus = BUS_NONE; imuId = hwId;
//     imuDiagMsg = "No IMU on SPI or I2C: check GY-91 power (3V3, GND), SCL PB13, SDA PB15";
//   }
// }

// bool imuOk() { return imuBus != BUS_NONE && idOk(imuId); }

// bool imuReadRaw(uint8_t *b) {
//   if (imuBus == BUS_SPI) {
//     spiImu.beginTransaction(IMU_SPI); digitalWrite(IMU_CS, LOW);
//     spiImu.transfer(0x3B | 0x80);
//     for (int i = 0; i < 14; i++) b[i] = spiImu.transfer(0);
//     digitalWrite(IMU_CS, HIGH); spiImu.endTransaction();
//     return true;
//   }
//   if (imuBus == BUS_I2C) return i2cReadRegs(i2cAddr, 0x3B, b, 14);
//   return false;
// }

// float readVbat() {
//   uint32_t s = 0;
//   for (int i = 0; i < 16; i++) s += analogRead(VBAT);
//   return (s / 16.0f) / 4095.0f * 3.3f * 2.0f;
// }

// // ================= motors =================
// void motorsWrite(float pct) {
//   for (int i = 0; i < 4; i++) analogWrite(MOTOR_PIN[i], (int)(pct * 4095 / 100));
// }
// void motorsStop() { motorTarget = 0; }

// void handleCommand(const String &cmd) {
//   lastCmdMs = millis();
//   if (cmd == "HB") return;
//   if (cmd == "LED") {
//     ledOn = !ledOn;
//     link.print("ACK LED "); link.println(ledOn ? "ON" : "OFF");
//   } else if (cmd.startsWith("M ")) {
//     int pct = constrain(cmd.substring(2).toInt(), 0, 100);
//     if (pct == 0) { motorsStop(); link.println("ACK MOTOR OFF"); }
//     else {
//       if (motorTarget == 0) motorStartMs = millis();
//       motorTarget = pct;
//       link.print("ACK MOTOR ON "); link.println(pct);
//     }
//   }
// }

// void setup() {
//   for (int i = 0; i < 4; i++) { pinMode(MOTOR_PIN[i], OUTPUT); digitalWrite(MOTOR_PIN[i], LOW); }
//   analogWriteResolution(12);
//   analogWriteFrequency(16000);
//   motorsWrite(0);
//   pinMode(LED, OUTPUT); digitalWrite(LED, HIGH);
//   pinMode(BARO_CS, OUTPUT); digitalWrite(BARO_CS, HIGH);
//   pinMode(IMU_CS, OUTPUT); digitalWrite(IMU_CS, HIGH);
//   analogReadResolution(12);
//   link.begin(115200);
//   Serial.begin(115200);
//   delay(150);               // let the GY-91 power up
//   imuSetup();
// }

// void loop() {
//   static uint32_t lastSend = 0, lastRamp = 0, lastDiag = 0, count = 0;
//   static String line;

//   while (link.available()) {
//     char c = link.read();
//     if (c == '\n') { line.trim(); if (line.length()) handleCommand(line); line = ""; }
//     else if (c != '\r' && line.length() < 40) line += c;
//   }

//   if (motorTarget > 0 && millis() - lastCmdMs > LINK_TIMEOUT_MS) motorsStop();
//   if (motorTarget > 0 && millis() - motorStartMs > MOTOR_MAX_MS) { motorsStop(); link.println("ACK MOTOR OFF"); }

//   if (millis() - lastRamp >= 20) {
//     lastRamp = millis();
//     if (motorNow < motorTarget) motorNow = min((float)motorTarget, motorNow + 2);
//     else if (motorNow > motorTarget) motorNow = max((float)motorTarget, motorNow - 5);
//     motorsWrite(motorNow);
//   }

//   if (ledOn || motorNow > 0) digitalWrite(LED, LOW);
//   else digitalWrite(LED, (millis() % 1000) < 80 ? LOW : HIGH);

//   if (millis() - lastDiag >= 3000 && motorNow == 0) {
//     lastDiag = millis();
//     link.print("ACK IMU: "); link.println(imuDiagMsg);
//   }

//   if (millis() - lastSend >= 100) {
//     lastSend = millis();
//     float roll = 0, pitch = 0, gx = 0, gy = 0, gz = 0;
//     uint8_t b[14];
//     if (imuOk() && imuReadRaw(b)) {
//       auto s16 = [&](int i) { return (int16_t)((b[i] << 8) | b[i + 1]); };
//       float ax = s16(0) / 2048.0f, ay = s16(2) / 2048.0f, az = s16(4) / 2048.0f;
//       gx = s16(8) / 16.4f; gy = s16(10) / 16.4f; gz = s16(12) / 16.4f;
//       roll = atan2f(ay, az) * 57.2958f;
//       pitch = atan2f(-ax, sqrtf(ay * ay + az * az)) * 57.2958f;
//     }
//     float v = readVbat();
//     link.print("T,"); link.print(++count); link.print(',');
//     link.print(imuId); link.print(',');
//     link.print(roll, 1); link.print(','); link.print(pitch, 1); link.print(',');
//     link.print(gx, 1); link.print(','); link.print(gy, 1); link.print(','); link.print(gz, 1); link.print(',');
//     link.print(v, 2); link.print(','); link.print(ledOn ? 1 : 0); link.print(',');
//     link.println((int)motorNow);
//   }
// }