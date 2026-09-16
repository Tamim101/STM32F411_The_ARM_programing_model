
#include <Arduino.h>

// ═══════════════════════════════════════════
//  PIN CONFIG  (XIAO ESP32-C3 pin names)
//  D0=GPIO2  D1=GPIO3  D2=GPIO4  D3=GPIO5
//  D4=GPIO6  D5=GPIO7  D6=GPIO21 D7=GPIO20
// ═══════════════════════════════════════════
#define PIN_M1  5    // D3  — testing 1 motor first
#define PIN_M2  6    // D4
#define PIN_M3  7    // D5
#define PIN_M4  21   // D6

// ═══════════════════════════════════════════
//  SAFETY
// ═══════════════════════════════════════════
#define TIMEOUT_MS  1000
unsigned long lastReceived = 0;
bool armed = false;
int  sendCounter = 0;

// ═══════════════════════════════════════════
//  STATE
// ═══════════════════════════════════════════
float T=0, R=0, P=0, Y=0;
float m1, m2, m3, m4;

// ───────────────────────────────────────────
//  sqrt linearization:  T = k*w²  →  w = sqrt(T)
//  maps 0..1  →  PWM 0..255
// ───────────────────────────────────────────
int toPWM(float val) {
  val = constrain(val, 0.0f, 1.0f);
  return (int)(sqrtf(val) * 255.0f);
}

// ───────────────────────────────────────────
//  MOTOR MIXER  (same math as Flix control.ino)
// ───────────────────────────────────────────
void runMixer() {
  m1 = constrain(T - R + P + Y, 0.0f, 1.0f);  // Front-Left
  m2 = constrain(T + R + P - Y, 0.0f, 1.0f);  // Front-Right
  m3 = constrain(T - R - P - Y, 0.0f, 1.0f);  // Back-Left
  m4 = constrain(T + R - P + Y, 0.0f, 1.0f);  // Back-Right

  // analogWrite works on ESP32-C3 — no ledcSetup needed!
  analogWrite(PIN_M1, toPWM(m1));
  analogWrite(PIN_M2, toPWM(m2));
  analogWrite(PIN_M3, toPWM(m3));
  analogWrite(PIN_M4, toPWM(m4));
}

// ───────────────────────────────────────────
//  SETUP
// ───────────────────────────────────────────
void setup() {
  Serial.begin(115200);
  delay(1000);   // give USB serial time to connect on C3

  // Set pins as output and start at zero
  pinMode(PIN_M1, OUTPUT);  analogWrite(PIN_M1, 0);
  pinMode(PIN_M2, OUTPUT);  analogWrite(PIN_M2, 0);
  pinMode(PIN_M3, OUTPUT);  analogWrite(PIN_M3, 0);
  pinMode(PIN_M4, OUTPUT);  analogWrite(PIN_M4, 0);

  // FIX: start timer now — not at 0
  lastReceived = millis();

  Serial.println("=================================");
  Serial.println("  MOTOR MIXER — XIAO ESP32-C3");
  Serial.println("  READY: send T,R,P,Y");
  Serial.println("  Example: 0.500,0.000,0.000,0.000");
  Serial.println("=================================");
}

// ───────────────────────────────────────────
//  LOOP
// ───────────────────────────────────────────
void loop() {

  // ── Read serial ─────────────────────────
  if (Serial.available() > 0) {
    String line = Serial.readStringUntil('\n');
    line.trim();

    if (line.length() > 0) {
      // Parse T,R,P,Y
      int c1 = line.indexOf(',');
      int c2 = line.indexOf(',', c1+1);
      int c3 = line.indexOf(',', c2+1);

      if (c1>0 && c2>0 && c3>0) {
        float newT = line.substring(0,    c1).toFloat();
        float newR = line.substring(c1+1, c2).toFloat();
        float newP = line.substring(c2+1, c3).toFloat();
        float newY = line.substring(c3+1    ).toFloat();

        // Validate — reject garbage
        if (newT >= 0.0f && newT <= 1.0f) {
          T = newT;
          R = constrain(newR, -1.0f, 1.0f);
          P = constrain(newP, -1.0f, 1.0f);
          Y = constrain(newY, -1.0f, 1.0f);
          lastReceived = millis();
          armed = true;
        }
      }
    }
  }

  // ── Safety timeout ──────────────────────
  if (armed && millis() - lastReceived > TIMEOUT_MS) {
    analogWrite(PIN_M1, 0);
    analogWrite(PIN_M2, 0);
    analogWrite(PIN_M3, 0);
    analogWrite(PIN_M4, 0);
    armed = false;
    T=0; R=0; P=0; Y=0;
    Serial.println("TIMEOUT: motors stopped");
    return;
  }

  if (!armed) return;

  // ── Run mixer ───────────────────────────
  runMixer();

  // ── Send back every 25 loops (~2Hz) ─────
  sendCounter++;
  if (sendCounter >= 25) {
    sendCounter = 0;
    Serial.printf("T=%.2f R=%.2f P=%.2f Y=%.2f | M1=%d M2=%d M3=%d M4=%d\n",
      T, R, P, Y,
      toPWM(m1), toPWM(m2), toPWM(m3), toPWM(m4));
  }

  delay(20);
}