// /**
//  * @file mq7_co_sensor.c
//  * @brief STM32F103 (BluePill) + MQ-7 Carbon Monoxide (CO) sensor
//  *
//  * IMPORTANT — READ THIS FIRST
//  * ---------------------------
//  * Your old code was for an MPU6050 (a motion sensor using I2C). The MQ-7 is a
//  * GAS sensor: it gives an ANALOG VOLTAGE, which we read with the ADC. That is
//  * why the old code never detected gas — it was never reading the MQ-7 at all.
//  * This file replaces all the I2C/MPU6050 logic with proper analog reading.
//  *
//  * SAFETY: A homemade MQ-7 is NOT a substitute for a certified CO alarm
//  * (UL 2034 / EN 50291). Use this for learning/monitoring only.
//  *
//  *
//  * LED BEHAVIOUR (onboard PC13, ACTIVE-LOW: LOW = ON):
//  *   1. Solid ON 2 s at boot      -> board is alive
//  *   2. Fast blink x5             -> UART marker
//  *   3. Slow blinking ~90 s       -> sensor warming up (do NOT trust readings yet)
//  *   4. One long blink            -> clean-air calibration done
//  *   5. Slow heartbeat (1 Hz)     -> normal monitoring, CO low
//  *   6. Fast continuous blink     -> ALARM, CO is rising / above threshold
//  *
//  *
//  * WIRING
//  * ------
//  *   MQ-7 VCC   -> 5V   (the heater needs 5V; do NOT use 3.3V)
//  *   MQ-7 GND   -> GND  (common ground with the STM32 — required)
//  *   MQ-7 AOUT  -> [voltage divider] -> PA0  (ADC channel 0)
//  *
//  *   >>> CRITICAL: the MQ-7 AOUT can swing up to ~5V. The STM32 ADC pin
//  *   >>> tolerates only 3.3V. Feeding 5V into PA0 can DAMAGE the chip.
//  *   >>> Use a divider:  AOUT --[ 10k ]--+--[ 20k ]-- GND
//  *   >>>                                 |
//  *   >>>                                PA0
//  *   >>> That scales 5.0V down to ~3.3V. The factor is set below as
//  *   >>> VOLTAGE_DIVIDER = 1.5 (because (10k+20k)/20k = 1.5).
//  *   >>> If you wired AOUT straight to PA0 (risky), set VOLTAGE_DIVIDER = 1.0.
//  *
//  *   STM32 PA9  (TX) -> USB adapter RX
//  *   STM32 PA10 (RX) -> USB adapter TX
//  *   STM32 GND       -> USB adapter GND
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

// /* ========================================================================== */
// /* REGISTER DEFINITIONS                                                        */
// /* ========================================================================== */

// #define RCC_BASE        0x40021000UL
// #define GPIOA_BASE      0x40010800UL
// #define GPIOC_BASE      0x40011000UL
// #define ADC1_BASE       0x40012400UL
// #define USART1_BASE     0x40013800UL
// #define FLASH_REG_BASE  0x40022000UL
// #define STK_BASE        0xE000E010UL

// #define RCC_CR          (*(volatile uint32_t *)(RCC_BASE + 0x00))
// #define RCC_CFGR        (*(volatile uint32_t *)(RCC_BASE + 0x04))
// #define RCC_APB2ENR     (*(volatile uint32_t *)(RCC_BASE + 0x18))

// #define FLASH_ACR       (*(volatile uint32_t *)(FLASH_REG_BASE + 0x00))

// #define GPIOA_CRL       (*(volatile uint32_t *)(GPIOA_BASE + 0x00))
// #define GPIOA_CRH       (*(volatile uint32_t *)(GPIOA_BASE + 0x04))
// #define GPIOC_CRH       (*(volatile uint32_t *)(GPIOC_BASE + 0x04))
// #define GPIOC_ODR       (*(volatile uint32_t *)(GPIOC_BASE + 0x0C))

// /* ADC1 */
// #define ADC1_SR         (*(volatile uint32_t *)(ADC1_BASE + 0x00))
// #define ADC1_CR1        (*(volatile uint32_t *)(ADC1_BASE + 0x04))
// #define ADC1_CR2        (*(volatile uint32_t *)(ADC1_BASE + 0x08))
// #define ADC1_SMPR2      (*(volatile uint32_t *)(ADC1_BASE + 0x10))
// #define ADC1_SQR1       (*(volatile uint32_t *)(ADC1_BASE + 0x2C))
// #define ADC1_SQR3       (*(volatile uint32_t *)(ADC1_BASE + 0x34))
// #define ADC1_DR         (*(volatile uint32_t *)(ADC1_BASE + 0x4C))

// #define USART1_SR       (*(volatile uint32_t *)(USART1_BASE + 0x00))
// #define USART1_DR       (*(volatile uint32_t *)(USART1_BASE + 0x04))
// #define USART1_BRR      (*(volatile uint32_t *)(USART1_BASE + 0x08))
// #define USART1_CR1      (*(volatile uint32_t *)(USART1_BASE + 0x0C))
// #define USART1_CR2      (*(volatile uint32_t *)(USART1_BASE + 0x10))
// #define USART1_CR3      (*(volatile uint32_t *)(USART1_BASE + 0x14))

// #define STK_CTRL        (*(volatile uint32_t *)(STK_BASE + 0x00))
// #define STK_LOAD        (*(volatile uint32_t *)(STK_BASE + 0x04))
// #define STK_VAL         (*(volatile uint32_t *)(STK_BASE + 0x08))

// /* ========================================================================== */
// /* MQ-7 / MEASUREMENT SETTINGS  (tune these for your hardware)                 */
// /* ========================================================================== */

// #define VREF             3.3f     /* ADC reference voltage                     */
// #define ADC_MAX          4095.0f  /* 12-bit ADC full scale                     */
// #define VOLTAGE_DIVIDER  1.5f     /* set 1.0 if NO divider (not recommended)   */
// #define VCC_SENSOR       5.0f     /* MQ-7 supply voltage                       */
// #define RL_KOHM          10.0f    /* load resistor on the MQ-7 module (kOhm)   */

// /* Warm-up time after power-on. The MQ-7 needs to heat up before it reads
//  * meaningfully. 90 s is a minimum; a brand-new sensor also needs a one-time
//  * "burn-in" of 24-48 h powered on before it settles.                         */
// #define WARMUP_MS        90000UL

// /* ALARM logic.
//  * When CO rises, the sensor resistance Rs DROPS, so the ratio Rs/R0 drops.
//  * We raise the alarm when the ratio falls below ALARM_RATIO (relative method,
//  * which is robust even without lab calibration). Lower the number to make the
//  * alarm less sensitive, raise it to make it more sensitive.                  */
// #define ALARM_RATIO      0.55f

// /* Rough ppm estimate from the datasheet log-log curve. These are APPROXIMATE.
//  * The absolute ppm number is only meaningful after real calibration against a
//  * known CO source — treat it as a trend, not a certified reading.            */
// #define PPM_A            100.0f
// #define PPM_B           (-1.53f)

// /* ========================================================================== */
// /* GLOBALS                                                                     */
// /* ========================================================================== */

// volatile uint32_t tick_ms = 0;
// static float R0 = 10.0f;          /* sensor resistance in clean air (kOhm)     */

// /* ========================================================================== */
// /* SYSTICK / DELAY                                                             */
// /* ========================================================================== */

// void SysTick_Init(void) {
//     STK_LOAD = 72000 - 1;
//     STK_VAL  = 0;
//     STK_CTRL = 0x07;
// }

// void SysTick_Handler(void) { tick_ms++; }

// void Delay_ms(uint32_t ms) {
//     uint32_t start = tick_ms;
//     while ((tick_ms - start) < ms);
// }

// /* ========================================================================== */
// /* LED (PC13, active-low: clear bit = ON, set bit = OFF)                       */
// /* ========================================================================== */

// static inline void LED_On(void)  { GPIOC_ODR &= ~(1 << 13); }
// static inline void LED_Off(void) { GPIOC_ODR |=  (1 << 13); }

// void LED_BlinkSlow(int count) {
//     for (int i = 0; i < count; i++) { LED_On(); Delay_ms(400); LED_Off(); Delay_ms(400); }
// }
// void LED_BlinkFast(int count) {
//     for (int i = 0; i < count; i++) { LED_On(); Delay_ms(80);  LED_Off(); Delay_ms(80);  }
// }
// void LED_BlinkLong(void) { LED_On(); Delay_ms(1500); LED_Off(); Delay_ms(500); }

// /* ========================================================================== */
// /* CLOCK & GPIO                                                                */
// /* ========================================================================== */

// void SystemClock_Init(void) {
//     RCC_CR |= (1 << 16);                 /* HSE on  */
//     while (!(RCC_CR & (1 << 17)));

//     FLASH_ACR = (FLASH_ACR & ~0x07) | 0x02;

//     RCC_CFGR &= ~((0x0F << 18) | (1 << 17) | (1 << 16));
//     RCC_CFGR |= (0x07 << 18) | (1 << 16);    /* PLL x9, src HSE  */
//     RCC_CFGR |= (0x04 << 8);                 /* APB1 = /2        */

//     RCC_CR |= (1 << 24);                     /* PLL on           */
//     while (!(RCC_CR & (1 << 25)));

//     RCC_CFGR = (RCC_CFGR & ~0x03) | 0x02;     /* SW = PLL         */
//     while ((RCC_CFGR & 0x0C) != 0x08);
// }

// void GPIO_Init(void) {
//     /* GPIOA, GPIOC, AFIO clocks */
//     RCC_APB2ENR |= (1 << 0) | (1 << 2) | (1 << 4);

//     /* PA0 -> analog input (MODE=00, CNF=00 = 0x0) */
//     GPIOA_CRL &= ~(0xF << 0);

//     /* PA9 TX (AF push-pull 50MHz = 0xB) */
//     GPIOA_CRH = (GPIOA_CRH & 0xFFFFFF0F) | 0x000000B0;
//     /* PA10 RX (floating input = 0x4) */
//     GPIOA_CRH = (GPIOA_CRH & 0xFFFFF0FF) | 0x00000400;

//     /* PC13 LED (output push-pull 2MHz = 0x2) */
//     GPIOC_CRH = (GPIOC_CRH & 0xFF0FFFFF) | 0x00200000;
//     LED_Off();
// }

// /* ========================================================================== */
// /* USART1 @ 9600                                                               */
// /* ========================================================================== */

// void USART1_Init(void) {
//     RCC_APB2ENR |= (1 << 14);
//     USART1_BRR = 7500;                   /* 72e6 / 9600 */
//     USART1_CR2 = 0;
//     USART1_CR3 = 0;
//     USART1_CR1 = (1 << 13) | (1 << 3) | (1 << 2);
// }

// void USART1_SendChar(char c) {
//     while (!(USART1_SR & (1 << 7)));
//     USART1_DR = (uint8_t)c;
// }
// void USART1_SendString(const char *s) { while (*s) USART1_SendChar(*s++); }

// /* ========================================================================== */
// /* ADC1 (single channel, software-triggered, polled)                          */
// /* ========================================================================== */

// void ADC1_Init(void) {
//     /* ADC clock must be <= 14 MHz. APB2 = 72 MHz, so prescale /6 = 12 MHz. */
//     RCC_CFGR &= ~(0x3 << 14);
//     RCC_CFGR |=  (0x2 << 14);
//     RCC_APB2ENR |= (1 << 9);             /* ADC1 clock */

//     ADC1_CR1 = 0;
//     ADC1_CR2 = 0;

//     ADC1_SMPR2 |= (0x7 << 0);            /* ch0 sample time = 239.5 cyc (stable) */
//     ADC1_SQR1 = 0;                       /* sequence length = 1 conversion       */
//     ADC1_SQR3 = 0;                       /* 1st conversion = channel 0           */

//     /* software-start trigger: EXTSEL=111 (SWSTART), EXTTRIG=1, ADON=1 */
//     ADC1_CR2 |= (1 << 0) | (0x7 << 17) | (1 << 20);
//     Delay_ms(1);

//     ADC1_CR2 |= (1 << 3);                /* RSTCAL */
//     while (ADC1_CR2 & (1 << 3));
//     ADC1_CR2 |= (1 << 2);                /* CAL    */
//     while (ADC1_CR2 & (1 << 2));
// }

// uint16_t ADC1_ReadRaw(void) {
//     ADC1_CR2 |= (1 << 22);               /* SWSTART */
//     while (!(ADC1_SR & (1 << 1)));       /* wait EOC */
//     return (uint16_t)(ADC1_DR & 0xFFF);
// }

// /* Average several samples to reduce noise. */
// uint16_t ADC1_ReadAvg(int n) {
//     uint32_t acc = 0;
//     for (int i = 0; i < n; i++) { acc += ADC1_ReadRaw(); Delay_ms(2); }
//     return (uint16_t)(acc / n);
// }

// /* ========================================================================== */
// /* MQ-7 MATH                                                                   */
// /* ========================================================================== */

// /* Actual sensor output voltage (undo the divider). */
// static float MQ7_Voltage(void) {
//     uint16_t raw = ADC1_ReadAvg(16);
//     return ((float)raw / ADC_MAX) * VREF * VOLTAGE_DIVIDER;
// }

// /* Sensor resistance Rs in kOhm. */
// static float MQ7_Rs(void) {
//     float vs = MQ7_Voltage();
//     if (vs < 0.05f) vs = 0.05f;          /* avoid divide-by-zero */
//     return RL_KOHM * (VCC_SENSOR - vs) / vs;
// }

// /* Calibrate R0 = resistance in clean air. Run with the board in FRESH air. */
// static void MQ7_Calibrate(void) {
//     float acc = 0.0f;
//     for (int i = 0; i < 50; i++) { acc += MQ7_Rs(); Delay_ms(50); }
//     R0 = acc / 50.0f;
// }

// /* Approximate CO ppm from Rs/R0. APPROXIMATE — see notes at top of file. */
// static float MQ7_EstimatePPM(float rs) {
//     float ratio = rs / R0;
//     if (ratio < 0.01f) ratio = 0.01f;
//     return PPM_A * powf(ratio, PPM_B);
// }

// /* ========================================================================== */
// /* MAIN                                                                        */
// /* ========================================================================== */

// int main(void) {
//     SystemClock_Init();
//     SysTick_Init();
//     GPIO_Init();

//     /* 1. board alive */
//     LED_On(); Delay_ms(2000); LED_Off(); Delay_ms(500);

//     /* 2. UART marker */
//     USART1_Init();
//     LED_BlinkFast(5);
//     Delay_ms(300);

//     USART1_SendString("\r\n=====================================\r\n");
//     USART1_SendString(" STM32F103 + MQ-7 Carbon Monoxide\r\n");
//     USART1_SendString("=====================================\r\n");
//     USART1_SendString("NOTE: this is a hobby monitor, NOT a\r\n");
//     USART1_SendString("certified CO alarm. Get a UL2034 unit\r\n");
//     USART1_SendString("for real safety.\r\n\r\n");

//     ADC1_Init();

//     /* 3. warm-up: heater must stabilise before readings mean anything */
//     USART1_SendString("Warming up sensor (~90s). Keep it in CLEAN air.\r\n");
//     USART1_SendString("Do NOT trust readings during warm-up.\r\n");
//     {
//         uint32_t start = tick_ms;
//         while ((tick_ms - start) < WARMUP_MS) {
//             LED_BlinkSlow(1);
//             char b[64];
//             snprintf(b, sizeof(b), "  warming... Vs=%.2fV\r\n", (double)MQ7_Voltage());
//             USART1_SendString(b);
//         }
//     }

//     /* 4. calibrate clean-air baseline */
//     USART1_SendString("\r\nCalibrating clean-air baseline (R0)...\r\n");
//     MQ7_Calibrate();
//     {
//         char b[64];
//         snprintf(b, sizeof(b), "  R0 = %.2f kOhm\r\n", (double)R0);
//         USART1_SendString(b);
//     }
//     LED_BlinkLong();

//     USART1_SendString("\r\nMonitoring started. Heartbeat = OK, fast blink = ALARM.\r\n\r\n");

//     /* 5. monitoring loop */
//     uint32_t last = 0;
//     while (1) {
//         if ((tick_ms - last) >= 1000) {       /* read once per second */
//             last = tick_ms;

//             float rs    = MQ7_Rs();
//             float ratio = rs / R0;
//             float ppm   = MQ7_EstimatePPM(rs);

//             char b[128];
//             snprintf(b, sizeof(b),
//                 "Rs=%.2fk  Rs/R0=%.2f  ~CO=%.0f ppm  %s\r\n",
//                 (double)rs, (double)ratio, (double)ppm,
//                 (ratio < ALARM_RATIO) ? "*** ALARM: CO RISING ***" : "ok");
//             USART1_SendString(b);

//             if (ratio < ALARM_RATIO) {
//                 LED_BlinkFast(5);             /* visible alarm */
//             } else {
//                 LED_On(); Delay_ms(40); LED_Off();   /* gentle heartbeat */
//             }
//         }
//     }
// }