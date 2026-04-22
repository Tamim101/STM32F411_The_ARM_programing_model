// =============================================================================
// STM32F103 BLUE PILL — BARE METAL DRONE PID + IMU FIRMWARE
// =============================================================================
// NO HAL. NO STDLIB. NO LIBRARIES. Pure register-level C.
//
// What this file does (in order every 1ms):
//   1. Read raw gyro + accel from MPU6050 over I2C
//   2. Complementary filter  → stable roll angle
//   3. PID controller        → correction torque
//   4. Send data over UART2 to pygame live plotter
//   5. Receive Kp/Ki/Kd updates from pygame tuner
//
// WIRING:
//   MPU6050 SDA  → PB7   (I2C1 SDA)
//   MPU6050 SCL  → PB6   (I2C1 SCL)
//   MPU6050 VCC  → 3.3V
//   MPU6050 GND  → GND
//   MPU6050 AD0  → GND   → I2C address = 0x68
//
//   USB-TTL TX   → PA3   (USART2 RX)
//   USB-TTL RX   → PA2   (USART2 TX)
//   Baud rate    = 115200
// =============================================================================

#include <stdint.h>
// stdint.h is NOT a runtime library — it just defines types like:
// uint8_t  = unsigned 8-bit  (0 to 255)
// int16_t  = signed 16-bit   (-32768 to 32767)
// uint32_t = unsigned 32-bit (0 to 4,294,967,295)
// Without these, we'd write "unsigned char" and "unsigned long" everywhere — messy.

// =============================================================================
// SECTION 1 — REGISTER ADDRESSES
// =============================================================================
// The STM32F103 CPU controls everything by reading/writing memory addresses.
// Each peripheral (GPIO, UART, I2C) lives at a fixed address in memory.
// These are in the STM32F103 Reference Manual (RM0008).
//
// The pattern:  (*(volatile uint32_t*)(ADDRESS))
//   - ADDRESS        = where the register is in memory
//   - (uint32_t*)    = treat that address as a pointer to a 32-bit integer
//   - *              = dereference → actually read/write that memory location
//   - volatile       = do NOT cache this value in a CPU register.
//                      Always go to actual memory. Needed because hardware
//                      can change these values at any time (interrupts, DMA).

// RCC = Reset and Clock Control
// Before ANY peripheral works, you must turn on its clock here.
// Think of it as a power switch for each peripheral.
#define RCC_BASE       0x40021000UL
#define RCC_APB2ENR    (*(volatile uint32_t*)(RCC_BASE + 0x18))
// APB2ENR = "APB2 Enable Register" — controls clocks for GPIOA, GPIOB, USART1
#define RCC_APB1ENR    (*(volatile uint32_t*)(RCC_BASE + 0x1C))
// APB1ENR = "APB1 Enable Register" — controls clocks for I2C1, USART2

// Bit positions inside RCC_APB2ENR (each bit = one peripheral's clock):
#define RCC_APB2ENR_AFIOEN  (1UL << 0)  // Alternate Function I/O clock
#define RCC_APB2ENR_IOPAEN  (1UL << 2)  // GPIOA clock
#define RCC_APB2ENR_IOPBEN  (1UL << 3)  // GPIOB clock

// Bit positions inside RCC_APB1ENR:
#define RCC_APB1ENR_USART2EN (1UL << 17) // USART2 clock
#define RCC_APB1ENR_I2C1EN   (1UL << 21) // I2C1 clock

// GPIOA: controls pins PA0 – PA15
// PA2 = USART2_TX (we send data out here)
// PA3 = USART2_RX (we receive data here)
#define GPIOA_BASE     0x40010800UL
#define GPIOA_CRL      (*(volatile uint32_t*)(GPIOA_BASE + 0x00))
// CRL = Configuration Register Low → configures PA0 to PA7
// Each pin uses 4 bits: [MODE1:MODE0 | CNF1:CNF0]
// MODE bits: 00=input, 01=output 10MHz, 10=output 2MHz, 11=output 50MHz
// CNF bits (output): 00=push-pull, 01=open-drain, 10=AF push-pull, 11=AF open-drain
// CNF bits (input):  00=analog,    01=floating,   10=pull-up/down

// GPIOB: controls pins PB0 – PB15
// PB6 = I2C1_SCL, PB7 = I2C1_SDA
#define GPIOB_BASE     0x40010C00UL
#define GPIOB_CRL      (*(volatile uint32_t*)(GPIOB_BASE + 0x00))

// USART2: serial communication peripheral
// PA2 = TX, PA3 = RX, 115200 baud
#define USART2_BASE    0x40004400UL
#define USART2_SR      (*(volatile uint32_t*)(USART2_BASE + 0x00))
// SR = Status Register. We read bits here to know what the UART is doing.
#define USART2_DR      (*(volatile uint32_t*)(USART2_BASE + 0x04))
// DR = Data Register. Write here to SEND a byte. Read here to RECEIVE a byte.
#define USART2_BRR     (*(volatile uint32_t*)(USART2_BASE + 0x08))
// BRR = Baud Rate Register. Controls serial speed.
#define USART2_CR1     (*(volatile uint32_t*)(USART2_BASE + 0x0C))
// CR1 = Control Register 1. Enable UART, TX, RX here.

// Key bits in USART2_SR:
#define USART_SR_TXE   (1UL << 7)  // TX register Empty → safe to write next byte
#define USART_SR_RXNE  (1UL << 5)  // RX register Not Empty → new byte received

// Key bits in USART2_CR1:
#define USART_CR1_UE   (1UL << 13) // USART Enable
#define USART_CR1_TE   (1UL << 3)  // Transmitter Enable
#define USART_CR1_RE   (1UL << 2)  // Receiver Enable

// I2C1: two-wire serial bus to talk to MPU6050
// PB6 = SCL (clock), PB7 = SDA (data)
#define I2C1_BASE      0x40005400UL
#define I2C1_CR1       (*(volatile uint32_t*)(I2C1_BASE + 0x00))
#define I2C1_CR2       (*(volatile uint32_t*)(I2C1_BASE + 0x04))
#define I2C1_CCR       (*(volatile uint32_t*)(I2C1_BASE + 0x1C))
#define I2C1_TRISE     (*(volatile uint32_t*)(I2C1_BASE + 0x20))
#define I2C1_SR1       (*(volatile uint32_t*)(I2C1_BASE + 0x14))
#define I2C1_SR2       (*(volatile uint32_t*)(I2C1_BASE + 0x18))
#define I2C1_DR        (*(volatile uint32_t*)(I2C1_BASE + 0x10))

// Key bits in I2C1_CR1:
#define I2C_CR1_PE     (1UL << 0)  // Peripheral Enable
#define I2C_CR1_START  (1UL << 8)  // Generate START condition
#define I2C_CR1_STOP   (1UL << 9)  // Generate STOP condition
#define I2C_CR1_ACK    (1UL << 10) // Acknowledge received bytes

// Key bits in I2C1_SR1:
#define I2C_SR1_SB     (1UL << 0)  // Start Bit generated
#define I2C_SR1_ADDR   (1UL << 1)  // Address acknowledged by slave
#define I2C_SR1_BTF    (1UL << 2)  // Byte Transfer Finished
#define I2C_SR1_RXNE   (1UL << 6)  // Receive register not empty
#define I2C_SR1_TXE    (1UL << 7)  // Transmit register empty

// SysTick: a countdown timer built into every ARM Cortex-M core
// We use it for millisecond timing
#define SYSTICK_BASE   0xE000E010UL
#define SYSTICK_CTRL   (*(volatile uint32_t*)(SYSTICK_BASE + 0x00))
#define SYSTICK_LOAD   (*(volatile uint32_t*)(SYSTICK_BASE + 0x04))
#define SYSTICK_VAL    (*(volatile uint32_t*)(SYSTICK_BASE + 0x08))

// =============================================================================
// SECTION 2 — MPU6050 INTERNAL REGISTER MAP
// =============================================================================
// These are addresses INSIDE the MPU6050 chip.
// We send them over I2C to specify which register to read/write.

#define MPU6050_ADDR         0x68  // 7-bit I2C address when AD0 pin = GND
#define MPU_PWR_MGMT_1       0x6B  // Power management — write 0 to wake up
#define MPU_SMPLRT_DIV       0x19  // Sample rate divider
#define MPU_CONFIG           0x1A  // Digital Low Pass Filter config
#define MPU_GYRO_CONFIG      0x1B  // Gyro full-scale range
#define MPU_ACCEL_CONFIG     0x1C  // Accel full-scale range
#define MPU_ACCEL_XOUT_H     0x3B  // First of 14 data bytes

// Sensitivity from MPU6050 datasheet:
// Gyro  ±250°/s  → 131.0 raw counts per °/s → divide raw by 131 = °/s
// Accel ±2g      → 16384 raw counts per g   → divide raw by 16384 = g
#define GYRO_SCALE    131.0f
#define ACCEL_SCALE   16384.0f

// =============================================================================
// SECTION 3 — GLOBAL STATE
// =============================================================================

volatile uint32_t ms_ticks = 0;
// volatile: the compiler must not optimize this away.
// It's modified inside SysTick_Handler (an interrupt), which the compiler
// can't see being called in normal code flow. Without volatile, the compiler
// might cache the value in a register and never update it.

// Raw sensor readings (integers from ADC inside MPU6050, range ±32768)
int16_t raw_ax, raw_ay, raw_az;  // accelerometer raw counts
int16_t raw_gx, raw_gy, raw_gz;  // gyroscope raw counts

// Converted sensor readings (real physical units)
float accel_x, accel_y, accel_z; // in g  (gravitational units)
float gyro_x,  gyro_y,  gyro_z;  // in °/s (degrees per second)

// State estimate
float roll_angle = 0.0f;  // current estimated roll angle in degrees
                           // 0° = level, +30° = right side down, -30° = left side down

// PID variables
float setpoint   = 0.0f;  // target roll angle (0 = level flight)
float pid_output = 0.0f;  // PID total output (becomes motor torque command)
float p_term     = 0.0f;  // proportional term (for telemetry display)
float i_term     = 0.0f;  // integral accumulator (persists between loops!)
float d_term     = 0.0f;  // derivative term (for telemetry display)
float last_error = 0.0f;  // error from previous loop (needed for derivative)

// PID tuning gains — these are the numbers you tune in the pygame simulator
float Kp = 1.2f;   // Proportional: bigger = more aggressive correction
float Ki = 0.01f;  // Integral: bigger = faster removal of steady-state error
float Kd = 0.4f;   // Derivative: bigger = more damping of oscillation

// Loop timing
float dt = 0.001f;  // time step in seconds (1ms = 0.001s initially)

// =============================================================================
// SECTION 4 — CUSTOM MATH (no <math.h>)
// =============================================================================
// We implement only what we need. This keeps the binary small and avoids
// linking the full C math library.

float f_abs(float x) {
    return (x < 0.0f) ? -x : x;
    // Ternary operator: condition ? value_if_true : value_if_false
}

// Iterative square root using Newton-Raphson method
// Starting guess x, then x = (x + n/x)/2 converges to sqrt(n) quickly
float f_sqrt(float n) {
    if (n <= 0.0f) return 0.0f;
    float x = n, y = 1.0f;
    while (f_abs(x - y) > 0.0001f) {
        x = (x + y) * 0.5f;
        y = n / x;
    }
    return x;
}

// atan2 approximation (max error < 0.005 radians = 0.28°, good enough)
// Needed to compute roll angle from accelerometer
float f_atan2(float y, float x) {
    float abs_y = f_abs(y) + 1e-10f;  // avoid division by zero
    float r, angle;
    if (x >= 0.0f) {
        r = (x - abs_y) / (x + abs_y);
        angle = 0.7854f + (-0.0742f + 0.2448f * r) * r;
        // polynomial approximation of atan in first octant
    } else {
        r = (x + abs_y) / (abs_y - x);
        angle = 2.3562f + (-0.0742f + 0.2448f * r) * r;
    }
    return (y < 0.0f) ? -angle : angle;
    // 2.3562 = 3π/4, 0.7854 = π/4
}

float rad_to_deg(float r) { return r * 57.2957795f; }
// 57.2957... = 180/π. Multiply radians by this to get degrees.

// =============================================================================
// SECTION 5 — SYSTICK (millisecond counter)
// =============================================================================

void systick_init(void) {
    // SysTick counts DOWN from LOAD value to 0, then fires interrupt, reloads.
    // At 8MHz system clock: 8,000,000 ticks per second
    // For 1ms interrupt: LOAD = 8,000,000 / 1000 - 1 = 7999
    SYSTICK_LOAD = 8000 - 1;  // reload value: 7999 → 1ms period at 8MHz
    SYSTICK_VAL  = 0;          // reset current counter
    // CTRL bits: [0]=enable counter, [1]=enable interrupt, [2]=use CPU clock
    SYSTICK_CTRL = (1 << 0) | (1 << 1) | (1 << 2);
}

// This runs automatically every 1ms — ARM looks for this exact function name
void SysTick_Handler(void) {
    ms_ticks++;
    // Every millisecond, increment our counter.
    // Main code reads ms_ticks to know elapsed time.
}

void delay_ms(uint32_t ms) {
    uint32_t start = ms_ticks;
    while ((ms_ticks - start) < ms);
    // Spin here doing nothing until 'ms' milliseconds have passed.
    // Subtraction is safe even when ms_ticks wraps around (uint32 overflow).
}

// Returns approximate microseconds — used for precise dt measurement
uint32_t get_micros(void) {
    uint32_t ms  = ms_ticks;
    uint32_t val = SYSTICK_VAL;    // current countdown (decreasing)
    // SYSTICK_VAL counts down: at start of ms it's 7999, at end it's 0
    // Each count = 1/8 µs at 8MHz
    return ms * 1000 + (8000 - 1 - val) / 8;
}

// =============================================================================
// SECTION 6 — GPIO CONFIGURATION
// =============================================================================

void gpio_init(void) {
    // Step 1: turn on clocks for GPIOA, GPIOB, and AFIO
    RCC_APB2ENR |= RCC_APB2ENR_IOPAEN | RCC_APB2ENR_IOPBEN | RCC_APB2ENR_AFIOEN;
    // |= means "set these bits without changing others"
    // This is safe because we don't want to disable clocks that might already be on

    // ---- PA2 = USART2_TX ----
    // CRL controls PA0–PA7. Each pin uses 4 bits.
    // PA2 is at bit positions [11:8] in CRL (pin number × 4 = 2 × 4 = 8)
    // We want: MODE=11 (50MHz), CNF=10 (AF push-pull) → binary 1011 = 0xB
    GPIOA_CRL &= ~(0xFUL << 8);  // clear bits 11:8 (& with mask that has 0s here)
    GPIOA_CRL |=  (0xBUL << 8);  // set  bits 11:8 to 1011

    // ---- PA3 = USART2_RX ----
    // PA3 is at bit positions [15:12] in CRL (3 × 4 = 12)
    // Input floating: MODE=00, CNF=01 → binary 0100 = 0x4
    GPIOA_CRL &= ~(0xFUL << 12);
    GPIOA_CRL |=  (0x4UL << 12);

    // ---- PB6 = I2C1_SCL, PB7 = I2C1_SDA ----
    // I2C requires open-drain: multiple devices share the bus; anyone can pull LOW.
    // Push-pull would fight between master and slave → damage or malfunction.
    // MODE=11 (50MHz), CNF=11 (AF open-drain) → binary 1111 = 0xF
    // PB6 bits [27:24], PB7 bits [31:28]
    GPIOB_CRL &= ~(0xFFUL << 24); // clear 8 bits covering PB6 and PB7
    GPIOB_CRL |=  (0xFFUL << 24); // set both to 0xF (AF open-drain 50MHz)
}

// =============================================================================
// SECTION 7 — USART2 (serial port to PC)
// =============================================================================

void uart_init(void) {
    RCC_APB1ENR |= RCC_APB1ENR_USART2EN;  // turn on USART2 clock

    // Set baud rate
    // BRR = (system_clock_Hz) / (baud_rate)
    // = 8,000,000 / 115200 = 69.44 → round to 69 = 0x45
    // Small rounding error (< 0.5%) is within UART tolerance
    USART2_BRR = 0x0045;

    // Enable USART2 with TX and RX
    USART2_CR1 = USART_CR1_UE | USART_CR1_TE | USART_CR1_RE;
}

void uart_send_byte(uint8_t b) {
    while (!(USART2_SR & USART_SR_TXE)); // wait: TXE=1 means buffer empty, ready
    USART2_DR = b;  // writing to DR loads the transmit shift register
}

void uart_send_str(const char* s) {
    while (*s) { uart_send_byte((uint8_t)*s++); }
    // *s  = dereference pointer (get character at current address)
    // s++ = advance pointer to next character
    // loop stops when *s == 0 (null terminator '\0')
}

uint8_t uart_available(void) {
    return (USART2_SR & USART_SR_RXNE) ? 1 : 0;
    // RXNE=1 means a new byte arrived and is waiting in USART2_DR
}

uint8_t uart_read(void) {
    return (uint8_t)(USART2_DR & 0xFF);
    // Reading DR clears RXNE automatically
}

// Send signed integer as decimal text (e.g. -314 → "-314")
void uart_send_int(int32_t n) {
    if (n < 0) { uart_send_byte('-'); n = -n; }
    if (n == 0) { uart_send_byte('0'); return; }
    char buf[12]; int i = 0;
    while (n > 0) { buf[i++] = (char)('0' + (n % 10)); n /= 10; }
    // digits are stored in reverse (least significant first), so send backwards
    for (int j = i - 1; j >= 0; j--) uart_send_byte((uint8_t)buf[j]);
}

// Send float encoded as integer×100 (e.g. 3.14 → "314")
// Avoids needing float-to-string formatting code
void uart_send_float(float f) {
    uart_send_int((int32_t)(f * 100.0f));
}

// =============================================================================
// SECTION 8 — I2C INITIALIZATION
// =============================================================================

void i2c_init(void) {
    RCC_APB1ENR |= RCC_APB1ENR_I2C1EN;

    // Software reset: clears any stuck state (I2C can get stuck in BUSY)
    I2C1_CR1 |= (1UL << 15);   // set SWRST bit
    I2C1_CR1 &= ~(1UL << 15);  // clear SWRST bit

    // CR2[5:0] = APB1 clock frequency in MHz — must match actual clock
    // APB1 = 8MHz on Blue Pill (default HSI configuration)
    I2C1_CR2 = 8;

    // CCR = Clock Control Register — sets I2C bus speed
    // Standard mode (Sm) = 100kHz
    // CCR = f_APB1 / (2 × f_SCL) = 8,000,000 / (2 × 100,000) = 40
    I2C1_CCR = 40;

    // TRISE = maximum SCL rise time in APB1 clock cycles
    // Standard mode spec: rise time ≤ 1000ns
    // TRISE = (1000ns × f_APB1_MHz) + 1 = (1000 × 10^-9 × 8 × 10^6) + 1 = 9
    I2C1_TRISE = 9;

    I2C1_CR1 = I2C_CR1_PE;  // enable I2C peripheral
}

// =============================================================================
// SECTION 9 — I2C BUS OPERATIONS
// =============================================================================
// I2C protocol basics:
//   START  → master takes control of bus (SDA goes LOW while SCL is HIGH)
//   ADDR+W → 7-bit address + 0 (write) → slave acknowledges
//   ADDR+R → 7-bit address + 1 (read)  → slave acknowledges
//   DATA   → 8-bit data byte, each followed by ACK or NACK
//   STOP   → master releases bus (SDA goes HIGH while SCL is HIGH)
//
// To READ a register from MPU6050:
//   START → ADDR+W → REG_ADDR → RESTART → ADDR+R → DATA → NACK → STOP

void i2c_start(void) {
    I2C1_CR1 |= I2C_CR1_START;
    while (!(I2C1_SR1 & I2C_SR1_SB));
    // SB (Start Bit) flag is set when START condition was successfully generated.
    // We MUST wait for this before sending the address.
}

void i2c_stop(void) {
    I2C1_CR1 |= I2C_CR1_STOP;
    // Hardware generates STOP after current byte transfer finishes
}

void i2c_addr_w(uint8_t addr) {
    I2C1_DR = (uint32_t)((addr << 1) | 0); // address in upper 7 bits, bit0=0=write
    while (!(I2C1_SR1 & I2C_SR1_ADDR));    // wait for slave to ACK the address
    (void)I2C1_SR2; // MUST read SR2 after SR1 to clear ADDR flag (hardware requirement!)
}

void i2c_addr_r(uint8_t addr) {
    I2C1_DR = (uint32_t)((addr << 1) | 1); // bit0=1=read
    while (!(I2C1_SR1 & I2C_SR1_ADDR));
    (void)I2C1_SR2;
}

void i2c_write(uint8_t data) {
    while (!(I2C1_SR1 & I2C_SR1_TXE));  // wait until TX buffer empty
    I2C1_DR = data;
    while (!(I2C1_SR1 & I2C_SR1_BTF));  // wait until byte fully shifted out
}

uint8_t i2c_read_ack(void) {
    I2C1_CR1 |= I2C_CR1_ACK;            // ACK = "send me more bytes"
    while (!(I2C1_SR1 & I2C_SR1_RXNE)); // wait for byte
    return (uint8_t)(I2C1_DR & 0xFF);
}

uint8_t i2c_read_nack(void) {
    I2C1_CR1 &= ~I2C_CR1_ACK; // NACK = "that's the last byte I want"
    i2c_stop();                // send STOP after this byte
    while (!(I2C1_SR1 & I2C_SR1_RXNE));
    return (uint8_t)(I2C1_DR & 0xFF);
}

void mpu_write(uint8_t reg, uint8_t val) {
    i2c_start();
    i2c_addr_w(MPU6050_ADDR);
    i2c_write(reg);
    i2c_write(val);
    i2c_stop();
}

void mpu_read(uint8_t reg, uint8_t* buf, uint8_t len) {
    i2c_start();
    i2c_addr_w(MPU6050_ADDR);
    i2c_write(reg);           // tell MPU which register to start from
    i2c_start();              // repeated START (no STOP between write and read)
    i2c_addr_r(MPU6050_ADDR);
    for (uint8_t i = 0; i < len; i++) {
        buf[i] = (i == len - 1) ? i2c_read_nack() : i2c_read_ack();
        // last byte: NACK+STOP. All others: ACK.
    }
}

// =============================================================================
// SECTION 10 — MPU6050 INIT
// =============================================================================

void mpu_init(void) {
    delay_ms(150); // let MPU6050 stabilize after power-up

    mpu_write(MPU_PWR_MGMT_1,  0x00); // wake up (clear sleep bit)
    delay_ms(10);
    mpu_write(MPU_SMPLRT_DIV,  0x00); // sample rate = 1kHz (no division)
    mpu_write(MPU_CONFIG,      0x03); // DLPF: 44Hz accel / 42Hz gyro bandwidth
    // DLPF reduces vibration noise but adds ~4.9ms latency — acceptable for attitude
    mpu_write(MPU_GYRO_CONFIG,  0x00); // gyro ±250°/s (most sensitive)
    mpu_write(MPU_ACCEL_CONFIG, 0x00); // accel ±2g (most sensitive)
    delay_ms(100);
}

// =============================================================================
// SECTION 11 — READ SENSOR DATA
// =============================================================================

void imu_read(void) {
    uint8_t buf[14];
    // MPU6050 stores sensor data in 14 consecutive registers starting at 0x3B:
    // [0,1]=ACCEL_X  [2,3]=ACCEL_Y  [4,5]=ACCEL_Z
    // [6,7]=TEMP     [8,9]=GYRO_X  [10,11]=GYRO_Y  [12,13]=GYRO_Z
    // Each value is 16-bit big-endian (high byte first, low byte second)

    mpu_read(MPU_ACCEL_XOUT_H, buf, 14);

    // Combine two 8-bit bytes into one 16-bit signed integer
    // Example: buf[0]=0x03, buf[1]=0xE8 → (0x03 << 8) | 0xE8 = 0x03E8 = 1000
    raw_ax = (int16_t)((buf[0]  << 8) | buf[1]);
    raw_ay = (int16_t)((buf[2]  << 8) | buf[3]);
    raw_az = (int16_t)((buf[4]  << 8) | buf[5]);
    // skip buf[6,7] = temperature
    raw_gx = (int16_t)((buf[8]  << 8) | buf[9]);
    raw_gy = (int16_t)((buf[10] << 8) | buf[11]);
    raw_gz = (int16_t)((buf[12] << 8) | buf[13]);

    // Convert raw counts to physical units
    accel_x = (float)raw_ax / ACCEL_SCALE; // g (1.0g when pointing up)
    accel_y = (float)raw_ay / ACCEL_SCALE;
    accel_z = (float)raw_az / ACCEL_SCALE;
    gyro_x  = (float)raw_gx / GYRO_SCALE;  // degrees per second
    gyro_y  = (float)raw_gy / GYRO_SCALE;
    gyro_z  = (float)raw_gz / GYRO_SCALE;
}

// =============================================================================
// SECTION 12 — COMPLEMENTARY FILTER
// =============================================================================
//
// WHY TWO SENSORS?
//
//   Gyroscope: measures rotation RATE (°/s). Integrating gives angle.
//     angle += gyro_rate × dt
//   Problem: gyro has a tiny bias (e.g. 0.01°/s even when still).
//   Over 10 minutes: 0.01 × 600s = 6° drift. Unusable without correction.
//
//   Accelerometer: measures forces. At rest, it measures only gravity.
//   Gravity vector points DOWN always. So accel can compute tilt angle:
//     roll = atan2(ay, az)
//   Problem: during flight, motors vibrate and drone accelerates.
//   Accel sees motor vibration as fake gravity. Angle becomes noisy.
//
//   SOLUTION — Complementary Filter:
//   Blend both: trust gyro for fast motion, use accel to prevent drift.
//
//   angle = alpha × (angle + gyro×dt) + (1 - alpha) × accel_angle
//
//   alpha = 0.98 means:
//   - 98% of new angle comes from integrating the gyro (fast, accurate short-term)
//   - 2% correction from accel angle (slow, drift-free long-term)
//   - Net effect: fast response + no long-term drift

void complementary_filter(void) {
    // Compute roll angle from accelerometer
    // When drone is level: ay=0, az=1g → atan2(0, 1) = 0°
    // When drone rolls 30°: ay=0.5g, az=0.866g → atan2(0.5, 0.866) = 30°
    float accel_roll = rad_to_deg(f_atan2(accel_y, accel_z));
    // Note: this formula gives roll angle but is affected by pitch.
    // For a single-axis experiment (only rolling, no pitch), it's perfect.

    // Integrate gyroscope to get angle change this loop
    // gyro_x = roll rate in °/s
    // multiplied by dt (seconds) = degrees rotated this loop
    float gyro_delta = gyro_x * dt;

    // Blend: 98% gyro integration + 2% accel correction
    roll_angle = 0.98f * (roll_angle + gyro_delta)
               + 0.02f * accel_roll;
    //          ^^^^^ fast gyro path         ^^^^^ slow accel correction
}

// =============================================================================
// SECTION 13 — PID CONTROLLER
// =============================================================================
//
// PID in one sentence: "look at the mistake, remember past mistakes,
// predict future mistakes — then push back against all three."
//
//  ERROR = setpoint - measured_angle
//
//  P (Proportional): push = Kp × error
//    → Big error? Big push. Twice the error? Twice the force.
//    → Problem: like a spring — it overshoots, oscillates.
//
//  I (Integral): push += Ki × error × dt   (sums ALL past errors)
//    → If small error persists for a long time, I grows until it fixes it.
//    → Eliminates "steady-state error" that P alone can never fix.
//    → Problem: if Kp is too small, I "winds up" too much → instability.
//
//  D (Derivative): push = Kd × (error - last_error) / dt
//    → Reacts to how FAST the error is changing.
//    → If error is shrinking rapidly, D applies a brake → less overshoot.
//    → Problem: amplifies sensor noise. Needs clean sensor data (use DLPF).
//
//  TOTAL: pid_output = P + I + D

void pid_update(void) {
    float error = setpoint - roll_angle;
    // Positive error = drone tilted wrong way = we need to apply corrective torque

    // --- P term ---
    p_term = Kp * error;
    // Instantly reacts to current error. This alone causes oscillation.

    // --- I term ---
    i_term += Ki * error * dt;
    // Accumulates over time. Grows when error persists, shrinks when corrected.
    // ANTI-WINDUP: limit I to prevent it growing forever when something is wrong
    if (i_term >  50.0f) i_term =  50.0f;
    if (i_term < -50.0f) i_term = -50.0f;

    // --- D term ---
    // (error - last_error) = change in error since last loop
    // divided by dt = rate of change (degrees per second)
    d_term = Kd * (error - last_error) / dt;
    // If error is shrinking: (error - last_error) is negative → D brakes the push
    // If error is growing:   (error - last_error) is positive → D amplifies push

    pid_output = p_term + i_term + d_term;

    // Clamp: motors have a physical range limit
    if (pid_output >  100.0f) pid_output =  100.0f;
    if (pid_output < -100.0f) pid_output = -100.0f;

    last_error = error; // save for next loop's derivative calculation
}

// =============================================================================
// SECTION 14 — TELEMETRY SENDER
// =============================================================================
// Send all state data to pygame as a CSV line every 10ms (100Hz)
// Format: "R,roll×100, setpoint×100, pid×100, p×100, i×100, d×100, Kp×100, Ki×100, Kd×100\n"
// Example: "R,1523,0,1842,1440,82,320,120,1,40\n"
// pygame decodes by splitting on commas and dividing each field by 100

void send_telemetry(void) {
    uart_send_byte('R');   uart_send_byte(',');
    uart_send_float(roll_angle);   uart_send_byte(',');
    uart_send_float(setpoint);     uart_send_byte(',');
    uart_send_float(pid_output);   uart_send_byte(',');
    uart_send_float(p_term);       uart_send_byte(',');
    uart_send_float(i_term);       uart_send_byte(',');
    uart_send_float(d_term);       uart_send_byte(',');
    uart_send_float(Kp);           uart_send_byte(',');
    uart_send_float(Ki);           uart_send_byte(',');
    uart_send_float(Kd);
    uart_send_byte('\n');
}

// =============================================================================
// SECTION 15 — COMMAND RECEIVER FROM PYGAME
// =============================================================================
// Pygame sends: "P120\n" → Kp=1.20, "I1\n" → Ki=0.01, "D40\n" → Kd=0.40
// "S1500\n" → setpoint=15.00°
// All values are multiplied by 100 on the pygame side.

static char  rx_buf[16];
static uint8_t rx_pos = 0;

void receive_cmds(void) {
    while (uart_available()) {
        char c = (char)uart_read();
        if (c == '\n') {
            rx_buf[rx_pos] = '\0';
            if (rx_pos > 1) {
                char cmd = rx_buf[0];
                int32_t val = 0, sign = 1;
                uint8_t start = 1;
                if (rx_buf[1] == '-') { sign = -1; start = 2; }
                for (uint8_t i = start; i < rx_pos; i++)
                    val = val * 10 + (rx_buf[i] - '0');
                val *= sign;
                if (cmd == 'P') { Kp = val / 100.0f; i_term = 0.0f; }
                if (cmd == 'I') { Ki = val / 100.0f; i_term = 0.0f; }
                if (cmd == 'D') { Kd = val / 100.0f; }
                if (cmd == 'S') { setpoint = val / 100.0f; }
            }
            rx_pos = 0;
        } else if (rx_pos < 15) {
            rx_buf[rx_pos++] = c;
        }
    }
}

// =============================================================================
// SECTION 16 — MAIN LOOP
// =============================================================================

int main(void) {
    systick_init(); // ARM core timer: 1ms interrupt
    gpio_init();    // configure PA2/PA3 for UART, PB6/PB7 for I2C
    uart_init();    // 115200 baud serial port to PC
    i2c_init();     // 100kHz I2C to MPU6050
    mpu_init();     // wake up and configure MPU6050 sensor

    uart_send_str("DRONE_PID_READY\n"); // tell pygame we're alive
    delay_ms(500);

    uint32_t last_send = 0;  // timestamp of last telemetry send

    while (1) {
        uint32_t t0 = get_micros(); // loop start time (microseconds)

        // 1. Read raw data from IMU
        imu_read();

        // 2. Estimate roll angle (gyro + accel → stable angle)
        complementary_filter();

        // 3. Compute PID correction
        pid_update();

        // 4. Check for gain updates from pygame
        receive_cmds();

        // 5. Send telemetry at 100Hz (every 10ms = every 10 ticks)
        if (ms_ticks - last_send >= 10) {
            send_telemetry();
            last_send = ms_ticks;
        }

        // 6. Pace loop to exactly 1kHz (1000µs period)
        while ((get_micros() - t0) < 1000);

        // 7. Measure actual dt for next PID calculation
        dt = (float)(get_micros() - t0) / 1000000.0f;
        if (dt > 0.01f)   dt = 0.01f;   // clamp: never > 10ms
        if (dt < 0.0001f) dt = 0.001f;  // clamp: never < 0.1ms
    }
}