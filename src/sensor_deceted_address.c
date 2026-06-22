// /**
//  * @file imu_read_sensor.c
//  * @brief STM32F103 MPU6050 Diagnostic Version
//  *
//  * This version helps you find WHERE the problem is.
//  * The onboard LED (PC13) gives visual feedback at every step.
//  *
//  * LED BEHAVIOR — WATCH IT CAREFULLY:
//  *
//  *   1. SOLID ON for 2 seconds at boot
//  *      → If you see this, the board is running. If NOT, board is not flashed.
//  *
//  *   2. FAST blink (5x quick) before UART test
//  *      → Just a marker
//  *
//  *   3. Sends "HELLO" to serial 5 times, slow blink between each
//  *      → If you see "HELLO" in serial monitor: UART works! ✓
//  *      → If you DON'T see anything: UART problem (wiring/baud rate)
//  *
//  *   4. Tries to detect MPU6050:
//  *      → 3 SLOW blinks = MPU6050 NOT FOUND (wiring issue)
//  *      → 1 LONG blink = MPU6050 FOUND ✓
//  *
//  *   5. After detection, prints WHO_AM_I value and sensor data
//  *
//  *   6. LED blinks once per second during normal operation
//  *
//  * Hardware:
//  *   MPU6050 SCL → PB6  (with 4.7kΩ pull-up to 3.3V) — IMPORTANT!
//  *   MPU6050 SDA → PB7  (with 4.7kΩ pull-up to 3.3V) — IMPORTANT!
//  *   MPU6050 VCC → 3.3V (NOT 5V)
//  *   MPU6050 GND → GND
//  *
//  *   STM32 PA9  (TX) → USB Adapter RX
//  *   STM32 PA10 (RX) → USB Adapter TX
//  *   STM32 GND       → USB Adapter GND  ← MUST be connected!
//  *
//  * platformio.ini:
//  *   [env:genericSTM32F103C8]
//  *   platform = ststm32
//  *   board = genericSTM32F103C8
//  *   framework = cmsis
//  *   upload_protocol = stlink
//  *   build_flags = -Os -u _printf_float
//  */

// #include <stdint.h>
// #include <math.h>
// #include <stdio.h>

// // ============================================================================
// // REGISTER DEFINITIONS
// // ============================================================================

// #define RCC_BASE        0x40021000UL
// #define GPIOA_BASE      0x40010800UL
// #define GPIOB_BASE      0x40010C00UL
// #define GPIOC_BASE      0x40011000UL
// #define I2C1_BASE       0x40005400UL
// #define USART1_BASE     0x40013800UL
// #define FLASH_REG_BASE  0x40022000UL
// #define STK_BASE        0xE000E010UL

// #define RCC_CR          (*(volatile uint32_t *)(RCC_BASE + 0x00))
// #define RCC_CFGR        (*(volatile uint32_t *)(RCC_BASE + 0x04))
// #define RCC_APB2ENR     (*(volatile uint32_t *)(RCC_BASE + 0x18))
// #define RCC_APB1ENR     (*(volatile uint32_t *)(RCC_BASE + 0x1C))

// #define FLASH_ACR       (*(volatile uint32_t *)(FLASH_REG_BASE + 0x00))

// #define GPIOA_CRH       (*(volatile uint32_t *)(GPIOA_BASE + 0x04))
// #define GPIOB_CRL       (*(volatile uint32_t *)(GPIOB_BASE + 0x00))
// #define GPIOC_CRH       (*(volatile uint32_t *)(GPIOC_BASE + 0x04))
// #define GPIOC_ODR       (*(volatile uint32_t *)(GPIOC_BASE + 0x0C))
// #define GPIOC_BSRR      (*(volatile uint32_t *)(GPIOC_BASE + 0x10))

// #define I2C1_CR1        (*(volatile uint32_t *)(I2C1_BASE + 0x00))
// #define I2C1_CR2        (*(volatile uint32_t *)(I2C1_BASE + 0x04))
// #define I2C1_DR         (*(volatile uint32_t *)(I2C1_BASE + 0x10))
// #define I2C1_SR1        (*(volatile uint32_t *)(I2C1_BASE + 0x14))
// #define I2C1_SR2        (*(volatile uint32_t *)(I2C1_BASE + 0x18))
// #define I2C1_CCR        (*(volatile uint32_t *)(I2C1_BASE + 0x1C))
// #define I2C1_TRISE      (*(volatile uint32_t *)(I2C1_BASE + 0x20))

// #define USART1_SR       (*(volatile uint32_t *)(USART1_BASE + 0x00))
// #define USART1_DR       (*(volatile uint32_t *)(USART1_BASE + 0x04))
// #define USART1_BRR      (*(volatile uint32_t *)(USART1_BASE + 0x08))
// #define USART1_CR1      (*(volatile uint32_t *)(USART1_BASE + 0x0C))
// #define USART1_CR2      (*(volatile uint32_t *)(USART1_BASE + 0x10))
// #define USART1_CR3      (*(volatile uint32_t *)(USART1_BASE + 0x14))

// #define STK_CTRL        (*(volatile uint32_t *)(STK_BASE + 0x00))
// #define STK_LOAD        (*(volatile uint32_t *)(STK_BASE + 0x04))
// #define STK_VAL         (*(volatile uint32_t *)(STK_BASE + 0x08))

// // LED on PC13 — note: PC13 is ACTIVE LOW on BluePill (LOW = ON)
// #define LED_ON()        (GPIOC_BSRR = (1 << 29))   // BR13: set bit = OFF, but PC13 inverted so... 
// #define LED_OFF()       (GPIOC_BSRR = (1 << 13))
// #define LED_TOGGLE()    (GPIOC_ODR ^= (1 << 13))

// // MPU6050
// #define MPU6050_ADDR            0x68
// #define MPU6050_WHO_AM_I        0x75
// #define MPU6050_PWR_MGMT_1      0x6B
// #define MPU6050_SMPLRT_DIV      0x19
// #define MPU6050_CONFIG          0x1A
// #define MPU6050_GYRO_CONFIG     0x1B
// #define MPU6050_ACCEL_CONFIG    0x1C
// #define MPU6050_INT_ENABLE      0x38
// #define MPU6050_ACCEL_XOUT_H    0x3B

// #define PI 3.14159265359f
// #define ACCEL_SCALE (9.81f / 16384.0f)
// #define GYRO_SCALE  (1.0f / 131.072f)
// #define GYRO_WEIGHT 0.98f
// #define DT          0.01f

// // ============================================================================
// // GLOBALS
// // ============================================================================

// volatile uint32_t tick_ms = 0;

// static int16_t accel_offset_x = 0, accel_offset_y = 0, accel_offset_z = 0;
// static int16_t gyro_offset_x = 0,  gyro_offset_y = 0,  gyro_offset_z = 0;

// static float roll_filtered  = 0.0f;
// static float pitch_filtered = 0.0f;
// static float yaw_filtered   = 0.0f;

// // ============================================================================
// // SYSTICK
// // ============================================================================

// void SysTick_Init(void) {
//     STK_LOAD = 72000 - 1;
//     STK_VAL  = 0;
//     STK_CTRL = 0x07;
// }

// void SysTick_Handler(void) {
//     tick_ms++;
// }

// void Delay_ms(uint32_t ms) {
//     uint32_t start = tick_ms;
//     while ((tick_ms - start) < ms);
// }

// // ============================================================================
// // LED HELPERS — Visual feedback even if serial doesn't work
// // ============================================================================

// void LED_BlinkSlow(int count) {
//     for (int i = 0; i < count; i++) {
//         GPIOC_ODR &= ~(1 << 13);  // LED ON (active low)
//         Delay_ms(400);
//         GPIOC_ODR |= (1 << 13);   // LED OFF
//         Delay_ms(400);
//     }
// }

// void LED_BlinkFast(int count) {
//     for (int i = 0; i < count; i++) {
//         GPIOC_ODR &= ~(1 << 13);
//         Delay_ms(80);
//         GPIOC_ODR |= (1 << 13);
//         Delay_ms(80);
//     }
// }

// void LED_BlinkLong(void) {
//     GPIOC_ODR &= ~(1 << 13);   // ON
//     Delay_ms(1500);
//     GPIOC_ODR |= (1 << 13);    // OFF
//     Delay_ms(500);
// }

// // ============================================================================
// // CLOCK & GPIO
// // ============================================================================

// void SystemClock_Init(void) {
//     RCC_CR |= (1 << 16);
//     while (!(RCC_CR & (1 << 17)));

//     FLASH_ACR = (FLASH_ACR & ~0x07) | 0x02;

//     RCC_CFGR &= ~((0x0F << 18) | (1 << 17) | (1 << 16));
//     RCC_CFGR |= (0x07 << 18) | (1 << 16);
//     RCC_CFGR |= (0x04 << 8);

//     RCC_CR |= (1 << 24);
//     while (!(RCC_CR & (1 << 25)));

//     RCC_CFGR = (RCC_CFGR & ~0x03) | 0x02;
//     while ((RCC_CFGR & 0x0C) != 0x08);
// }

// void GPIO_Init(void) {
//     RCC_APB2ENR |= (1 << 0) | (1 << 2) | (1 << 3) | (1 << 4);

//     // PA9 TX (AF PP 50MHz = 0xB)
//     GPIOA_CRH = (GPIOA_CRH & 0xFFFFFF0F) | 0x000000B0;
//     // PA10 RX (Floating Input = 0x4)
//     GPIOA_CRH = (GPIOA_CRH & 0xFFFFF0FF) | 0x00000400;

//     // PB6 SCL (AF OD 50MHz = 0xF)
//     GPIOB_CRL = (GPIOB_CRL & 0xF0FFFFFF) | 0x0F000000;
//     // PB7 SDA (AF OD 50MHz = 0xF)
//     GPIOB_CRL = (GPIOB_CRL & 0x0FFFFFFF) | 0xF0000000;

//     // PC13 LED (Output PP 2MHz = 0x2)
//     GPIOC_CRH = (GPIOC_CRH & 0xFF0FFFFF) | 0x00200000;

//     // LED off initially (PC13 is active low — set bit = OFF)
//     GPIOC_ODR |= (1 << 13);
// }

// // ============================================================================
// // USART1 @ 9600
// // ============================================================================

// void USART1_Init(void) {
//     RCC_APB2ENR |= (1 << 14);
//     // BRR for 9600 @ 72MHz: 72000000 / 9600 = 7500
//     USART1_BRR = 7500;
//     USART1_CR2 = 0;
//     USART1_CR3 = 0;
//     USART1_CR1 = (1 << 13) | (1 << 3) | (1 << 2);
// }

// void USART1_SendChar(char c) {
//     while (!(USART1_SR & (1 << 7)));
//     USART1_DR = (uint8_t)c;
// }

// void USART1_SendString(const char *str) {
//     while (*str) USART1_SendChar(*str++);
// }

// void USART1_SendNum(int num) {
//     char buf[16];
//     snprintf(buf, sizeof(buf), "%d", num);
//     USART1_SendString(buf);
// }

// void USART1_SendHex(uint8_t val) {
//     char buf[8];
//     snprintf(buf, sizeof(buf), "0x%02X", val);
//     USART1_SendString(buf);
// }

// // ============================================================================
// // I2C1 @ 100 kHz (slower = more reliable for debugging)
// // ============================================================================

// void I2C1_Init(void) {
//     RCC_APB1ENR |= (1 << 21);
//     I2C1_CR1 = 0;

//     // Reset I2C
//     I2C1_CR1 |= (1 << 15);
//     Delay_ms(2);
//     I2C1_CR1 &= ~(1 << 15);

//     I2C1_CR2   = 36;        // 36 MHz APB1
//     I2C1_CCR   = 180;       // 100 kHz: 36M / (2 × 100k) = 180  (slower = safer)
//     I2C1_TRISE = 37;        // 36 + 1
//     I2C1_CR1   |= (1 << 0); // Enable
// }

// static uint8_t I2C1_Start(void) {
//     I2C1_CR1 |= (1 << 10);  // ACK enable
//     I2C1_CR1 |= (1 << 8);   // START
//     uint32_t t = 50000;
//     while (!(I2C1_SR1 & 0x0001) && --t);
//     return t > 0;
// }

// static void I2C1_Stop(void) {
//     I2C1_CR1 |= (1 << 9);
// }

// static uint8_t I2C1_SendAddr(uint8_t addr, uint8_t rw) {
//     I2C1_DR = (addr << 1) | rw;
//     uint32_t t = 50000;
//     while (!(I2C1_SR1 & 0x0002) && --t) {
//         if (I2C1_SR1 & 0x0400) { I2C1_SR1 &= ~0x0400; return 0; }
//     }
//     if (t == 0) return 0;
//     (void)I2C1_SR2;
//     return 1;
// }

// uint8_t I2C1_Write(uint8_t addr, uint8_t *data, uint16_t len) {
//     if (!I2C1_Start()) return 0;
//     if (!I2C1_SendAddr(addr, 0)) { I2C1_Stop(); return 0; }
//     for (uint16_t i = 0; i < len; i++) {
//         I2C1_DR = data[i];
//         uint32_t t = 50000;
//         while (!(I2C1_SR1 & 0x0004) && --t);
//         if (t == 0) { I2C1_Stop(); return 0; }
//     }
//     uint32_t t = 50000;
//     while (!(I2C1_SR1 & 0x0004) && --t);
//     I2C1_Stop();
//     return 1;
// }

// uint8_t I2C1_Read(uint8_t addr, uint8_t *data, uint16_t len) {
//     I2C1_CR1 |= (1 << 10);
//     if (!I2C1_Start()) return 0;
//     if (!I2C1_SendAddr(addr, 1)) { I2C1_Stop(); return 0; }
//     for (uint16_t i = 0; i < len; i++) {
//         if (i == (len - 1)) I2C1_CR1 &= ~(1 << 10);
//         uint32_t t = 50000;
//         while (!(I2C1_SR1 & 0x0040) && --t);
//         if (t == 0) { I2C1_Stop(); return 0; }
//         data[i] = (uint8_t)I2C1_DR;
//     }
//     I2C1_Stop();
//     return 1;
// }

// // ============================================================================
// // I2C SCANNER — Find any I2C device on the bus
// // ============================================================================

// void I2C_Scan(void) {
//     USART1_SendString("\r\n--- I2C Bus Scan ---\r\n");
//     USART1_SendString("Scanning addresses 0x08 to 0x77...\r\n");

//     int found = 0;
//     for (uint8_t addr = 0x08; addr < 0x78; addr++) {
//         // Try to start communication with this address
//         if (I2C1_Start()) {
//             uint8_t ok = I2C1_SendAddr(addr, 0);
//             I2C1_Stop();
//             if (ok) {
//                 USART1_SendString("  FOUND device at address ");
//                 USART1_SendHex(addr);
//                 USART1_SendString("\r\n");
//                 found++;
//             }
//         }
//         Delay_ms(5);
//     }

//     if (found == 0) {
//         USART1_SendString("  NO I2C devices found!\r\n");
//         USART1_SendString("  Check:\r\n");
//         USART1_SendString("  1. SCL → PB6, SDA → PB7\r\n");
//         USART1_SendString("  2. 4.7kOhm pull-up resistors on SCL & SDA to 3.3V\r\n");
//         USART1_SendString("  3. MPU6050 VCC = 3.3V (NOT 5V)\r\n");
//         USART1_SendString("  4. GND connected\r\n");
//     } else {
//         USART1_SendString("Found ");
//         USART1_SendNum(found);
//         USART1_SendString(" device(s).\r\n");
//     }
//     USART1_SendString("--- Scan Done ---\r\n\r\n");
// }

// // ============================================================================
// // MPU6050
// // ============================================================================

// uint8_t MPU6050_WriteReg(uint8_t reg, uint8_t value) {
//     uint8_t data[2] = { reg, value };
//     return I2C1_Write(MPU6050_ADDR, data, 2);
// }

// uint8_t MPU6050_ReadReg(uint8_t reg, uint8_t *value) {
//     if (!I2C1_Write(MPU6050_ADDR, &reg, 1)) return 0;
//     return I2C1_Read(MPU6050_ADDR, value, 1);
// }

// uint8_t MPU6050_ReadRegs(uint8_t reg, uint8_t *data, uint8_t len) {
//     if (!I2C1_Write(MPU6050_ADDR, &reg, 1)) return 0;
//     return I2C1_Read(MPU6050_ADDR, data, len);
// }

// uint8_t MPU6050_Init(void) {
//     uint8_t who = 0;
//     if (!MPU6050_ReadReg(MPU6050_WHO_AM_I, &who)) {
//         USART1_SendString("  ERROR: Cannot read WHO_AM_I register\r\n");
//         return 0;
//     }
//     USART1_SendString("  WHO_AM_I = ");
//     USART1_SendHex(who);
//     USART1_SendString(" (expected 0x68)\r\n");

//     if (who != 0x68) {
//         USART1_SendString("  ERROR: Wrong WHO_AM_I value!\r\n");
//         return 0;
//     }

//     MPU6050_WriteReg(MPU6050_PWR_MGMT_1, 0x00);
//     Delay_ms(100);
//     MPU6050_WriteReg(MPU6050_SMPLRT_DIV, 9);
//     MPU6050_WriteReg(MPU6050_CONFIG, 0x03);
//     MPU6050_WriteReg(MPU6050_GYRO_CONFIG, 0x00);
//     MPU6050_WriteReg(MPU6050_ACCEL_CONFIG, 0x00);
//     MPU6050_WriteReg(MPU6050_INT_ENABLE, 0x00);
//     Delay_ms(50);

//     USART1_SendString("  Calibrating... keep board still & level!\r\n");

//     int32_t sax = 0, say = 0, saz = 0, sgx = 0, sgy = 0, sgz = 0;
//     uint8_t data[14];
//     int n = 0;

//     for (int i = 0; i < 256; i++) {
//         if (MPU6050_ReadRegs(MPU6050_ACCEL_XOUT_H, data, 14)) {
//             sax += (int16_t)((data[0]  << 8) | data[1]);
//             say += (int16_t)((data[2]  << 8) | data[3]);
//             saz += (int16_t)((data[4]  << 8) | data[5]);
//             sgx += (int16_t)((data[8]  << 8) | data[9]);
//             sgy += (int16_t)((data[10] << 8) | data[11]);
//             sgz += (int16_t)((data[12] << 8) | data[13]);
//             n++;
//         }
//         Delay_ms(5);
//     }

//     if (n == 0) {
//         USART1_SendString("  ERROR: Calibration failed (no samples)\r\n");
//         return 0;
//     }

//     accel_offset_x = sax / n;
//     accel_offset_y = say / n;
//     accel_offset_z = (saz / n) - 16384;
//     gyro_offset_x  = sgx / n;
//     gyro_offset_y  = sgy / n;
//     gyro_offset_z  = sgz / n;

//     USART1_SendString("  Calibration done. Samples: ");
//     USART1_SendNum(n);
//     USART1_SendString("\r\n");
//     return 1;
// }

// uint8_t MPU6050_ReadSensorData(float *ax, float *ay, float *az,
//                                 float *gx, float *gy, float *gz, float *temp) {
//     uint8_t data[14];
//     if (!MPU6050_ReadRegs(MPU6050_ACCEL_XOUT_H, data, 14)) return 0;

//     int16_t rax = (int16_t)((data[0]  << 8) | data[1])  - accel_offset_x;
//     int16_t ray = (int16_t)((data[2]  << 8) | data[3])  - accel_offset_y;
//     int16_t raz = (int16_t)((data[4]  << 8) | data[5])  - accel_offset_z;
//     int16_t rtemp = (int16_t)((data[6] << 8) | data[7]);
//     int16_t rgx = (int16_t)((data[8]  << 8) | data[9])  - gyro_offset_x;
//     int16_t rgy = (int16_t)((data[10] << 8) | data[11]) - gyro_offset_y;
//     int16_t rgz = (int16_t)((data[12] << 8) | data[13]) - gyro_offset_z;

//     *ax = rax * ACCEL_SCALE;
//     *ay = ray * ACCEL_SCALE;
//     *az = raz * ACCEL_SCALE;
//     *gx = rgx * GYRO_SCALE;
//     *gy = rgy * GYRO_SCALE;
//     *gz = rgz * GYRO_SCALE;
//     *temp = (rtemp / 340.0f) + 36.53f;
//     return 1;
// }

// void UpdateIMU(float ax, float ay, float az,
//                float gx, float gy, float gz,
//                float *roll, float *pitch, float *yaw) {
//     roll_filtered  += gx * DT;
//     pitch_filtered += gy * DT;
//     yaw_filtered   += gz * DT;

//     float accel_roll  = atan2f(ay, sqrtf(ax * ax + az * az)) * 180.0f / PI;
//     float accel_pitch = atan2f(-ax, sqrtf(ay * ay + az * az)) * 180.0f / PI;

//     roll_filtered  = GYRO_WEIGHT * roll_filtered  + (1.0f - GYRO_WEIGHT) * accel_roll;
//     pitch_filtered = GYRO_WEIGHT * pitch_filtered + (1.0f - GYRO_WEIGHT) * accel_pitch;

//     *roll  = roll_filtered;
//     *pitch = pitch_filtered;
//     *yaw   = yaw_filtered;
// }

// // ============================================================================
// // MAIN — Diagnostic Mode
// // ============================================================================

// int main(void) {
//     SystemClock_Init();
//     SysTick_Init();
//     GPIO_Init();

//     // STEP 1: Show that the board is alive (LED solid ON for 2 sec)
//     GPIOC_ODR &= ~(1 << 13);  // LED ON
//     Delay_ms(2000);
//     GPIOC_ODR |= (1 << 13);   // LED OFF
//     Delay_ms(500);

//     // STEP 2: Init UART, marker blink
//     USART1_Init();
//     LED_BlinkFast(5);
//     Delay_ms(500);

//     // STEP 3: Test UART by sending HELLO 5 times
//     for (int i = 0; i < 5; i++) {
//         USART1_SendString("HELLO from STM32 - test ");
//         USART1_SendNum(i + 1);
//         USART1_SendString("\r\n");
//         LED_BlinkSlow(1);
//     }

//     USART1_SendString("\r\n=============================================\r\n");
//     USART1_SendString(" STM32F103 + MPU6050 Diagnostic\r\n");
//     USART1_SendString("=============================================\r\n");

//     // STEP 4: Init I2C and scan the bus
//     I2C1_Init();
//     Delay_ms(100);
//     I2C_Scan();

//     // STEP 5: Try to talk to MPU6050
//     USART1_SendString("--- MPU6050 Init ---\r\n");
//     if (!MPU6050_Init()) {
//         USART1_SendString("\r\nMPU6050 NOT WORKING!\r\n");
//         USART1_SendString("LED will do 3 slow blinks repeatedly.\r\n");
//         while (1) {
//             LED_BlinkSlow(3);
//             Delay_ms(1000);
//         }
//     }

//     USART1_SendString("\r\nMPU6050 OK! Long LED blink incoming...\r\n");
//     LED_BlinkLong();

//     USART1_SendString("\r\nStarting sensor stream (Roll, Pitch, Yaw):\r\n");
//     USART1_SendString("Tilt the board to see the angles change!\r\n\r\n");

//     uint32_t last = 0;
//     uint32_t led_count = 0;
//     char buf[200];

//     while (1) {
//         if ((tick_ms - last) >= 10) {
//             last = tick_ms;

//             float ax, ay, az, gx, gy, gz, temp;
//             float roll, pitch, yaw;

//             if (MPU6050_ReadSensorData(&ax, &ay, &az, &gx, &gy, &gz, &temp)) {
//                 UpdateIMU(ax, ay, az, gx, gy, gz, &roll, &pitch, &yaw);

//                 snprintf(buf, sizeof(buf),
//                     "Roll:%7.2f Pitch:%7.2f Yaw:%7.2f | AX:%5.2f AY:%5.2f AZ:%5.2f | T:%.1fC\r\n",
//                     roll, pitch, yaw, ax, ay, az, temp);
//                 USART1_SendString(buf);

//                 if (++led_count >= 100) {
//                     led_count = 0;
//                     LED_TOGGLE();
//                 }
//             }
//         }
//     }
// }