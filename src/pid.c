/*
 * ============================================================================
 * STM32F103C8T6 BLUE PILL — BARE METAL IMU ATTITUDE + PID
 * ============================================================================
 * Pure C — direct register access — zero libraries — zero HAL
 *
 * PINS (exactly matching your working IMU code):
 *   UART:  PA9  TX  → USB-TTL RX  → /dev/ttyUSB0 or COMx   (USART1)
 *          PA10 RX  → USB-TTL TX  (receive gain commands from pygame)
 *   I2C:   PB6  SCL → MPU6050 SCL  (I2C1)
 *          PB7  SDA → MPU6050 SDA  (I2C1)
 *   LED:   PC13     → onboard LED  (active LOW — blinks to show loop alive)
 *
 * CLOCK:
 *   Internal HSI 8MHz — no PLL — works on ALL Blue Pill clone chips
 *   UART BRR = 8000000 / 115200 = 69  (BRR register value)
 *
 * WHAT THIS FIRMWARE DOES every 1ms:
 *   1. Read 14 bytes from MPU6050 over I2C  (accel X/Y/Z + gyro X/Y/Z)
 *   2. Complementary filter → stable roll angle (gyro 98% + accel 2%)
 *   3. PID controller       → correction output from roll error
 *   4. Every 10ms: send telemetry CSV line to pygame over UART
 *   5. Every loop: check UART for new Kp/Ki/Kd/Setpoint from pygame
 *
 * PYGAME PROTOCOL:
 *   STM32 sends:  "R,<roll>,<setpoint>,<pid>,<p>,<i>,<d>,<kp>,<ki>,<kd>\n"
 *                  all values are float × 100 encoded as integers
 *                  e.g.  roll=15.23° → "1523"
 *
 *   Pygame sends: "P120\n"  → Kp = 1.20
 *                 "I1\n"    → Ki = 0.01
 *                 "D40\n"   → Kd = 0.40
 *                 "S1500\n" → setpoint = 15.00°
 * ============================================================================
 */

#include <stdint.h>
/*
 * stdint.h is a C language standard header — NOT a library.
 * It only defines fixed-width integer types. No functions, no code.
 * Without it we'd write "unsigned char" and "unsigned long" everywhere.
 *
 *   uint8_t  = unsigned 8-bit  integer  (0 to 255)
 *   int16_t  = signed   16-bit integer  (-32768 to +32767)
 *   uint32_t = unsigned 32-bit integer  (0 to 4,294,967,295)
 *   int32_t  = signed   32-bit integer  (-2,147,483,648 to +2,147,483,647)
 */


/* ============================================================================
 * SECTION 1 — HOW BARE-METAL REGISTER ACCESS WORKS
 * ============================================================================
 *
 * On STM32, every peripheral is controlled by writing to specific RAM addresses.
 * The CPU has no special instructions for peripherals — you just write/read memory.
 * This is called "memory-mapped I/O".
 *
 * Example from the STM32F103 Reference Manual (RM0008):
 *   GPIOC_ODR is at address 0x4001100C
 *   Bit 13 of that register controls PC13 (the LED)
 *   Writing 0 to bit 13 turns LED ON (active LOW)
 *   Writing 1 to bit 13 turns LED OFF
 *
 * The C pattern to access a register at address ADDR:
 *
 *   (*(volatile uint32_t*)(ADDR))
 *    │   │         │        └─── the memory address (a number)
 *    │   │         └──────────── cast to "pointer to 32-bit unsigned int"
 *    │   └────────────────────── volatile: never cache, always go to real memory
 *    └────────────────────────── dereference: actually read/write that address
 *
 * We wrap each one in a #define so code reads like:
 *   GPIOC_ODR |= (1 << 13);    ← human readable
 * instead of:
 *   (*(volatile uint32_t*)0x4001100C) |= (1 << 13);  ← cryptic
 * ============================================================================
 */


/* ============================================================================
 * RCC — Reset and Clock Control
 * ============================================================================
 * EVERY peripheral on STM32 starts with its clock OFF to save power.
 * You MUST enable the clock before touching any peripheral register.
 * If you skip this, register writes are silently ignored — very confusing bug.
 *
 * RCC has two bus clock enable registers:
 *   APB2ENR — controls: GPIOA, GPIOB, GPIOC, USART1, SPI1, ADC1, AFIO
 *   APB1ENR — controls: USART2, I2C1, I2C2, SPI2, TIM2..TIM4, USB
 *
 * We need:
 *   APB2ENR: GPIOA (PA9/PA10 UART), GPIOB (PB6/PB7 I2C), GPIOC (PC13 LED),
 *            USART1 (our TX/RX), AFIO (alternate function pin mapping)
 *   APB1ENR: I2C1 (MPU6050 bus)
 */
#define RCC_BASE        0x40021000UL
#define RCC_APB2ENR     (*(volatile uint32_t*)(RCC_BASE + 0x18))
#define RCC_APB1ENR     (*(volatile uint32_t*)(RCC_BASE + 0x1C))

/* Bit masks for RCC_APB2ENR — set a bit to 1 to enable that clock */
#define RCC_APB2_AFIOEN  (1UL << 0)   /* Alternate Function IO      */
#define RCC_APB2_IOPAEN  (1UL << 2)   /* GPIOA clock                */
#define RCC_APB2_IOPBEN  (1UL << 3)   /* GPIOB clock                */
#define RCC_APB2_IOPCEN  (1UL << 4)   /* GPIOC clock  (for LED PC13)*/
#define RCC_APB2_USART1EN (1UL << 14) /* USART1 clock  (PA9 TX)     */
/*
 * IMPORTANT: USART1 is on APB2 (fast bus, same speed as CPU = 8MHz).
 * USART2 is on APB1 (slower bus = 4MHz at default config).
 * We use USART1 → PA9 to match your working IMU code.
 */

/* Bit masks for RCC_APB1ENR */
#define RCC_APB1_I2C1EN  (1UL << 21)  /* I2C1 clock  (PB6/PB7)     */


/* ============================================================================
 * GPIO — General Purpose Input/Output
 * ============================================================================
 * Each GPIO port (A, B, C...) has two configuration registers:
 *   CRL = Configuration Register Low  → configures pins 0–7
 *   CRH = Configuration Register High → configures pins 8–15
 *
 * Each pin uses 4 bits in the register: [CNF1:CNF0 | MODE1:MODE0]
 *
 *   MODE bits (what direction/speed):
 *     00 = Input mode
 *     01 = Output, max speed 10 MHz
 *     10 = Output, max speed  2 MHz
 *     11 = Output, max speed 50 MHz
 *
 *   CNF bits when MODE = input (00):
 *     00 = Analog input
 *     01 = Floating input  (no pull resistor — pin floats if unconnected)
 *     10 = Input with pull-up or pull-down (set by ODR register)
 *
 *   CNF bits when MODE = output (01/10/11):
 *     00 = General purpose push-pull   (driven HIGH or LOW by software)
 *     01 = General purpose open-drain  (only drives LOW; HIGH = floating)
 *     10 = Alternate function push-pull (peripheral drives the pin)
 *     11 = Alternate function open-drain (peripheral + open-drain)
 *
 * OPEN-DRAIN explained:
 *   In I2C, both master AND slave can pull the line LOW.
 *   If both were push-pull, a conflict would short-circuit them.
 *   Open-drain: the pin can only pull LOW (like a switch to GND).
 *   The external pull-up resistor (4.7kΩ) pulls it HIGH when nobody pulls LOW.
 *   This lets multiple devices share one wire safely.
 */
#define GPIOA_BASE      0x40010800UL
#define GPIOA_CRL       (*(volatile uint32_t*)(GPIOA_BASE + 0x00)) /* PA0–PA7  */
#define GPIOA_CRH       (*(volatile uint32_t*)(GPIOA_BASE + 0x04)) /* PA8–PA15 */
/*
 * PA9  is in CRH (pins 8–15)
 * PA9 bit position in CRH = (9 - 8) * 4 = bit 4
 * PA10 bit position in CRH = (10 - 8) * 4 = bit 8
 */

#define GPIOB_BASE      0x40010C00UL
#define GPIOB_CRL       (*(volatile uint32_t*)(GPIOB_BASE + 0x00)) /* PB0–PB7  */
/*
 * PB6 bit position in CRL = 6 * 4 = bit 24
 * PB7 bit position in CRL = 7 * 4 = bit 28
 */

#define GPIOC_BASE      0x40011000UL
#define GPIOC_CRH       (*(volatile uint32_t*)(GPIOC_BASE + 0x04)) /* PC8–PC15 */
#define GPIOC_ODR       (*(volatile uint32_t*)(GPIOC_BASE + 0x0C)) /* Output Data Register */
/*
 * PC13 is in CRH. Bit position = (13 - 8) * 4 = bit 20
 * ODR bit 13: write 0 = LED ON (active LOW), write 1 = LED OFF
 */


/* ============================================================================
 * USART1 — Universal Synchronous/Asynchronous Receiver/Transmitter
 * ============================================================================
 * We use it in async mode (standard serial UART):
 *   PA9  = TX (we send data to PC)
 *   PA10 = RX (we receive commands from pygame)
 *   8 data bits, no parity, 1 stop bit (8N1) — the default
 *   Baud rate 115200
 *
 * Key registers:
 *   SR  = Status Register     → read to check if TX empty, RX has data
 *   DR  = Data Register       → write to transmit, read to receive
 *   BRR = Baud Rate Register  → controls speed
 *   CR1 = Control Register 1  → enable UART, TX, RX
 */
#define USART1_BASE     0x40013800UL
#define USART1_SR       (*(volatile uint32_t*)(USART1_BASE + 0x00))
#define USART1_DR       (*(volatile uint32_t*)(USART1_BASE + 0x04))
#define USART1_BRR      (*(volatile uint32_t*)(USART1_BASE + 0x08))
#define USART1_CR1      (*(volatile uint32_t*)(USART1_BASE + 0x0C))

/* Status Register bits we care about */
#define USART_SR_TXE    (1UL << 7)  /* TX Data Register Empty — safe to write next byte */
#define USART_SR_RXNE   (1UL << 5)  /* RX Data Register Not Empty — new byte ready    */
/*
 * How sending works:
 *   1. Wait until TXE = 1  (previous byte fully loaded into shift register)
 *   2. Write byte to DR    (hardware automatically shifts it out serially)
 *   3. Repeat
 *
 * How receiving works:
 *   1. Poll RXNE bit       (or use interrupt — we poll for simplicity)
 *   2. When RXNE = 1, read DR  → clears RXNE automatically
 */

/* Control Register 1 bits */
#define USART_CR1_UE    (1UL << 13) /* USART Enable      */
#define USART_CR1_TE    (1UL << 3)  /* Transmitter Enable */
#define USART_CR1_RE    (1UL << 2)  /* Receiver Enable    */


/* ============================================================================
 * I2C1 — Inter-Integrated Circuit bus
 * ============================================================================
 * Two-wire serial bus: SCL (clock) and SDA (data).
 * Used to communicate with MPU6050.
 * PB6 = SCL, PB7 = SDA.
 *
 * I2C protocol overview:
 *   ┌────────────────────────────────────────────────────────┐
 *   │ START | ADDR+W | REG_ADDR | DATA | DATA | ... | STOP  │  (write)
 *   │ START | ADDR+W | REG_ADDR | START | ADDR+R | DATA... NACK STOP │ (read)
 *   └────────────────────────────────────────────────────────┘
 *
 * START:  SDA goes LOW while SCL is HIGH  (master claims the bus)
 * STOP:   SDA goes HIGH while SCL is HIGH (master releases the bus)
 * ADDR:   7-bit device address + 1 direction bit (0=write, 1=read)
 * ACK:    receiver pulls SDA LOW after each byte ("I got it, send more")
 * NACK:   receiver leaves SDA HIGH after last byte ("stop sending")
 */
#define I2C1_BASE       0x40005400UL
#define I2C1_CR1        (*(volatile uint32_t*)(I2C1_BASE + 0x00))
#define I2C1_CR2        (*(volatile uint32_t*)(I2C1_BASE + 0x04))
#define I2C1_CCR        (*(volatile uint32_t*)(I2C1_BASE + 0x1C))
#define I2C1_TRISE      (*(volatile uint32_t*)(I2C1_BASE + 0x20))
#define I2C1_SR1        (*(volatile uint32_t*)(I2C1_BASE + 0x14))
#define I2C1_SR2        (*(volatile uint32_t*)(I2C1_BASE + 0x18))
#define I2C1_DR         (*(volatile uint32_t*)(I2C1_BASE + 0x10))

/* I2C CR1 bits */
#define I2C_CR1_PE      (1UL << 0)  /* Peripheral Enable                    */
#define I2C_CR1_START   (1UL << 8)  /* Generate START condition              */
#define I2C_CR1_STOP    (1UL << 9)  /* Generate STOP condition               */
#define I2C_CR1_ACK     (1UL << 10) /* Send ACK after each received byte     */

/* I2C SR1 bits — we poll these to know when each step completes */
#define I2C_SR1_SB      (1UL << 0)  /* Start Bit generated (START sent OK)  */
#define I2C_SR1_ADDR    (1UL << 1)  /* Address sent + acknowledged by slave */
#define I2C_SR1_BTF     (1UL << 2)  /* Byte Transfer Finished               */
#define I2C_SR1_RXNE    (1UL << 6)  /* Receive buffer not empty (byte ready)*/
#define I2C_SR1_TXE     (1UL << 7)  /* Transmit buffer empty (send next)    */


/* ============================================================================
 * SysTick — ARM Cortex-M core timer
 * ============================================================================
 * Every ARM chip has this timer built-in (not STM32-specific).
 * It's a 24-bit countdown timer. When it reaches 0, it fires an interrupt
 * and reloads from the LOAD register.
 *
 * We configure it to fire every 1ms → our "heartbeat" for timing everything.
 *
 * At 8MHz:  8,000,000 ticks/second → 8000 ticks = exactly 1ms
 * LOAD = 8000 - 1 = 7999  (counts 7999 → 7998 → ... → 1 → 0 → interrupt → reload)
 */
#define SYSTICK_BASE    0xE000E010UL
#define SYSTICK_CTRL    (*(volatile uint32_t*)(SYSTICK_BASE + 0x00))
#define SYSTICK_LOAD    (*(volatile uint32_t*)(SYSTICK_BASE + 0x04))
#define SYSTICK_VAL     (*(volatile uint32_t*)(SYSTICK_BASE + 0x08))


/* ============================================================================
 * MPU6050 INTERNAL REGISTER ADDRESSES
 * ============================================================================
 * These are NOT STM32 memory addresses.
 * These are register numbers INSIDE the MPU6050 chip.
 * We send them over I2C to tell the MPU6050 which register to read/write.
 *
 * Full register map: MPU6050 Product Specification Rev 3.4, Section 4
 */
#define MPU6050_ADDR        0x68  /* I2C address when AD0 = GND */
                                  /* If AD0 = VCC, address = 0x69 */

#define MPU_REG_PWR_MGMT_1  0x6B  /* Power Management 1
                                   * Default = 0x40 (sleep mode, clock = internal 8MHz)
                                   * Write 0x00 → wake up, use internal oscillator */

#define MPU_REG_SMPLRT_DIV  0x19  /* Sample Rate Divider
                                   * Sample Rate = Gyro Rate / (1 + SMPLRT_DIV)
                                   * Gyro Rate = 1kHz when DLPF enabled
                                   * Write 0 → no division → 1kHz sample rate */

#define MPU_REG_CONFIG      0x1A  /* Configuration — Digital Low Pass Filter (DLPF)
                                   * DLPF_CFG = 3 → accel BW=44Hz, gyro BW=42Hz
                                   * This filters out high-freq motor vibration noise
                                   * at the cost of ~4.9ms latency — acceptable */

#define MPU_REG_GYRO_CONFIG 0x1B  /* Gyroscope Configuration
                                   * FS_SEL bits [4:3]:
                                   *   00 = ±250°/s   → sensitivity 131  LSB/°/s
                                   *   01 = ±500°/s   → sensitivity 65.5 LSB/°/s
                                   *   10 = ±1000°/s  → sensitivity 32.8 LSB/°/s
                                   *   11 = ±2000°/s  → sensitivity 16.4 LSB/°/s
                                   * We use 00 (most sensitive, good for slow hover) */

#define MPU_REG_ACCEL_CONFIG 0x1C /* Accelerometer Configuration
                                   * AFS_SEL bits [4:3]:
                                   *   00 = ±2g    → sensitivity 16384 LSB/g
                                   *   01 = ±4g    → sensitivity  8192 LSB/g
                                   *   10 = ±8g    → sensitivity  4096 LSB/g
                                   *   11 = ±16g   → sensitivity  2048 LSB/g
                                   * We use 00 (most sensitive) */

#define MPU_REG_ACCEL_XOUT_H 0x3B /* First data register
                                    * 14 consecutive bytes from here:
                                    * [0x3B-0x3C] ACCEL_X high,low
                                    * [0x3D-0x3E] ACCEL_Y high,low
                                    * [0x3F-0x40] ACCEL_Z high,low
                                    * [0x41-0x42] TEMP    high,low  (we skip)
                                    * [0x43-0x44] GYRO_X  high,low
                                    * [0x45-0x46] GYRO_Y  high,low
                                    * [0x47-0x48] GYRO_Z  high,low */

/* Sensitivity scale factors (from datasheet, matching our FS_SEL=0, AFS_SEL=0) */
#define GYRO_SCALE    131.0f    /* raw / 131.0  = degrees/second  */
#define ACCEL_SCALE   16384.0f  /* raw / 16384.0 = g (1g = 9.81 m/s²) */


/* ============================================================================
 * SECTION 3 — GLOBAL VARIABLES
 * ============================================================================ */

volatile uint32_t ms_ticks = 0;
/*
 * 'volatile' is critical here.
 * This variable is modified inside SysTick_Handler() (an interrupt routine).
 * The main loop reads it continuously.
 *
 * Without volatile: the compiler sees ms_ticks never changes in main(),
 * so it optimizes: loads it once into a CPU register and never re-reads RAM.
 * Result: ms_ticks looks frozen = delay_ms() loops forever.
 *
 * With volatile: every read must go to actual RAM address, not a register.
 * The interrupt can update RAM and the main loop will see the updated value.
 */

/* Raw 16-bit ADC values directly from MPU6050 registers */
int16_t raw_ax, raw_ay, raw_az;   /* accelerometer: raw counts, range ±32768 */
int16_t raw_gx, raw_gy, raw_gz;   /* gyroscope:     raw counts, range ±32768 */

/* Physical unit values (converted from raw) */
float accel_x, accel_y, accel_z;  /* in g  (1g = 9.81 m/s²)  */
float gyro_x,  gyro_y,  gyro_z;   /* in °/s (degrees/second)  */

/* Estimated attitude */
float roll_angle = 0.0f;
/*
 * Current roll angle estimate from complementary filter.
 * 0°  = level (drone horizontal)
 * +ve = right side tilting down
 * -ve = left side tilting down
 * Range: -180° to +180° (but drone should stay within ±90°)
 */

/* PID state */
float setpoint   = 0.0f;   /* desired roll angle (0° = fly level) */
float pid_output = 0.0f;   /* final PID output → motor torque command */
float p_term     = 0.0f;   /* P contribution this loop (saved for telemetry) */
float i_term     = 0.0f;   /* I accumulator — PERSISTS between loops         */
float d_term     = 0.0f;   /* D contribution this loop (saved for telemetry) */
float last_error = 0.0f;   /* error from previous loop (needed for D term)   */

/* PID gain values — tunable live from pygame */
float Kp = 1.2f;    /* Proportional gain */
float Ki = 0.01f;   /* Integral gain     */
float Kd = 0.4f;    /* Derivative gain   */

/* Loop timing */
float dt = 0.001f;  /* seconds between loops — starts at 1ms, measured each loop */


/* ============================================================================
 * SECTION 4 — CUSTOM MATH (no <math.h>)
 * ============================================================================
 * We only need three functions: abs, sqrt, atan2.
 * Writing them ourselves keeps the binary small and the build simple.
 * No math library linking required.
 */

static float f_abs(float x) {
    return (x < 0.0f) ? -x : x;
    /*
     * Ternary operator: (condition) ? value_if_true : value_if_false
     * Same as:
     *   if (x < 0.0f) return -x;
     *   else          return  x;
     */
}

static float f_sqrt(float n) {
    /*
     * Newton-Raphson square root iteration.
     * Start with guess x = n.
     * Repeatedly improve: x_new = (x + n/x) / 2
     * This converges to sqrt(n) very quickly (doubles correct digits each step).
     *
     * Example: sqrt(2.0)
     *   x=2.0: x=(2.0+1.0)/2=1.5
     *   x=1.5: x=(1.5+1.333)/2=1.4167
     *   x=1.4167: x≈1.4142  ← correct to 4 decimal places after 3 steps
     */
    if (n <= 0.0f) return 0.0f;
    float x = n, y = 1.0f;
    while (f_abs(x - y) > 0.0001f) {
        x = (x + y) * 0.5f;   /* average */
        y = n / x;             /* Newton step */
    }
    return x;
}

static float f_atan2(float y, float x) {
    /*
     * Approximation of atan2(y, x) in radians.
     * Returns the angle (in radians) of the vector (x, y) from the +X axis.
     * Range: -π to +π
     *
     * We use Rajan's polynomial approximation (max error ≈ 0.005 rad = 0.28°).
     * Accurate enough for attitude estimation on a drone.
     *
     * Why do we need atan2?
     *   When drone rolls, gravity vector projects onto Y and Z axes.
     *   Roll = atan2(accel_y, accel_z)
     *   At 0° roll:  accel_y=0,   accel_z=1g → atan2(0,1)   = 0°  ✓
     *   At 30° roll: accel_y=0.5, accel_z=0.87 → atan2(0.5,0.87) = 30° ✓
     */
    float abs_y = f_abs(y) + 1e-10f;   /* tiny offset prevents division by zero */
    float r, angle;

    if (x >= 0.0f) {
        r = (x - abs_y) / (x + abs_y);
        angle = 0.7854f + (-0.0742f + 0.2448f * r) * r;
        /* 0.7854 = π/4 radians = 45° */
    } else {
        r = (x + abs_y) / (abs_y - x);
        angle = 2.3562f + (-0.0742f + 0.2448f * r) * r;
        /* 2.3562 = 3π/4 radians = 135° */
    }
    return (y < 0.0f) ? -angle : angle;
    /* mirror sign of y to get correct quadrant */
}

static float rad_to_deg(float r) {
    return r * 57.2957795f;
    /* 180/π = 57.2957... : multiply radians by this to get degrees */
}


/* ============================================================================
 * SECTION 5 — SYSTICK SETUP AND ISR
 * ============================================================================ */

static void systick_init(void) {
    /*
     * Configure SysTick for 1ms interrupt at 8MHz HSI clock.
     *
     * LOAD register = (clock_freq / desired_freq) - 1
     *              = (8,000,000 / 1,000) - 1
     *              = 7999
     *
     * Why -1? The timer counts: LOAD → LOAD-1 → ... → 1 → 0 → INTERRUPT → reload LOAD
     * So it counts LOAD+1 values = LOAD+1 ticks = 8000 ticks = exactly 1ms.
     */
    SYSTICK_LOAD = 8000 - 1;    /* = 7999 */
    SYSTICK_VAL  = 0;            /* clear current counter (start fresh) */

    /*
     * CTRL register bits:
     *   bit 0 = ENABLE   → start the counter
     *   bit 1 = TICKINT  → enable interrupt when counter reaches 0
     *   bit 2 = CLKSOURCE → 1 = use processor clock (8MHz HSI)
     *                       0 = use external clock / 8 = 1MHz (too slow for 1ms)
     */
    SYSTICK_CTRL = (1UL << 0) | (1UL << 1) | (1UL << 2);
}

void SysTick_Handler(void) {
    /*
     * This function is called AUTOMATICALLY by the ARM Cortex-M hardware
     * every time SysTick counter hits 0 (every 1ms).
     *
     * The name "SysTick_Handler" is the exact symbol ARM's vector table looks for.
     * The vector table is a list of function pointers at the start of flash.
     * Entry at offset 0x3C = SysTick exception = this function.
     *
     * All we do: count milliseconds.
     * The main loop uses ms_ticks for delay_ms() and telemetry timing.
     */
    ms_ticks++;
}

static void delay_ms(uint32_t ms) {
    uint32_t start = ms_ticks;
    while ((ms_ticks - start) < ms);
    /*
     * Busy-wait loop. CPU spins doing nothing until time passes.
     * Not efficient, but simple and fine for initialization.
     *
     * The subtraction (ms_ticks - start) is safe even when ms_ticks wraps
     * around at 4,294,967,295 back to 0 — unsigned overflow is defined in C.
     */
}

static uint32_t get_micros(void) {
    /*
     * Returns approximate current time in microseconds.
     * Used to measure precise dt between loops.
     *
     * SysTick counts DOWN from 7999 to 0 each millisecond.
     * SYSTICK_VAL = how many ticks remain in the current millisecond.
     * Elapsed ticks since start of this ms = 7999 - SYSTICK_VAL
     * Each tick = 1/8 µs at 8MHz → divide by 8 to get µs
     *
     * Total µs = ms_ticks × 1000  +  (7999 - SYSTICK_VAL) / 8
     */
    uint32_t ms  = ms_ticks;
    uint32_t val = SYSTICK_VAL;
    return (ms * 1000UL) + ((8000UL - 1UL - val) / 8UL);
}


/* ============================================================================
 * SECTION 6 — GPIO CONFIGURATION
 * ============================================================================ */

static void gpio_init(void) {
    /*
     * Step 1: Enable clocks for all GPIO ports we use, plus AFIO.
     *
     * |= means "OR into the register" — sets the specified bits to 1
     * without changing bits we don't mention.
     * This is important: other bits may already be set by startup code.
     */
    RCC_APB2ENR |= RCC_APB2_AFIOEN   /* AFIO  — needed for USART1 AF pins */
                 | RCC_APB2_IOPAEN   /* GPIOA — PA9/PA10 for UART */
                 | RCC_APB2_IOPBEN   /* GPIOB — PB6/PB7 for I2C  */
                 | RCC_APB2_IOPCEN   /* GPIOC — PC13 for LED      */
                 | RCC_APB2_USART1EN;/* USART1 — our serial port  */

    /* ------------------------------------------------------------------
     * PA9 = USART1_TX
     * ------------------------------------------------------------------
     * PA9 is pin 9, in CRH register (covers PA8–PA15).
     * Bit position in CRH = (9 - 8) × 4 = 4
     *
     * We want: Alternate Function Push-Pull, 50MHz
     *   MODE = 11 (50MHz output)
     *   CNF  = 10 (AF push-pull)
     *   Combined 4-bit value = CNF[1:0] | MODE[1:0] = 1011 binary = 0xB
     *
     * Step: clear the 4 bits, then set them to 0xB
     */
    GPIOA_CRH &= ~(0xFUL << 4);   /* clear bits [7:4]  (0xF = 0b1111) */
    GPIOA_CRH |=  (0xBUL << 4);   /* set  bits [7:4] to 1011 = 0xB   */
    /*
     * Why 50MHz? Not because we send data at 50MHz.
     * This is the GPIO driver's slew rate — how fast the output can change.
     * Higher slew rate = cleaner edges at 115200 baud. 2MHz is fine too,
     * but 50MHz costs nothing at this power level.
     */

    /* ------------------------------------------------------------------
     * PA10 = USART1_RX
     * ------------------------------------------------------------------
     * PA10 bit position in CRH = (10 - 8) × 4 = 8
     *
     * Floating input: MODE = 00, CNF = 01 → 0100 binary = 0x4
     * The USB-TTL adapter drives this line, so floating is fine.
     * (Pull-up 0x08 also works if the line might be unconnected)
     */
    GPIOA_CRH &= ~(0xFUL << 8);
    GPIOA_CRH |=  (0x4UL << 8);

    /* ------------------------------------------------------------------
     * PB6 = I2C1_SCL
     * PB7 = I2C1_SDA
     * ------------------------------------------------------------------
     * Both must be Alternate Function Open-Drain.
     *
     * OPEN-DRAIN is mandatory for I2C because:
     *   - Both STM32 (master) and MPU6050 (slave) can pull the line LOW
     *   - Only the external 4.7kΩ pull-up resistor pulls it HIGH
     *   - Push-pull would fight: if one device drives HIGH and another LOW
     *     simultaneously, you get a short circuit → damage or data corruption
     *   - With open-drain: "driving LOW" = turn on transistor to GND
     *                      "releasing"   = turn off transistor, pull-up takes over
     *
     * AF open-drain: MODE = 11 (50MHz), CNF = 11 → 1111 binary = 0xF
     *
     * PB6 bit position in CRL = 6 × 4 = 24
     * PB7 bit position in CRL = 7 × 4 = 28
     */
    GPIOB_CRL &= ~(0xFFUL << 24);  /* clear 8 bits covering PB6 [27:24] and PB7 [31:28] */
    GPIOB_CRL |=  (0xFFUL << 24);  /* set both to 0xF (1111 = AF open-drain 50MHz) */

    /* ------------------------------------------------------------------
     * PC13 = Onboard LED (active LOW)
     * ------------------------------------------------------------------
     * PC13 bit position in CRH = (13 - 8) × 4 = 20
     *
     * General purpose push-pull output, 2MHz (slow is fine for LED):
     * MODE = 10 (2MHz), CNF = 00 → 0010 binary = 0x2
     */
    GPIOC_CRH &= ~(0xFUL << 20);
    GPIOC_CRH |=  (0x2UL << 20);

    /* Start with LED OFF (active LOW → set bit = OFF) */
    GPIOC_ODR |= (1UL << 13);
}


/* ============================================================================
 * SECTION 7 — USART1 INITIALIZATION (PA9 TX, PA10 RX)
 * ============================================================================ */

static void uart_init(void) {
    /*
     * RCC clock for USART1 was already enabled in gpio_init() via APB2ENR.
     *
     * Set baud rate:
     *   BRR = f_clock / baud_rate
     *       = 8,000,000 / 115,200
     *       = 69.44...
     *       ≈ 69  (we truncate — 0.6% error, well within UART 3% tolerance)
     *
     * The BRR register has two parts:
     *   bits [15:4] = mantissa  (integer part)
     *   bits  [3:0] = fraction  (fractional part × 16)
     *
     * For integer-only: BRR = 69 = 0x0045
     * (69 × 16 + 0 = 1104 in raw BRR units, but 0x0045 in mantissa = 69×16=1104)
     * Actually simpler: just write 69 decimal = 0x45:
     *   mantissa = 69 >> 0 = 69, fraction = 0
     *   This gives BRR = (69 << 4) | 0 = 0x0450... no.
     *
     * Correct calculation:
     *   BRR integer = 8000000 / 115200 = 69 (integer division)
     *   No fractional part (good enough at 8MHz)
     *   Write BRR = 69 = 0x45
     *   The mantissa field [15:4] will hold 69/1 = 4 (69 = 4×16+5)... 
     *
     * Simplest and correct: BRR = (mantissa << 4) | fraction
     *   mantissa = 69/1 = 4  ... wait.
     *
     * CORRECT: BRR register value = f_PCLK / (16 × baud) × 16
     *                              = f_PCLK / baud (simplified for standard mode)
     *                              = 8000000 / 115200 = 69
     * So write 69 directly. BRR = 0x0045.
     * The hardware uses BRR[15:4] as integer, BRR[3:0] as fraction.
     * 69 in hex = 0x45 = binary 0100 0101
     * BRR[15:4] = 0x4 = 4 (mantissa), BRR[3:0] = 0x5 = 5 (fraction=5/16)
     * Actual baud = 8000000 / (16 × (4 + 5/16)) = 8000000 / 69 = 115942 (0.6% off) ✓
     */
    USART1_BRR = 0x0045;  /* 69 decimal = 115200 baud at 8MHz HSI */

    /*
     * CR1: enable USART, transmitter, and receiver simultaneously.
     * All other CR1 bits default to 0 = 8 data bits, no parity, 1 stop bit.
     */
    USART1_CR1 = USART_CR1_UE | USART_CR1_TE | USART_CR1_RE;
}

static void uart_send_byte(uint8_t b) {
    while (!(USART1_SR & USART_SR_TXE));
    /*
     * Spin here until TXE = 1.
     * TXE = 1 means: the shift register has loaded the previous byte and
     * the DR is now empty → safe to write the next byte.
     *
     * TXE does NOT mean the byte has fully transmitted yet —
     * it means the hardware FIFO (1 byte deep) is ready for the next one.
     * This lets us keep the UART busy 100% of the time.
     */
    USART1_DR = b;
    /* Writing DR: hardware automatically serializes the bits at 115200 baud. */
}

static void uart_send_str(const char* s) {
    while (*s) {
        uart_send_byte((uint8_t)*s);
        s++;
    }
    /*
     * *s  = dereference pointer = get the char at current address
     * s++ = move pointer forward one byte (one char)
     * Loop exits when *s == 0 (null terminator '\0' ends every C string)
     */
}

static uint8_t uart_available(void) {
    return (USART1_SR & USART_SR_RXNE) ? 1 : 0;
    /* RXNE = 1 means a full byte has been received and is waiting in DR */
}

static uint8_t uart_read(void) {
    return (uint8_t)(USART1_DR & 0xFF);
    /* Reading DR automatically clears the RXNE flag */
}

static void uart_send_int(int32_t n) {
    /*
     * Convert integer to ASCII decimal string and send.
     * We avoid printf/sprintf (needs full C library).
     *
     * Algorithm:
     *   1. If negative, send '-' and negate
     *   2. Extract digits right-to-left using modulo 10
     *   3. Store in buffer (they're reversed)
     *   4. Send buffer in reverse order
     *
     * Example: n = -314
     *   send '-', n = 314
     *   buf[0] = '0' + 314%10 = '4'
     *   buf[1] = '0' + 31%10  = '1'
     *   buf[2] = '0' + 3%10   = '3'
     *   send buf[2],buf[1],buf[0] = "314"
     *   total sent: "-314"
     */
    if (n < 0) { uart_send_byte('-'); n = -n; }
    if (n == 0) { uart_send_byte('0'); return; }
    char buf[12];
    int  i = 0;
    while (n > 0) {
        buf[i++] = (char)('0' + (n % 10));
        n /= 10;
    }
    for (int j = i - 1; j >= 0; j--)
        uart_send_byte((uint8_t)buf[j]);
}

static void uart_send_float(float f) {
    /*
     * We send floats as integer × 100 to avoid float-to-string conversion.
     * Example: 15.23 → send "1523"
     * Pygame receives "1523" and divides by 100.0 to get 15.23.
     *
     * Handles negatives correctly: -3.14 → int(-314) → "-314"
     *
     * Precision: 2 decimal places (centiseconds, centidegrees — enough for PID)
     */
    uart_send_int((int32_t)(f * 100.0f));
}


/* ============================================================================
 * SECTION 8 — I2C1 INITIALIZATION
 * ============================================================================ */

static void i2c_init(void) {
    /* Clock was already enabled in gpio_init() via APB1ENR in... wait.
     * Actually I2C1 is APB1. Let's enable it here explicitly. */
    RCC_APB1ENR |= RCC_APB1_I2C1EN;

    /* Software reset — clears any stuck "BUSY" state.
     * I2C can get stuck if power was cut mid-transaction.
     * Set SWRST=1 then clear it to do a clean reset.
     */
    I2C1_CR1 |=  (1UL << 15);  /* SWRST = 1: peripheral held in reset */
    I2C1_CR1 &= ~(1UL << 15);  /* SWRST = 0: release from reset       */

    /*
     * CR2 bits [5:0] = FREQ: APB1 bus frequency in MHz.
     * This does NOT set the I2C speed — it just tells I2C what the APB1 clock is
     * so it can compute timing correctly.
     * APB1 = 8MHz on Blue Pill (HSI, no prescaler).
     */
    I2C1_CR2 = 8;  /* 8 MHz */

    /*
     * CCR = Clock Control Register — sets SCL frequency.
     *
     * Standard mode (Sm, 100kHz):
     *   SCL period = 2 × CCR × T_APB1
     *   100kHz → period = 10µs
     *   CCR = 10µs / (2 × (1/8MHz)) = 10µs / 0.25µs = 40
     *
     * CCR[15] = 0 → Standard mode (100kHz max)
     * CCR[15] = 1 → Fast mode (400kHz max, needs different formula)
     */
    I2C1_CCR = 40;  /* 100kHz I2C bus speed */

    /*
     * TRISE = Maximum Rise Time register.
     * I2C spec: SDA/SCL rise time ≤ 1000ns in standard mode.
     * TRISE = (max_rise_ns × f_APB1_MHz) + 1
     *       = (1000 × 10⁻⁹ × 8 × 10⁶) + 1
     *       = 8 + 1 = 9
     *
     * This tells the I2C peripheral how long to wait for the line to rise
     * before sampling — prevents false reads when signal is still rising.
     */
    I2C1_TRISE = 9;

    /* Enable the I2C1 peripheral */
    I2C1_CR1 = I2C_CR1_PE;
}


/* ============================================================================
 * SECTION 9 — I2C BUS OPERATIONS (primitive building blocks)
 * ============================================================================
 *
 * These low-level functions implement the I2C physical protocol steps.
 * Each function does exactly one step and waits for hardware confirmation.
 *
 * Hardware confirmation pattern:
 *   1. Tell hardware what to do (write to register)
 *   2. Poll status flag until hardware says it's done
 *   3. Continue
 *
 * This polling approach is simple but wastes CPU cycles waiting.
 * A production firmware would use DMA + interrupts — but for learning,
 * polling makes each step explicit and easy to debug.
 */

static void i2c_start(void) {
    I2C1_CR1 |= I2C_CR1_START;
    /*
     * Setting START bit tells I2C hardware to generate a START condition:
     * SDA goes LOW while SCL is still HIGH.
     * This signals all slaves on the bus: "a transaction is starting."
     */
    while (!(I2C1_SR1 & I2C_SR1_SB));
    /*
     * SB (Start Bit) flag is set by hardware when START was sent successfully.
     * We must wait for SB=1 before sending the address.
     * Reading SR1 (done by the while condition) is part of clearing the SB flag.
     */
}

static void i2c_stop(void) {
    I2C1_CR1 |= I2C_CR1_STOP;
    /*
     * STOP condition: SDA goes HIGH while SCL is HIGH.
     * Signals all slaves: "transaction complete, bus is free."
     * Hardware generates STOP after finishing the current byte.
     * No flag to wait for — the main code just continues.
     */
}

static void i2c_send_addr_write(uint8_t addr) {
    I2C1_DR = (uint32_t)((addr << 1) | 0);
    /*
     * I2C address byte format: [A6:A5:A4:A3:A2:A1:A0 | R/W]
     * addr = 7-bit device address (e.g. 0x68 for MPU6050)
     * (addr << 1) shifts it left by 1 to make room for R/W bit at bit 0
     * | 0 = R/W = 0 = WRITE mode
     *
     * MPU6050: addr=0x68, write → DR = 0xD0
     */
    while (!(I2C1_SR1 & I2C_SR1_ADDR));
    /*
     * ADDR flag = 1 when:
     *   - The address byte was sent
     *   - The slave sent an ACK (confirmed it received its address)
     * If slave is not present or wrong address: ADDR never sets → infinite loop.
     * In production: add timeout. For learning: works if wiring is correct.
     */
    (void)I2C1_SR2;
    /*
     * CRITICAL: ADDR flag is cleared by reading SR1 then SR2 in sequence.
     * We already read SR1 in the while() condition above.
     * Now we MUST read SR2 to finish clearing ADDR.
     * Casting to (void) tells compiler "yes, I'm intentionally discarding this value."
     * If you skip this, ADDR stays set and I2C gets stuck.
     */
}

static void i2c_send_addr_read(uint8_t addr) {
    I2C1_DR = (uint32_t)((addr << 1) | 1);  /* R/W = 1 = READ mode */
    while (!(I2C1_SR1 & I2C_SR1_ADDR));
    (void)I2C1_SR2;
}

static void i2c_write_byte(uint8_t data) {
    while (!(I2C1_SR1 & I2C_SR1_TXE));
    /*
     * TXE = 1 means TX data register is empty → safe to write next byte.
     * Wait here until previous byte was loaded into shift register.
     */
    I2C1_DR = data;
    while (!(I2C1_SR1 & I2C_SR1_BTF));
    /*
     * BTF = Byte Transfer Finished.
     * Wait until the byte has been fully shifted out AND the slave sent ACK.
     * After BTF=1, we know the byte was received successfully.
     */
}

static uint8_t i2c_read_ack(void) {
    I2C1_CR1 |= I2C_CR1_ACK;           /* enable ACK → tells slave to keep sending */
    while (!(I2C1_SR1 & I2C_SR1_RXNE));/* wait for byte to arrive in DR             */
    return (uint8_t)(I2C1_DR & 0xFF);  /* reading DR also clears RXNE               */
}

static uint8_t i2c_read_nack(void) {
    I2C1_CR1 &= ~I2C_CR1_ACK;          /* disable ACK → sends NACK = "last byte"   */
    i2c_stop();                          /* send STOP right after receiving this byte */
    while (!(I2C1_SR1 & I2C_SR1_RXNE));
    return (uint8_t)(I2C1_DR & 0xFF);
}


/* ============================================================================
 * SECTION 10 — MPU6050 REGISTER ACCESS
 * ============================================================================ */

static void mpu_write_reg(uint8_t reg, uint8_t val) {
    /*
     * I2C write sequence:
     *   START → [MPU_ADDR + W] → [register number] → [value] → STOP
     */
    i2c_start();
    i2c_send_addr_write(MPU6050_ADDR);
    i2c_write_byte(reg);   /* tell MPU: "I want to write register 0xXX" */
    i2c_write_byte(val);   /* the new value for that register            */
    i2c_stop();
}

static void mpu_read_bytes(uint8_t reg, uint8_t *buf, uint8_t len) {
    /*
     * I2C read sequence (write-then-read with repeated START):
     *
     *   Phase 1 (WRITE — tell MPU which register to start reading from):
     *     START → [MPU_ADDR + W] → [register number]
     *
     *   Phase 2 (READ — switch to read mode, receive bytes):
     *     repeated START → [MPU_ADDR + R] → byte0 ACK → byte1 ACK → ... → last byte NACK STOP
     *
     * The MPU6050 auto-increments its register pointer after each byte read,
     * so we only need to specify the starting register once.
     * Reading from 0x3B with len=14 gives us all sensor data in one transaction.
     */

    /* Phase 1: set register pointer */
    i2c_start();
    i2c_send_addr_write(MPU6050_ADDR);
    i2c_write_byte(reg);

    /* Phase 2: repeated START, switch to read */
    i2c_start();  /* repeated START — no STOP between write and read phases */
    i2c_send_addr_read(MPU6050_ADDR);

    for (uint8_t i = 0; i < len; i++) {
        if (i == len - 1)
            buf[i] = i2c_read_nack();  /* last byte: send NACK + STOP */
        else
            buf[i] = i2c_read_ack();   /* all others: send ACK (keep going) */
    }
}


/* ============================================================================
 * SECTION 11 — MPU6050 INITIALIZATION
 * ============================================================================ */

static void mpu_init(void) {
    delay_ms(150);
    /*
     * MPU6050 datasheet says: after power-on, wait at least 100ms before
     * trying to communicate. The chip runs its internal startup sequence.
     * Using 150ms for safety margin.
     */

    /* Wake up: clear sleep bit */
    mpu_write_reg(MPU_REG_PWR_MGMT_1, 0x00);
    /*
     * Power Management Register 1 (0x6B):
     * Default value = 0x40 (bit 6 = SLEEP = 1 → chip is sleeping)
     * Writing 0x00 clears SLEEP → chip wakes up
     * Also sets CLKSEL = 0 = internal 8MHz oscillator
     * (For better stability, you could set CLKSEL=1 to use gyro X as clock,
     *  but internal oscillator is fine for learning)
     */
    delay_ms(10);

    /* Set sample rate */
    mpu_write_reg(MPU_REG_SMPLRT_DIV, 0x00);
    /*
     * Sample Rate = Gyro Output Rate / (1 + SMPLRT_DIV)
     * With DLPF enabled: Gyro Output Rate = 1kHz
     * SMPLRT_DIV = 0 → Sample Rate = 1000/(1+0) = 1000Hz = 1kHz
     * Our loop runs at 1kHz → every loop gets a fresh sensor reading.
     */

    /* Configure DLPF (Digital Low Pass Filter) */
    mpu_write_reg(MPU_REG_CONFIG, 0x03);
    /*
     * CONFIG register bits [2:0] = DLPF_CFG
     * Value 3:
     *   Accelerometer: bandwidth = 44Hz, delay = 4.9ms
     *   Gyroscope:     bandwidth = 42Hz, delay = 4.8ms
     *
     * DLPF removes high-frequency noise (motor vibrations, electrical noise)
     * while preserving the slower attitude changes we care about.
     *
     * The 4.8ms latency means: sensor reading lags reality by 4.8ms.
     * At 1kHz loop rate, this is ~5 loops of lag.
     * For a slow-flying drone: totally acceptable.
     * For racing drone: use DLPF_CFG=0 (no filter, less lag, more noise).
     */

    /* Configure gyroscope full-scale range */
    mpu_write_reg(MPU_REG_GYRO_CONFIG, 0x00);
    /*
     * FS_SEL = 0 → ±250°/s range
     * This is the most sensitive setting.
     * A drone rarely rotates faster than 250°/s in normal flight.
     * If you're building an acro/racing drone, use FS_SEL=2 (±1000°/s).
     */

    /* Configure accelerometer full-scale range */
    mpu_write_reg(MPU_REG_ACCEL_CONFIG, 0x00);
    /*
     * AFS_SEL = 0 → ±2g range
     * Most sensitive. Gravity is 1g, and a drone in normal flight
     * doesn't experience more than 2-3g.
     */

    delay_ms(100);  /* let filter settle after configuration */
}


/* ============================================================================
 * SECTION 12 — READ SENSOR DATA
 * ============================================================================ */

static void imu_read(void) {
    uint8_t buf[14];
    /*
     * 14 bytes in order from MPU6050 starting at register 0x3B:
     * Index: [0][1] = ACCEL_X_H, ACCEL_X_L
     *        [2][3] = ACCEL_Y_H, ACCEL_Y_L
     *        [4][5] = ACCEL_Z_H, ACCEL_Z_L
     *        [6][7] = TEMP_H,    TEMP_L     (we skip this)
     *        [8][9] = GYRO_X_H,  GYRO_X_L
     *       [10][11]= GYRO_Y_H,  GYRO_Y_L
     *       [12][13]= GYRO_Z_H,  GYRO_Z_L
     *
     * Big-endian: HIGH byte comes first, LOW byte second.
     */
    mpu_read_bytes(MPU_REG_ACCEL_XOUT_H, buf, 14);

    /*
     * Reconstruct 16-bit signed integers from two 8-bit bytes.
     *
     * Example: buf[0] = 0x03, buf[1] = 0xE8
     *   (int16_t)((0x03 << 8) | 0xE8)
     *   = (int16_t)(0x0300 | 0x00E8)
     *   = (int16_t)(0x03E8)
     *   = 1000
     *
     * The (int16_t) cast is important — it makes the compiler treat the
     * bit pattern as a signed number, so values above 32767 wrap negative.
     * e.g. 0xFFFF = 65535 as uint16 = -1 as int16
     */
    raw_ax = (int16_t)((buf[0]  << 8) | buf[1]);
    raw_ay = (int16_t)((buf[2]  << 8) | buf[3]);
    raw_az = (int16_t)((buf[4]  << 8) | buf[5]);
    /* buf[6], buf[7] = temperature — not needed, skip */
    raw_gx = (int16_t)((buf[8]  << 8) | buf[9]);
    raw_gy = (int16_t)((buf[10] << 8) | buf[11]);
    raw_gz = (int16_t)((buf[12] << 8) | buf[13]);

    /*
     * Convert raw counts to physical units using sensitivity scale:
     *
     * Accel: raw / 16384.0 = g
     *   e.g. raw_az = 16384 → accel_z = 1.0g (pointing up, measuring gravity)
     *        raw_az = -16384 → accel_z = -1.0g (upside down)
     *
     * Gyro: raw / 131.0 = degrees/second
     *   e.g. raw_gx = 131 → gyro_x = 1.0°/s (rotating very slowly)
     *        raw_gx = 13100 → gyro_x = 100°/s (fast rotation)
     */
    accel_x = (float)raw_ax / ACCEL_SCALE;
    accel_y = (float)raw_ay / ACCEL_SCALE;
    accel_z = (float)raw_az / ACCEL_SCALE;
    gyro_x  = (float)raw_gx / GYRO_SCALE;
    gyro_y  = (float)raw_gy / GYRO_SCALE;
    gyro_z  = (float)raw_gz / GYRO_SCALE;
}


/* ============================================================================
 * SECTION 13 — COMPLEMENTARY FILTER
 * ============================================================================
 *
 * THE PROBLEM: each sensor alone is insufficient.
 *
 * GYROSCOPE alone:
 *   angle += gyro_rate × dt     (numerical integration)
 *   Pros: fast response, accurate for quick movements, unaffected by vibration
 *   Cons: every real gyro has a tiny DC bias (e.g. 0.02°/s when stationary)
 *         Integrating this bias: 0.02 × 3600s = 72° drift per hour
 *         Even a 0.001°/s bias = 3.6° drift per hour — unusable for hovering
 *
 * ACCELEROMETER alone:
 *   roll = atan2(accel_y, accel_z)   (gravity vector direction)
 *   Pros: no drift — gravity is always there pointing down
 *   Cons: during flight, drone accelerates. Accel sees: gravity + motor thrust + vibration
 *         Vibrations from 20,000 RPM motors make the reading very noisy
 *         angle jumps ±5° every few milliseconds
 *
 * COMPLEMENTARY FILTER: blend the two
 *   angle = α × (angle + gyro × dt)  +  (1-α) × accel_angle
 *           └──────────────────────┘    └───────────────────┘
 *           gyro path (fast, drifts)    accel path (slow, stable)
 *
 * α = 0.98 means:
 *   98% of the new angle = old angle corrected by gyro (fast motion tracking)
 *    2% of the new angle = accel reading (prevents long-term drift)
 *
 * Think of it like GPS + inertial navigation:
 *   - IMU: fast, accurate short-term, drifts long-term
 *   - GPS: slow, noisy, but always knows true position
 *   - Blend: takes best of both
 *
 * Time constant of the filter:
 *   τ = α × dt / (1 - α) = 0.98 × 0.001 / 0.02 = 0.049 seconds
 *   This means: a gyro drift error is corrected with a 49ms time constant.
 *   Fast enough to correct drift, slow enough to ignore vibration noise.
 */

static void complementary_filter(void) {
    /* --- Accelerometer angle ---
     *
     * Gravity vector in body frame:
     *   Level drone:      [ax, ay, az] ≈ [0, 0, 1g]
     *   30° right roll:   [ax, ay, az] ≈ [0, 0.5g, 0.866g]
     *
     * Roll angle = atan2(ay, az)
     *   atan2(0, 1)     = 0°     (level)
     *   atan2(0.5, 0.87)= 30°    (rolled right 30°)
     *   atan2(1, 0)     = 90°    (vertical)
     *
     * Note: this formula assumes no pitch (nose level). For full 3D
     * you'd use atan2(ay, sqrt(ax²+az²)) — but for a single-axis test, fine.
     */
    float accel_roll = rad_to_deg(f_atan2(accel_y, accel_z));

    /* --- Gyroscope integration ---
     *
     * gyro_x = roll rate in degrees/second
     * dt = time since last loop in seconds (≈ 0.001s at 1kHz)
     * gyro_delta = how many degrees the drone rotated this loop
     */
    float gyro_delta = gyro_x * dt;

    /* --- Blend ---
     *
     * 0.98 × (angle last loop + rotation this loop) + 0.02 × accel correction
     *
     * Each loop:
     *   - Gyro term brings angle very close to truth (98%)
     *   - Accel term gently pulls angle toward gravity-based estimate (2%)
     *   - Combined: angle tracks real motion AND stays calibrated
     */
    roll_angle = 0.98f * (roll_angle + gyro_delta)
               + 0.02f * accel_roll;
}


/* ============================================================================
 * SECTION 14 — PID CONTROLLER
 * ============================================================================
 *
 * PID in plain English:
 *   P = "I see the error RIGHT NOW — push proportionally"
 *   I = "I remember all past errors — correct for persistent bias"
 *   D = "I see the error CHANGING — brake before it overshoots"
 *
 * Mechanical analogy — trying to hold a door at 90°:
 *   P: push harder the more it's closed. But you'll oscillate (overshoot, bounce).
 *   D: notice it swinging toward you fast → ease up before it hits you.
 *   I: if it keeps drifting slightly despite P — build up slow pressure to fix it.
 *
 * Mathematical definition:
 *   error(t) = setpoint - measured(t)
 *
 *   P(t) = Kp × error(t)
 *
 *   I(t) = Ki × ∫₀ᵗ error(τ)dτ         (integral from start)
 *   In code: I += Ki × error × dt        (discrete approximation)
 *
 *   D(t) = Kd × d/dt [error(t)]
 *   In code: D = Kd × (error - last_error) / dt   (finite difference)
 *
 *   output(t) = P(t) + I(t) + D(t)
 */

static void pid_update(void) {
    float error = setpoint - roll_angle;
    /*
     * error > 0: drone tilted left of setpoint → need to push right (positive torque)
     * error < 0: drone tilted right of setpoint → need to push left (negative torque)
     * error = 0: perfectly level → no correction needed
     */

    /* ---- P term ---- */
    p_term = Kp * error;
    /*
     * Simple and immediate.
     * If error = 10°:  p_term = 1.2 × 10 = 12.0 (units are arbitrary "torque units")
     * If error = 1°:   p_term = 1.2 × 1  =  1.2
     * If error = -5°:  p_term = 1.2 × -5 = -6.0 (opposite direction)
     *
     * Kp alone causes oscillation because:
     *   - Drone tilted right → large positive push left
     *   - Drone swings past level → now error is negative → pushes right
     *   - Repeat → bounces forever (like a spring with no damping)
     */

    /* ---- I term ---- */
    i_term += Ki * error * dt;
    /*
     * Accumulates the integral of error over time.
     * After 1 second of 5° error: i_term += 0.01 × 5 × 1.0 = 0.05
     * After 10 seconds:           i_term ≈ 0.5  (growing slowly)
     * This growing correction eventually overcomes the steady-state error.
     *
     * Why does steady-state error exist with P only?
     *   If there's a constant disturbance (e.g. one motor slightly weaker),
     *   the drone hovers at e.g. 2° off level. At that point:
     *   P output = Kp × 2 = 2.4 (pushing left)
     *   Disturbance = 2.4 exactly (balanced)
     *   Error stays at 2°. P never gets it to 0.
     *   I fixes this: i_term keeps growing until it overcomes the disturbance.
     */

    /* Anti-windup: clamp I term */
    if (i_term >  50.0f) i_term =  50.0f;
    if (i_term < -50.0f) i_term = -50.0f;
    /*
     * Anti-windup prevents "integral windup":
     * If the drone is stuck (e.g. sitting on the ground, motors off),
     * error is large, I keeps accumulating → reaches huge values.
     * When drone finally lifts, I overwhelms P+D → violent overcorrection.
     *
     * Clamping I to ±50 means maximum I contribution = ±50 torque units.
     * Choose the clamp value = maximum reasonable steady-state torque needed.
     */

    /* ---- D term ---- */
    d_term = Kd * (error - last_error) / dt;
    /*
     * (error - last_error) = change in error since last loop
     * Dividing by dt = rate of change in °/s (like a derivative)
     *
     * Example:
     *   last loop: error = 10°
     *   this loop: error = 8°   (getting better, error shrinking)
     *   d_term = 0.4 × (8 - 10) / 0.001 = 0.4 × -2000 = -800
     *   → D applies a NEGATIVE correction → brakes the motion → prevents overshoot
     *
     *   This is why D is called the "derivative" — it reacts to the RATE OF CHANGE,
     *   not the current value.
     *
     * D amplifies noise: if gyro has 0.1° noise → error noise = 0.1°
     *   d_term noise = Kd × 0.1 / 0.001 = 0.4 × 100 = 40 → large!
     *   This is why the MPU6050 DLPF is important — it reduces gyro noise.
     */

    pid_output = p_term + i_term + d_term;

    /* Clamp output to motor command range */
    if (pid_output >  100.0f) pid_output =  100.0f;
    if (pid_output < -100.0f) pid_output = -100.0f;

    last_error = error;
    /* Save error for next loop's D term calculation */
}


/* ============================================================================
 * SECTION 15 — TELEMETRY SENDER
 * ============================================================================
 * Send CSV packet to pygame every 10ms (100 Hz).
 *
 * Format:
 *   "R,<roll100>,<sp100>,<pid100>,<p100>,<i100>,<d100>,<kp100>,<ki100>,<kd100>\n"
 *
 * All float values encoded as integer × 100 (2 decimal places).
 * Pygame splits on commas, divides each field by 100.0 to recover floats.
 *
 * Example output line:
 *   "R,1523,0,842,1440,-82,320,120,1,40\n"
 *   → roll=15.23° setpoint=0° pid=8.42 p=14.40 i=-0.82 d=3.20 Kp=1.20 Ki=0.01 Kd=0.40
 */
static void send_telemetry(void) {
    uart_send_byte('R');           uart_send_byte(',');
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


/* ============================================================================
 * SECTION 16 — COMMAND RECEIVER (from pygame via UART RX on PA10)
 * ============================================================================
 * Non-blocking: reads available bytes, accumulates them in rx_buf[].
 * When '\n' arrives, parses the complete command.
 *
 * Protocol (same format as telemetry — all × 100):
 *   "P120\n"  → Kp = 1.20
 *   "I1\n"    → Ki = 0.01
 *   "D40\n"   → Kd = 0.40
 *   "S1500\n" → setpoint = 15.00°
 *   "S-500\n" → setpoint = -5.00°
 */
static char    rx_buf[16];
static uint8_t rx_pos = 0;

static void receive_cmds(void) {
    while (uart_available()) {
        char c = (char)uart_read();

        if (c == '\n') {
            /* End of command — null-terminate and parse */
            rx_buf[rx_pos] = '\0';

            if (rx_pos >= 2) {
                char    cmd   = rx_buf[0];  /* first char = command type */
                int32_t val   = 0;
                int32_t sign  = 1;
                uint8_t start = 1;

                /* Handle negative values */
                if (rx_pos > 1 && rx_buf[1] == '-') { sign = -1; start = 2; }

                /* Parse decimal integer from remaining chars */
                for (uint8_t i = start; i < rx_pos; i++) {
                    if (rx_buf[i] >= '0' && rx_buf[i] <= '9')
                        val = val * 10 + (rx_buf[i] - '0');
                    /*
                     * '0' in ASCII = 48. '9' = 57.
                     * rx_buf[i] - '0' converts ASCII digit to integer:
                     *   '3' - '0' = 51 - 48 = 3
                     *   '7' - '0' = 55 - 48 = 7
                     */
                }
                val *= sign;

                /* Apply command — all values are × 100 from pygame */
                if (cmd == 'P') { Kp = (float)val / 100.0f; i_term = 0.0f; }
                if (cmd == 'I') { Ki = (float)val / 100.0f; i_term = 0.0f; }
                if (cmd == 'D') { Kd = (float)val / 100.0f; }
                if (cmd == 'S') { setpoint = (float)val / 100.0f; }
                /*
                 * When Kp or Ki changes, we reset i_term to zero.
                 * Why? If you're tuning and suddenly double Kp,
                 * the old accumulated i_term was based on old Kp.
                 * It would cause a sudden jerk. Resetting gives a clean start.
                 */
            }
            rx_pos = 0;   /* reset buffer for next command */

        } else if (rx_pos < (uint8_t)(sizeof(rx_buf) - 1)) {
            rx_buf[rx_pos++] = c;  /* add character to buffer */
        }
        /* if buffer full (rx_pos == 15): silently discard — prevents overflow */
    }
}


/* ============================================================================
 * SECTION 17 — MAIN
 * ============================================================================ */

int main(void) {
    /* Initialize all hardware in correct order */
    systick_init();   /* 1. ARM core timer — must be first so delay_ms() works   */
    gpio_init();      /* 2. Configure all pin directions and alternate functions  */
    uart_init();      /* 3. Set up serial port (PA9 TX, PA10 RX, 115200 baud)    */
    i2c_init();       /* 4. Set up I2C bus (PB6 SCL, PB7 SDA, 100kHz)           */
    mpu_init();       /* 5. Wake up and configure MPU6050 sensor                 */

    /* Startup indication: blink LED 3 times */
    for (int i = 0; i < 3; i++) {
        GPIOC_ODR &= ~(1UL << 13);  /* PC13 LOW  = LED ON  */
        delay_ms(100);
        GPIOC_ODR |=  (1UL << 13);  /* PC13 HIGH = LED OFF */
        delay_ms(100);
    }

    /* Tell pygame we're ready */
    uart_send_str("DRONE_PID_READY\n");
    delay_ms(200);

    uint32_t last_telem_ms = 0;  /* timestamp of last telemetry send             */
    uint32_t loop_count    = 0;  /* counts loops, used for LED blink             */

    /* =========================================================================
     * MAIN LOOP
     * Runs forever at ~1kHz (1000 loops per second)
     * Each iteration:
     *   - Reads IMU
     *   - Estimates angle
     *   - Runs PID
     *   - Handles UART
     *   - Sends telemetry every 10ms
     * ========================================================================= */
    while (1) {
        uint32_t t0 = get_micros();  /* record loop start time in microseconds */

        /* --- BLOCK 1: Read IMU --- */
        imu_read();
        /*
         * Reads 14 bytes from MPU6050 via I2C.
         * Converts to accel_x/y/z (g) and gyro_x/y/z (°/s).
         * Takes ~200µs at 100kHz I2C.
         */

        /* --- BLOCK 2: Complementary filter → roll angle --- */
        complementary_filter();
        /*
         * Updates roll_angle using:
         *   98% gyro integration (fast)
         *   2% accel gravity angle (drift correction)
         * Result: roll_angle in degrees, stable, no drift.
         */

        /* --- BLOCK 3: PID controller → torque command --- */
        pid_update();
        /*
         * Updates pid_output (and p_term, i_term, d_term for display).
         * pid_output range: -100 to +100
         * In a real drone, this feeds the motor mixing matrix.
         * Here we just send it to pygame for visualization.
         */

        /* --- BLOCK 4: Check for commands from pygame --- */
        receive_cmds();
        /*
         * Non-blocking. Checks if new bytes arrived on PA10 (UART RX).
         * If a complete command is found, updates Kp/Ki/Kd/setpoint.
         * Updated gains take effect on the VERY NEXT loop iteration.
         */

        /* --- BLOCK 5: Send telemetry at 100Hz --- */
        if (ms_ticks - last_telem_ms >= 10) {
            send_telemetry();
            last_telem_ms = ms_ticks;
            /*
             * Why not send every loop (1000Hz)?
             * At 115200 baud with a ~50-char line:
             *   50 chars × 10 bits/char = 500 bits
             *   500 / 115200 = 4.3ms to transmit one line
             * Sending every 1ms would require 4.3ms → can't keep up.
             * Sending every 10ms → 5× margin → no UART buffer overflow.
             */
        }

        /* --- BLOCK 6: LED heartbeat (blink every 500ms to show loop alive) --- */
        loop_count++;
        if (loop_count >= 500) {
            GPIOC_ODR ^= (1UL << 13);  /* XOR bit 13 = toggle LED */
            loop_count = 0;
            /*
             * ^= (XOR) toggles bit 13:
             *   If bit was 1 (LED off): becomes 0 → LED ON
             *   If bit was 0 (LED on):  becomes 1 → LED OFF
             * 500 loops × 1ms/loop = 500ms → LED blinks at 1Hz (0.5s ON, 0.5s OFF)
             */
        }

        /* --- BLOCK 7: Pace loop to exactly 1ms (1kHz) --- */
        while ((get_micros() - t0) < 1000);
        /*
         * Spin here until 1000 microseconds have elapsed since loop start.
         * This makes dt consistent regardless of how long blocks 1-6 took.
         *
         * If blocks 1-6 took 600µs: spin 400µs here → total = 1000µs ✓
         * If blocks 1-6 took 200µs: spin 800µs here → total = 1000µs ✓
         *
         * Why consistent dt matters for PID:
         *   D term = (error - last_error) / dt
         *   If dt varies wildly, D term becomes noisy → poor control.
         *   Consistent 1ms dt → clean D term → smoother PID.
         */

        /* --- BLOCK 8: Measure actual dt for next iteration --- */
        dt = (float)(get_micros() - t0) / 1000000.0f;
        /*
         * After the wait, compute actual elapsed time.
         * Should be very close to 0.001000 seconds.
         * Small variations (±1µs) from interrupt timing are normal.
         */

        /* Safety clamps on dt */
        if (dt > 0.01f)    dt = 0.01f;    /* if somehow > 10ms: limit damage to PID */
        if (dt < 0.0001f)  dt = 0.001f;   /* if somehow < 0.1ms: prevent D explosion */
        /*
         * Without clamping: if dt = 0 (shouldn't happen but hardware glitch possible),
         * D term = (error - last_error) / 0 → division by zero → NaN → PID outputs garbage.
         * Clamping protects against edge cases.
         */
    }
    /* while(1) never exits — embedded systems run forever */
    /* The (void) return at the end of main() never executes */
}