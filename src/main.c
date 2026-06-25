// // // #define PERIPH_BASE                  (0x40000000UL)
// // // #define AHBPERIPH_OFFSET             (0X00020000UL)
// // // #define AHBPERIPH_BASE               (PERIPH_BASE + AHBPERIPH_OFFSET)
// // // #define GPIOA_OFFSET                 (0x0000UL)

// // // #define GPIOA_BASE                   (AHBPERIPH_BASE + GPIOA_OFFSET)

// // // #define RCC_OFFSET                   (0x3800UL)
// // // #define RCC_BASE                     (AHBPERIPH_BASE + RCC_OFFSET)

// // // #define  AHB1EN_R_OFFSET             (0x30UL)
// // // #define  RCC_AHB1EN_R                (*(volatile unsigned int *) (RCC_BASE + AHB1EN_R_OFFSET))
// // // #define  GPIOA_MODE_R_OFFSET         (0x00UL)
// // // #define  GPIOA_MODE_R                (*(volatile unsigned int *) (GPIOA_BASE + GPIOA_MODE_R_OFFSET))

// // // #define OD_R_OFFSET                  (0x14UL)
// // // #define GPIOA_OD_R                   (*(volatile unsigned int *) (GPIOA_BASE + OD_R_OFFSET))

// // // #define GPIOAEN                      (1U<<0)
// // // #define LED_PIN                      PIN5

// // // int main(void){
// // //     RCC_AHB1EN_R |=  GPIOAEN;
// // //     GPIOA_MODE_R |= (1U<<10);  // 0 TO 10 BIT
// // //     GPIOA_MODE_R &=~(1U<<11);   // 11 TO 0 BIT 
// // //     while(1){
// // //         GPIOA_OD_R ^= LED_PIN;
// // //         for(int i = 0; i<100000; i++){
            
// // //         }

// // //     }
    

// // // }

// // // #include <stdint.h>
// // // #define PERIPH_BASE        0x40000000UL
// // // #define RCC ((RCC_TypeDef*)RCC_BASE)
// // // #define RCC ((RCC_TypeDef *)RCC_BASE)
// // // #define GPIOA ((GPIO_TypeDef*)GPIOA_BASE)
// // // #define GPIOA ((GPIO_TypeDef *)GPIOA_BASE)

// // // // RCC and GPIO base addresses for STM32F103 (F1)
// // // #define APB2PERIPH_BASE    (PERIPH_BASE + 0x00010000UL)

// // // #define RCC_BASE           (PERIPH_BASE + 0x00021000UL)      // 0x40021000
// // // #define GPIOA_BASE         (APB2PERIPH_BASE + 0x00000800UL)  // 0x40010800

// // // // RCC registers
// // // #define RCC_APB2ENR_OFFSET 0x18UL
// // // #define RCC_APB2ENR        (*(volatile uint32_t *)(RCC_BASE + RCC_APB2ENR_OFFSET))

// // // // GPIO registers (F1)
// // // #define GPIO_CRL_OFFSET    0x00UL
// // // #define GPIO_ODR_OFFSET    0x0CUL
// // // #define GPIOA_CRL          (*(volatile uint32_t *)(GPIOA_BASE + GPIO_CRL_OFFSET))
// // // #define GPIOA_ODR          (*(volatile uint32_t *)(GPIOA_BASE + GPIO_ODR_OFFSET))

// // // // Bit definitions
// // // #define IOPAEN             (1U << 2)   // RCC_APB2ENR: I/O port A clock enable
// // // #define LED_PIN            (1U << 5)   // PA5

// // // #define __IO volatile
// // // typedef struct {
// // //     volatile uint32_t CR;
// // //     volatile uint32_t CFGR;
// // //     volatile uint32_t CIR;
// // //     volatile uint32_t APB2RSTR;
// // //     volatile uint32_t APB1RSTR;
// // //     volatile uint32_t AHBENR;
// // //     volatile uint32_t APB2ENR;
// // //     volatile uint32_t APB1ENR;
// // // }RCC_TypeDef;
// // // typedef struct {
// // //     volatile uint32_t CRL;
// // //     volatile uint32_t CRH;
// // //     volatile uint32_t IDR;
// // //     volatile uint32_t ODR;
// // //     volatile uint32_t BSRR;
// // //     volatile uint32_t BRR;
// // //     volatile uint32_t LCKR;
// // // }GPIO_TypeDef;

// // // #define RCC      ((RCC_TypeDef*)RCC_BASE)
// // // #define GPIOA    ((GPIO_TypeDef*)GPIOA_BASE)

// // // #include "stm32f1xx.h"

// // // static void delay(volatile uint32_t t) {
// // //     while (t--) { __asm__("nop"); }
// // // }

// // // int main(void) {
// // //     RCC->APB2ENR |= RCC_APB2ENR_IOPCEN;          // Enable GPIOC clock

// // //     GPIOC->CRH &= ~(0xFUL << ((13U - 8U) * 4U)); // PC13 output push-pull 2MHz
// // //     GPIOC->CRH |=  (0x2UL << ((13U - 8U) * 4U));

// // //     GPIOC->BSRR = (1U << 13);                    // LED OFF (active-low)

// // //     while (1) {
// // //         GPIOC->BRR  = (1U << 13);                // ON
// // //         delay(200000);
// // //         GPIOC->BSRR = (1U << 13);                // OFF
// // //         delay(200000);
// // //     }
// // // }


// // // #include "stm32f1xx.h"

// // // #include <stdio.h>
// // // #include<stdint.h>
// // // #include "uart.h"
// // // int main(void)
// // // {
// // //     uart2_init();

// // //     while (1) {
// // //         uart2_write_string("Hello from STM32F103 USART2 (PA2)\r\n");

// // //     }
// // // }
// // // #include "stm32f1xx.h" // or "stm32f103xb.h" depending on your CMSIS setup

// // // // Blue Pill LED on PC13 is usually active-low
// // // #define LED_ON()   (GPIOC->BRR  = (1U << 13))  // drive low
// // // #define LED_OFF()  (GPIOC->BSRR = (1U << 13))  // drive high

// // // static void gpio_led_init(void) {
// // //     RCC->APB2ENR |= RCC_APB2ENR_IOPCEN;      // enable GPIOC clock

// // //     // PC13 output push-pull, 2 MHz
// // //     // CRH controls pins 8..15; PC13 is bits [23:20]
// // //     GPIOC->CRH &= ~(0xFU << 20);
// // //     GPIOC->CRH |=  (0x2U << 20);            // MODE13=10 (2MHz), CNF13=00 (PP)

// // //     LED_OFF();
// // // }

// // // static void uart1_init_115200(void) {
// // //     RCC->APB2ENR |= RCC_APB2ENR_IOPAEN;      // enable GPIOA clock
// // //     RCC->APB2ENR |= RCC_APB2ENR_USART1EN;    // enable USART1 clock

// // //     // PA9 (TX) = AF push-pull, 50 MHz
// // //     GPIOA->CRH &= ~(0xFU << 4);              // PA9 is bits [7:4]
// // //     GPIOA->CRH |=  (0xBU << 4);              // MODE9=11 (50MHz), CNF9=10 (AF PP)

// // //     // PA10 (RX) = input floating
// // //     GPIOA->CRH &= ~(0xFU << 8);              // PA10 is bits [11:8]
// // //     GPIOA->CRH |=  (0x4U << 8);              // MODE10=00, CNF10=01 (floating)

// // //     // Baud rate:
// // //     // For 72MHz PCLK2 and 115200 baud -> BRR = 0x0271
// // //     // If your PCLK2 isn't 72MHz, this value must change.
// // //     USART1->BRR = 0x0271;

// // //     USART1->CR1 = 0;
// // //     USART1->CR1 |= USART_CR1_RE;            // receiver enable
// // //     USART1->CR1 |= USART_CR1_TE;            // transmitter enable (optional)
// // //     USART1->CR1 |= USART_CR1_UE;            // USART enable
// // // }

// // // static char uart1_read_char_blocking(void) {
// // //     while (!(USART1->SR & USART_SR_RXNE)) {
// // //         // wait until a byte arrives
// // //     }
// // //     return (char)USART1->DR;                // reading DR clears RXNE
// // // }

// // // int main(void) {
// // //     gpio_led_init();
// // //     uart1_init_115200();

// // //     while (1) {
// // //         char c = uart1_read_char_blocking();

// // //         // Many terminals send '\r' or '\n' after keys; ignore them
// // //         if (c == '\r' || c == '\n') continue;

// // //         if (c == 'o' || c == 'O') {
// // //             LED_ON();
// // //         } else if (c == 'k' || c == 'K') {
// // //             LED_OFF();
// // //         }
// // //     }
// // // }
// // // int main(void) {
// // //     gpio_led_init();
// // //     uart1_init_115200();

// // //     while (1) {
// // //         char c = uart1_read_char_blocking();

// // //         // Many terminals send '\r' or '\n' after keys; ignore them
// // //         if (c == '\r' || c == '\n') continue;

// // //         if (c == 'o' || c == 'O') {
// // //             LED_ON();
// // //         } else if (c == 'k' || c == 'K') {
// // //             LED_OFF();
// // //         }
// // //     }
// // // }

// // // #include "main.h"

// // // ADC_HandleTypeDef hadc1;

// // // void SystemClock_Config(void);
// // // static void MX_GPIO_Init(void);
// // // static void MX_ADC1_Init(void);

// // // int main(void)
// // // {
// // //   HAL_Init();
// // //   SystemClock_Config();
// // //   MX_GPIO_Init();
// // //   MX_ADC1_Init();

// // //   while (1)
// // //   {
// // //     HAL_ADC_Start(&hadc1);
// // //     HAL_ADC_PollForConversion(&hadc1, HAL_MAX_DELAY);

// // //     uint16_t adc_value = HAL_ADC_GetValue(&hadc1);

// // //     HAL_ADC_Stop(&hadc1);

// // //     // Put breakpoint here and watch adc_value
// // //     HAL_Delay(200);
// // //   }
// // // }

// // // #include "math.h"

// // // ADC_HandleTypeDef hadc1;

// // // void SystemClock_config(void);
// // // static void MX_GPIO_INit(void);
// // // static void MX_ADC1_INit(void);

// // // int main(void){
// // //     HAL_Init();
// // //     SystemClock_Config();
// // //     MX_GPIO_INit();
// // //     MX_ADC1_INit();
// // //     while (1)
// // //     {
// // //         HAL_ADC_Start(&hadc1);
// // //         HAL_ADC_PollForConversion(&hadc1, HAL_MAX_DELAY);
// // //         uint16_t adc_value = HAL_ADC_GetValue(&hadc1);
// // //         HAL_ADC_Stop(&hadc1);
// // //         HAL_Delay(200);
// // //     }
    
// // // }

// // // #define GPIOAEN     (1U<<0)
// // // #define ADC1EN      (1U<<8)
// // // void pa1_adc_init(void){
// // //     RCC->AHBENR |= GPIOAEN;  // ENABLE CLOCK ACCESS TO GPIOA
// // //     GPIOA->BRR |= (1U<<2);
// // //     GPIOA-> BRR |= (1U<<3);  // SET THE MODE OF PA1 TO ANALOG 
// // //     RCC->APB2ENR |= ADC1EN ;   // ENABLE CLOCK ACCESS TO ADC
// // //     while (1){
// // //         for (int i = 0 ; i < 100000;i++){
            
// // //         }
// // //     }


// // // }

// // // #include <stdio.h>
// // // #include <stdint.h>
// // // #include "stm32f1xx.h"
// // // #include "uart.h"
// // // #include "adc.h"
// // // uint32_t sensor_value;
// // // int main(void){
// // //     uart2_tx_init();
// // //     pal_adc_int();
// // //     start_converstion();
// // //     while(1){
// // //         sensor_value = adc_read();
// // //         printf("Sensor value : %d \n\r",sensor_value);
// // //     }
// // // }





// // // #include <stdint.h>
// // // #include "stm32f1xx.h"
// // // #include "timers.h"
// // // #include "systick.h"

// // // #define LED_PIN   (1U << 5)   // PA5

// // // static void delay(volatile uint32_t t) {
// // //     while (t--) { __asm__("nop"); }
// // // }

// // // int main(void)
// // // {
// // //     // Enable GPIOA clock (F103: APB2)
// // //     RCC->APB2ENR |= RCC_APB2ENR_IOPAEN;

// // //     // PA5 output push-pull 2MHz (CRL, pin 5)
// // //     GPIOA->CRL &= ~(0xFUL << (5U * 4U));
// // //     GPIOA->CRL |=  (0x2UL << (5U * 4U));

// // //     tim2_1hz_init();
   

// // //   while (1)
// // // {
// // //     while (!(TIM2->SR & TIM_SR_UIF)) { }  // wait update event
// // //     TIM2->SR &= ~TIM_SR_UIF;              // clear flag

// // //     GPIOA->ODR ^= LED_PIN;
// // //   ;
// // // }

    
  
// // // // }
// // // #include "stm32f1xx.h"

// // // #define LED_PIN       (1U << 13)   // PC13 onboard LED
// // // #define BUTTON_PIN    (1U << 0)    // PA0 button

// // // volatile uint8_t button_pressed = 0;

// // // static void pc13_led_init(void);
// // // static void pa0_exti_init(void);

// // // int main(void)
// // // {
// // //     pc13_led_init();
// // //     pa0_exti_init();

// // //     // LED OFF initially (Blue Pill LED is active-low)
// // //     GPIOC->BSRR = LED_PIN;

// // //     while (1)
// // //     {
// // //         if (button_pressed)
// // //         {
// // //             button_pressed = 0;

// // //             // Toggle LED
// // //             GPIOC->ODR ^= LED_PIN;
// // //         }
// // //     }
// // // }

// // // static void pc13_led_init(void)
// // // {
// // //     // Enable GPIOC clock
// // //     RCC->APB2ENR |= RCC_APB2ENR_IOPCEN;

// // //     // PC13 = output 2 MHz, push-pull
// // //     GPIOC->CRH &= ~(0xFUL << ((13U - 8U) * 4U));
// // //     GPIOC->CRH |=  (0x2UL << ((13U - 8U) * 4U));

// // //     // LED OFF initially
// // //     GPIOC->BSRR = LED_PIN;
// // // }

// // // static void pa0_exti_init(void)
// // // {
// // //     // Enable GPIOA and AFIO clocks
// // //     RCC->APB2ENR |= RCC_APB2ENR_IOPAEN;
// // //     RCC->APB2ENR |= RCC_APB2ENR_AFIOEN;

// // //     // PA0 input with pull-up/pull-down
// // //     GPIOA->CRL &= ~(0xFUL << (0U * 4U));
// // //     GPIOA->CRL |=  (0x8UL << (0U * 4U));   // input pull-up/pull-down

// // //     // Pull-up on PA0
// // //     GPIOA->BSRR = BUTTON_PIN;

// // //     // Map EXTI0 to PA0
// // //     AFIO->EXTICR[0] &= ~(0xFUL << 0);

// // //     // Unmask EXTI0
// // //     EXTI->IMR |= BUTTON_PIN;

// // //     // Trigger on falling edge
// // //     EXTI->FTSR |= BUTTON_PIN;
// // //     EXTI->RTSR &= ~BUTTON_PIN;

// // //     // Clear pending flag
// // //     EXTI->PR = BUTTON_PIN;

// // //     // Enable EXTI0 interrupt in NVIC
// // //     NVIC_EnableIRQ(EXTI0_IRQn);
// // // }

// // // void EXTI0_IRQHandler(void)
// // // {
// // //     if (EXTI->PR & BUTTON_PIN)
// // //     {
// // //         EXTI->PR = BUTTON_PIN;   // clear pending bit
// // //         button_pressed = 1;
// // //     }
// // // }



// // /*
// //  * IMU ATTITUDE — STM32F103C8T6 Blue Pill + MPU6050
// //  * Pure C — direct register access — zero libraries — zero HAL
// //  *
// //  * Every single register write is visible.
// //  * You can see exactly what the hardware is doing.
// //  *
// //  * UART:  PA9 TX → Arduino pin 0 (RX) → /dev/ttyUSB0
// //  * I2C:   PB6 SCL, PB7 SDA → MPU6050
// //  * LED:   PC13 (active LOW)
// //  *
// //  * BAUD RATE AUTO-DETECTED:
// //  * We try the internal 8MHz HSI oscillator directly (no PLL).
// //  * UART baud = 8000000 / 115200 = 69.4 → BRR = 69
// //  * This works on ALL Blue Pill clones regardless of chip brand.
// //  */

// // #include <stdint.h>

// // /* ═══════════════════════════════════════════════════════════════════════
// //    REGISTER BASE ADDRESSES
// //    Every peripheral in STM32 is just a memory address.
// //    You read/write these addresses to control hardware directly.
// //    ═══════════════════════════════════════════════════════════════════════ */
// // #define RCC_BASE    0x40021000UL   /* Reset and Clock Control */
// // #define GPIOA_BASE  0x40010800UL   /* GPIO port A (PA9=TX) */
// // #define GPIOB_BASE  0x40010C00UL   /* GPIO port B (PB6=SCL PB7=SDA) */
// // #define GPIOC_BASE  0x40011000UL   /* GPIO port C (PC13=LED) */
// // #define USART1_BASE 0x40013800UL   /* UART1 (PA9 TX) */
// // #define I2C1_BASE   0x40005400UL   /* I2C1 (PB6 PB7) */
// // #define SYSTICK_BASE 0xE000E010UL  /* SysTick timer */
// // #define FLASH_BASE  0x40022000UL   /* Flash memory controller */

// // /* ═══════════════════════════════════════════════════════════════════════
// //    RCC REGISTERS — controls which peripherals get a clock signal
// //    Without enabling a peripheral clock it will not respond at all
// //    ═══════════════════════════════════════════════════════════════════════ */
// // #define RCC_CR      (*(volatile uint32_t*)(RCC_BASE + 0x00))  /* clock control */
// // #define RCC_CFGR    (*(volatile uint32_t*)(RCC_BASE + 0x04))  /* clock config */
// // #define RCC_APB2ENR (*(volatile uint32_t*)(RCC_BASE + 0x18))  /* APB2 clock enable */
// // #define RCC_APB1ENR (*(volatile uint32_t*)(RCC_BASE + 0x1C))  /* APB1 clock enable */

// // /* ═══════════════════════════════════════════════════════════════════════
// //    GPIO REGISTERS — controls pin direction and output
// //    CRL = pins 0-7, CRH = pins 8-15
// //    Each pin uses 4 bits: MODE[1:0] CNF[1:0]
// //    ═══════════════════════════════════════════════════════════════════════ */
// // #define GPIOA_CRL   (*(volatile uint32_t*)(GPIOA_BASE + 0x00))
// // #define GPIOA_CRH   (*(volatile uint32_t*)(GPIOA_BASE + 0x04))
// // #define GPIOB_CRL   (*(volatile uint32_t*)(GPIOB_BASE + 0x00))
// // #define GPIOC_CRH   (*(volatile uint32_t*)(GPIOC_BASE + 0x04))
// // #define GPIOC_ODR   (*(volatile uint32_t*)(GPIOC_BASE + 0x0C))

// // /* ═══════════════════════════════════════════════════════════════════════
// //    USART1 REGISTERS — serial communication
// //    SR = status, DR = data, BRR = baud rate, CR1 = control
// //    ═══════════════════════════════════════════════════════════════════════ */
// // #define USART1_SR   (*(volatile uint32_t*)(USART1_BASE + 0x00))
// // #define USART1_DR   (*(volatile uint32_t*)(USART1_BASE + 0x04))
// // #define USART1_BRR  (*(volatile uint32_t*)(USART1_BASE + 0x08))
// // #define USART1_CR1  (*(volatile uint32_t*)(USART1_BASE + 0x0C))

// // /* ═══════════════════════════════════════════════════════════════════════
// //    I2C1 REGISTERS — two wire communication to MPU6050
// //    CR1/CR2 = control, SR1/SR2 = status, DR = data, CCR = clock speed
// //    ═══════════════════════════════════════════════════════════════════════ */
// // #define I2C1_CR1    (*(volatile uint32_t*)(I2C1_BASE + 0x00))
// // #define I2C1_CR2    (*(volatile uint32_t*)(I2C1_BASE + 0x04))
// // #define I2C1_OAR1   (*(volatile uint32_t*)(I2C1_BASE + 0x08))
// // #define I2C1_DR     (*(volatile uint32_t*)(I2C1_BASE + 0x10))
// // #define I2C1_SR1    (*(volatile uint32_t*)(I2C1_BASE + 0x14))
// // #define I2C1_SR2    (*(volatile uint32_t*)(I2C1_BASE + 0x18))
// // #define I2C1_CCR    (*(volatile uint32_t*)(I2C1_BASE + 0x1C))
// // #define I2C1_TRISE  (*(volatile uint32_t*)(I2C1_BASE + 0x20))

// // /* ═══════════════════════════════════════════════════════════════════════
// //    SYSTICK REGISTERS — millisecond counter
// //    ═══════════════════════════════════════════════════════════════════════ */
// // #define STK_CTRL    (*(volatile uint32_t*)(SYSTICK_BASE + 0x00))
// // #define STK_LOAD    (*(volatile uint32_t*)(SYSTICK_BASE + 0x04))
// // #define STK_VAL     (*(volatile uint32_t*)(SYSTICK_BASE + 0x08))

// // /* USART status bits */
// // #define USART_SR_TXE   (1 << 7)   /* TX register empty — ready to send */
// // #define USART_SR_TC    (1 << 6)   /* transmission complete */
// // #define USART_SR_RXNE  (1 << 5)   /* RX register not empty — data arrived */

// // /* I2C status bits */
// // #define I2C_SR1_SB      (1 << 0)   /* start bit generated */
// // #define I2C_SR1_ADDR    (1 << 1)   /* address sent */
// // #define I2C_SR1_BTF     (1 << 2)   /* byte transfer finished */
// // #define I2C_SR1_TXE     (1 << 7)   /* data register empty */
// // #define I2C_SR1_RXNE    (1 << 6)   /* data register not empty */
// // #define I2C_SR1_AF      (1 << 10)  /* acknowledge failure */
// // #define I2C_CR1_PE      (1 << 0)   /* peripheral enable */
// // #define I2C_CR1_START   (1 << 8)   /* generate start condition */
// // #define I2C_CR1_STOP    (1 << 9)   /* generate stop condition */
// // #define I2C_CR1_ACK     (1 << 10)  /* acknowledge enable */

// // /* MPU6050 */
// // #define MPU_ADDR        0x68        /* I2C address (AD0=GND) */

// // /* ═══════════════════════════════════════════════════════════════════════
// //    GLOBAL TICK COUNTER — incremented by SysTick every 1ms
// //    volatile = tell compiler this can change outside normal code flow
// //    ═══════════════════════════════════════════════════════════════════════ */
// // static volatile uint32_t tick_ms = 0;

// // /* ── SysTick interrupt handler — called every 1ms ─────────────────────── */
// // void SysTick_Handler(void) { tick_ms++; }

// // /* ── Delay in milliseconds ────────────────────────────────────────────── */
// // static void delay_ms(uint32_t ms)
// // {
// //     uint32_t start = tick_ms;
// //     while ((tick_ms - start) < ms);
// // }

// // /* ═══════════════════════════════════════════════════════════════════════
// //    CLOCK SETUP
// //    Use HSI (internal 8MHz oscillator) directly — NO PLL.
// //    This is the simplest possible clock — works on every STM32F103 clone.
// //    System clock = 8MHz.
// //    UART BRR for 115200 baud = 8000000 / 115200 = 69
// //    ═══════════════════════════════════════════════════════════════════════ */
// // static void clock_init(void)
// // {
// //     /*
// //      * HSI is already running at reset (it is the default clock).
// //      * We just need to make sure it is selected as system clock.
// //      * Bits [1:0] of CFGR = 00 means HSI is system clock.
// //      * This should already be the case but we set it explicitly.
// //      */
// //     RCC_CFGR &= ~(0x3UL);   /* bits [1:0] = 00 = HSI as SYSCLK */

// //     /* Wait for HSI to be selected */
// //     while ((RCC_CFGR & (0x3UL << 2)) != 0);

// //     /*
// //      * SysTick at 1ms:
// //      * LOAD = (clock_hz / 1000) - 1 = (8000000 / 1000) - 1 = 7999
// //      * CTRL bit 2 = 1 means use processor clock (HSI = 8MHz)
// //      * CTRL bit 1 = 1 means generate interrupt
// //      * CTRL bit 0 = 1 means enable counter
// //      */
// //     STK_LOAD = 7999;
// //     STK_VAL  = 0;
// //     STK_CTRL = (1 << 2) | (1 << 1) | (1 << 0);
// // }

// // /* ═══════════════════════════════════════════════════════════════════════
// //    GPIO SETUP
// //    Each pin has a 4-bit config field in CRL (pins 0-7) or CRH (pins 8-15)
// //    Bits: CNF1 CNF0 MODE1 MODE0
// //    Output: MODE=11 (50MHz), CNF=00 (push-pull) or CNF=10 (AF push-pull)
// //    Input:  MODE=00, CNF=01 (floating) or CNF=10 (pull-up/down)
// //    AF OD (I2C): MODE=11, CNF=11
// //    ═══════════════════════════════════════════════════════════════════════ */
// // static void gpio_init(void)
// // {
// //     /* Enable clocks for GPIOA, GPIOB, GPIOC, USART1, I2C1 */
// //     RCC_APB2ENR |= (1<<2)  /* GPIOA */
// //                  | (1<<3)  /* GPIOB */
// //                  | (1<<4)  /* GPIOC */
// //                  | (1<<14);/* USART1 */
// //     RCC_APB1ENR |= (1<<21);/* I2C1 */

// //     /* ── PC13 = LED output (push-pull 2MHz) ──────────────────────────
// //        PC13 is in CRH, pin 13 = bits [23:20]
// //        MODE=10 (2MHz output), CNF=00 (push-pull)
// //        Value = 0b0010 = 0x2
// //     ── */
// //     GPIOC_CRH &= ~(0xFUL << 20);   /* clear bits [23:20] */
// //     GPIOC_CRH |=  (0x2UL << 20);   /* MODE=10 CNF=00 */
// //     GPIOC_ODR |=  (1 << 13);       /* LED off (active LOW on Blue Pill) */

// //     /* ── PA9 = USART1 TX (AF push-pull 50MHz) ────────────────────────
// //        PA9 is in CRH, pin 9 = bits [7:4]
// //        MODE=11 (50MHz), CNF=10 (AF push-pull)
// //        Value = 0b1011 = 0xB
// //     ── */
// //     GPIOA_CRH &= ~(0xFUL << 4);    /* clear bits [7:4] */
// //     GPIOA_CRH |=  (0xBUL << 4);    /* MODE=11 CNF=10 */

// //     /* ── PA10 = USART1 RX (input floating) ──────────────────────────
// //        PA10 is in CRH, pin 10 = bits [11:8]
// //        MODE=00 (input), CNF=01 (floating)
// //        Value = 0b0100 = 0x4
// //     ── */
// //     GPIOA_CRH &= ~(0xFUL << 8);
// //     GPIOA_CRH |=  (0x4UL << 8);

// //     /* ── PB6 = I2C1 SCL, PB7 = I2C1 SDA (AF open-drain 50MHz) ──────
// //        I2C requires open-drain mode — devices pull line low, resistor pulls high
// //        PB6 in CRL, pin 6 = bits [27:24]: MODE=11 CNF=11 = 0xF
// //        PB7 in CRL, pin 7 = bits [31:28]: MODE=11 CNF=11 = 0xF
// //     ── */
// //     GPIOB_CRL &= ~(0xFFUL << 24);  /* clear bits [31:24] */
// //     GPIOB_CRL |=  (0xFFUL << 24);  /* both PB6 PB7: MODE=11 CNF=11 */
// // }

// // /* ═══════════════════════════════════════════════════════════════════════
// //    USART1 SETUP
// //    Clock = HSI = 8MHz
// //    BRR = fCLK / baudrate = 8000000 / 115200 = 69.44 → 69
// //    CR1: bit 13 = UE (USART enable), bit 3 = TE (transmit enable)
// //    ═══════════════════════════════════════════════════════════════════════ */
// // static void uart_init(void)
// // {
// //     USART1_BRR = 69;              /* 8MHz / 115200 = 69 */
// //     USART1_CR1 = (1<<13)|(1<<3); /* UE=1, TE=1 */
// // }

// // /* Send one character */
// // static void uart_putc(char c)
// // {
// //     while (!(USART1_SR & USART_SR_TXE)); /* wait until TX buffer empty */
// //     USART1_DR = (uint32_t)c;
// // }

// // /* Send null-terminated string */
// // static void uart_puts(const char *s)
// // {
// //     while (*s) uart_putc(*s++);
// // }

// // /* Send integer as decimal string */
// // static void uart_puti(int32_t n)
// // {
// //     if (n < 0) { uart_putc('-'); n = -n; }
// //     if (n == 0) { uart_putc('0'); return; }
// //     char tmp[12]; int i = 0;
// //     while (n > 0) { tmp[i++] = '0' + (n % 10); n /= 10; }
// //     while (i--) uart_putc(tmp[i]);  /* print backwards */
// // }

// // /* Send float as "intpart.fracpart" string — no printf needed */
// // static void uart_putf(float f, int decimals)
// // {
// //     if (f < 0) { uart_putc('-'); f = -f; }
// //     int32_t ip = (int32_t)f;
// //     uart_puti(ip);
// //     uart_putc('.');
// //     float fp = f - ip;
// //     for (int i = 0; i < decimals; i++) {
// //         fp *= 10;
// //         uart_putc('0' + (int)fp);
// //         fp -= (int)fp;
// //     }
// // }

// // /* ═══════════════════════════════════════════════════════════════════════
// //    I2C1 SETUP
// //    Clock = APB1 = HSI = 8MHz (no PLL divider set)
// //    100kHz standard mode: CCR = fAPB1 / (2 * fI2C) = 8000000 / 200000 = 40
// //    TRISE for 100kHz = (fAPB1 / 1000000) + 1 = 9
// //    CR2 FREQ field = APB1 MHz = 8
// //    ═══════════════════════════════════════════════════════════════════════ */
// // static void i2c_init(void)
// // {
// //     I2C1_CR1 &= ~I2C_CR1_PE;    /* disable I2C first */

// //     I2C1_CR2   = 8;              /* FREQ = 8MHz (APB1 clock in MHz) */
// //     I2C1_CCR   = 40;             /* 100kHz: 8MHz / (2*100kHz) = 40 */
// //     I2C1_TRISE = 9;              /* max rise time: (8MHz/1MHz)+1 = 9 */
// //     I2C1_OAR1  = 0x4000;        /* own address not used, bit 14 must be 1 */

// //     I2C1_CR1 |= I2C_CR1_PE;     /* enable I2C */
// // }

// // /* ── I2C low-level helpers ────────────────────────────────────────────── */

// // /* Wait for a status bit with timeout — returns 0 on timeout */
// // static int i2c_wait(volatile uint32_t *reg, uint32_t bit)
// // {
// //     uint32_t t = tick_ms;
// //     while (!(*reg & bit)) {
// //         if ((tick_ms - t) > 10) return 0; /* 10ms timeout */
// //     }
// //     return 1;
// // }

// // /* Generate I2C START condition and send address */
// // static int i2c_start(uint8_t addr_rw)
// // {
// //     /* Generate START */
// //     I2C1_CR1 |= I2C_CR1_START;
// //     if (!i2c_wait(&I2C1_SR1, I2C_SR1_SB)) return 0;

// //     /* Send address + R/W bit */
// //     I2C1_DR = addr_rw;

// //     /* Wait for address acknowledged */
// //     if (!i2c_wait(&I2C1_SR1, I2C_SR1_ADDR)) return 0;

// //     /* Clear ADDR flag by reading SR1 then SR2 */
// //     (void)I2C1_SR1;
// //     (void)I2C1_SR2;
// //     return 1;
// // }

// // /* Send one byte and wait for completion */
// // static int i2c_write_byte(uint8_t data)
// // {
// //     if (!i2c_wait(&I2C1_SR1, I2C_SR1_TXE)) return 0;
// //     I2C1_DR = data;
// //     if (!i2c_wait(&I2C1_SR1, I2C_SR1_BTF)) return 0;
// //     return 1;
// // }

// // /* Generate STOP condition */
// // static void i2c_stop(void)
// // {
// //     I2C1_CR1 |= I2C_CR1_STOP;
// //     /* Wait for stop to complete */
// //     uint32_t t = tick_ms;
// //     while ((I2C1_SR2 & (1<<1)) && ((tick_ms - t) < 5)); /* wait BUSY=0 */
// // }

// // /* ── MPU6050: write one byte to register ─────────────────────────────── */
// // static void mpu_write(uint8_t reg, uint8_t val)
// // {
// //     i2c_start(MPU_ADDR << 1);  /* address + write bit (0) */
// //     i2c_write_byte(reg);
// //     i2c_write_byte(val);
// //     i2c_stop();
// //     delay_ms(2);
// // }

// // /* ── MPU6050: read len bytes starting from reg ────────────────────────── */
// // static void mpu_read(uint8_t reg, uint8_t *buf, uint8_t len)
// // {
// //     /* Write phase: send register address */
// //     i2c_start(MPU_ADDR << 1);
// //     i2c_write_byte(reg);

// //     /* Repeated start — switch to read mode */
// //     I2C1_CR1 |= I2C_CR1_START;
// //     i2c_wait(&I2C1_SR1, I2C_SR1_SB);

// //     /* Send address + read bit (1) */
// //     if (len == 1) {
// //         /* Single byte: disable ACK before sending address */
// //         I2C1_CR1 &= ~I2C_CR1_ACK;
// //     } else {
// //         I2C1_CR1 |= I2C_CR1_ACK;
// //     }

// //     I2C1_DR = (MPU_ADDR << 1) | 1;  /* address + read */
// //     i2c_wait(&I2C1_SR1, I2C_SR1_ADDR);
// //     (void)I2C1_SR1; (void)I2C1_SR2; /* clear ADDR */

// //     for (uint8_t i = 0; i < len; i++) {
// //         if (i == len - 1) {
// //             /* Last byte: send NACK then STOP */
// //             I2C1_CR1 &= ~I2C_CR1_ACK;
// //             I2C1_CR1 |= I2C_CR1_STOP;
// //         }
// //         i2c_wait(&I2C1_SR1, I2C_SR1_RXNE);
// //         buf[i] = (uint8_t)I2C1_DR;
// //     }
// // }

// // /* ═══════════════════════════════════════════════════════════════════════
// //    MATH HELPERS — no math.h needed, pure fixed-point or float
// //    STM32F103 has no FPU so float operations use software emulation
// //    ═══════════════════════════════════════════════════════════════════════ */
// // #include <math.h>   /* only for atan2f and sqrtf — these are single functions */
// // #define RAD2DEG 57.2957795f

// // /* ═══════════════════════════════════════════════════════════════════════
// //    MAIN
// //    ═══════════════════════════════════════════════════════════════════════ */
// // int main(void)
// // {
// //     clock_init();
// //     gpio_init();
// //     uart_init();
// //     i2c_init();

// //     delay_ms(200);

// //     uart_puts("\r\n=== IMU BARE METAL ===\r\n");
// //     uart_puts("Clock: HSI 8MHz direct (no PLL)\r\n");
// //     uart_puts("UART:  PA9 TX 115200 baud\r\n");
// //     uart_puts("I2C:   PB6 SCL PB7 SDA 100kHz\r\n\r\n");

// //     /* ── Check MPU6050 WHO_AM_I ───────────────────────────────────────── */
// //     uart_puts("Checking MPU6050...\r\n");

// //     /* Wake up MPU6050 — it starts sleeping */
// //     mpu_write(0x6B, 0x00);   /* PWR_MGMT_1 = 0 = wake up */
// //     delay_ms(100);

// //     uint8_t who = 0;
// //     mpu_read(0x75, &who, 1);  /* WHO_AM_I register */

// //     uart_puts("WHO_AM_I = 0x");
// //     /* Print hex */
// //     uart_putc("0123456789ABCDEF"[who >> 4]);
// //     uart_putc("0123456789ABCDEF"[who & 0xF]);
// //     uart_puts("\r\n");

// //     if (who != 0x68 && who != 0x72) {
// //         uart_puts("ERROR: MPU6050 not found!\r\n");
// //         uart_puts("Check: SDA=PB7 SCL=PB6 VCC=3.3V AD0=GND\r\n");
// //         /* Blink fast and keep printing error */
// //         while (1) {
// //             uart_puts("WIRING ERROR\r\n");
// //             GPIOC_ODR ^= (1 << 13);
// //             delay_ms(300);
// //         }
// //     }

// //     uart_puts("MPU6050 found!\r\n\r\n");

// //     /* ── Configure MPU6050 ────────────────────────────────────────────── */
// //     mpu_write(0x6B, 0x01);   /* PWR_MGMT_1: wake, gyro X clock */
// //     mpu_write(0x19, 0x09);   /* SMPLRT_DIV: 100Hz */
// //     mpu_write(0x1A, 0x03);   /* CONFIG: 44Hz DLPF */
// //     mpu_write(0x1B, 0x00);   /* GYRO_CONFIG: ±250°/s */
// //     mpu_write(0x1C, 0x00);   /* ACCEL_CONFIG: ±2g */
// //     delay_ms(100);

// //     /* ── Calibrate gyroscope ──────────────────────────────────────────── */
// //     uart_puts("Calibrating gyro — keep STILL...\r\n");

// //     float gx_off = 0, gy_off = 0, gz_off = 0;
// //     for (int i = 0; i < 200; i++) {
// //         uint8_t b[6];
// //         mpu_read(0x43, b, 6);
// //         gx_off += (int16_t)((b[0]<<8)|b[1]) / 131.0f;
// //         gy_off += (int16_t)((b[2]<<8)|b[3]) / 131.0f;
// //         gz_off += (int16_t)((b[4]<<8)|b[5]) / 131.0f;
// //         delay_ms(5);
// //     }
// //     gx_off /= 200; gy_off /= 200; gz_off /= 200;

// //     uart_puts("Gyro offsets: gx=");
// //     uart_putf(gx_off, 3);
// //     uart_puts(" gy="); uart_putf(gy_off, 3);
// //     uart_puts(" gz="); uart_putf(gz_off, 3);
// //     uart_puts("\r\n\r\n");

// //     uart_puts("Sending ROLL,PITCH,YAW...\r\n");

// //     /* ── Main loop ────────────────────────────────────────────────────── */
// //     float roll = 0, pitch = 0, yaw = 0;
// //     const float ALPHA = 0.98f;
// //     const float DT    = 0.010f;

// //     while (1)
// //     {
// //         uint32_t t0 = tick_ms;

// //         /* Read accelerometer: 6 bytes from register 0x3B */
// //         uint8_t ab[6];
// //         mpu_read(0x3B, ab, 6);
// //         float ax = (int16_t)((ab[0]<<8)|ab[1]) / 16384.0f;
// //         float ay = (int16_t)((ab[2]<<8)|ab[3]) / 16384.0f;
// //         float az = (int16_t)((ab[4]<<8)|ab[5]) / 16384.0f;

// //         /* Read gyroscope: 6 bytes from register 0x43 */
// //         uint8_t gb[6];
// //         mpu_read(0x43, gb, 6);
// //         float gx = (int16_t)((gb[0]<<8)|gb[1]) / 131.0f - gx_off;
// //         float gy = (int16_t)((gb[2]<<8)|gb[3]) / 131.0f - gy_off;
// //         float gz = (int16_t)((gb[4]<<8)|gb[5]) / 131.0f - gz_off;

// //         /* Angles from accelerometer */
// //         float ar = atan2f(ay, az) * RAD2DEG;
// //         float ap = atan2f(-ax, sqrtf(ay*ay + az*az)) * RAD2DEG;

// //         /* Complementary filter: 98% gyro + 2% accel */
// //         roll  = ALPHA*(roll  + gx*DT) + (1.0f-ALPHA)*ar;
// //         pitch = ALPHA*(pitch + gy*DT) + (1.0f-ALPHA)*ap;
// //         yaw  += gz * DT;
// //         if (yaw >  180.0f) yaw -= 360.0f;
// //         if (yaw < -180.0f) yaw += 360.0f;

// //         /*
// //          * Send: ROLL:23.45,PITCH:-12.30,YAW:5.67\r\n
// //          * We build the string manually — no sprintf, no HAL
// //          */
// //         uart_puts("ROLL:");
// //         uart_putf(roll, 2);
// //         uart_puts(",PITCH:");
// //         uart_putf(pitch, 2);
// //         uart_puts(",YAW:");
// //         uart_putf(yaw, 2);
// //         uart_puts("\r\n");

// //         /* Blink LED every loop so you know it is running */
// //         GPIOC_ODR ^= (1 << 13);

// //         /* Hold 10ms loop rate */
// //         while ((tick_ms - t0) < 10);
// //     }
// // }


// // #include <stdint.h>
// // #include <stdio.h>

// // // ============ STM32F103 Register Definitions ============

// // // RCC (Reset and Clock Control)
// // #define RCC_BASE 0x40021000
// // #define RCC_CR (*(volatile uint32_t *)(RCC_BASE + 0x00))
// // #define RCC_CFGR (*(volatile uint32_t *)(RCC_BASE + 0x04))
// // #define RCC_APB1RSTR (*(volatile uint32_t *)(RCC_BASE + 0x10))
// // #define RCC_APB2RSTR (*(volatile uint32_t *)(RCC_BASE + 0x0C))
// // #define RCC_AHBENR (*(volatile uint32_t *)(RCC_BASE + 0x14))
// // #define RCC_APB1ENR (*(volatile uint32_t *)(RCC_BASE + 0x1C))
// // #define RCC_APB2ENR (*(volatile uint32_t *)(RCC_BASE + 0x18))

// // // GPIO Bases
// // #define GPIOA_BASE 0x40010800
// // #define GPIOB_BASE 0x40010C00
// // #define GPIOC_BASE 0x40011000

// // // GPIO Macros (FIXED - added 'base' parameter)
// // #define GPIO_CRL(base) (*(volatile uint32_t *)((base) + 0x00))
// // #define GPIO_CRH(base) (*(volatile uint32_t *)((base) + 0x04))
// // #define GPIO_IDR(base) (*(volatile uint32_t *)((base) + 0x08))
// // #define GPIO_ODR(base) (*(volatile uint32_t *)((base) + 0x0C))
// // #define GPIO_BSRR(base) (*(volatile uint32_t *)((base) + 0x10))
// // #define GPIO_BRR(base) (*(volatile uint32_t *)((base) + 0x14))

// // // I2C1
// // #define I2C1_BASE 0x40005400
// // #define I2C1_CR1 (*(volatile uint32_t *)(I2C1_BASE + 0x00))
// // #define I2C1_CR2 (*(volatile uint32_t *)(I2C1_BASE + 0x04))
// // #define I2C1_OAR1 (*(volatile uint32_t *)(I2C1_BASE + 0x08))
// // #define I2C1_OAR2 (*(volatile uint32_t *)(I2C1_BASE + 0x0C))
// // #define I2C1_DR (*(volatile uint32_t *)(I2C1_BASE + 0x10))
// // #define I2C1_SR1 (*(volatile uint32_t *)(I2C1_BASE + 0x14))
// // #define I2C1_SR2 (*(volatile uint32_t *)(I2C1_BASE + 0x18))
// // #define I2C1_CCR (*(volatile uint32_t *)(I2C1_BASE + 0x1C))
// // #define I2C1_TRISE (*(volatile uint32_t *)(I2C1_BASE + 0x20))

// // // USART1
// // #define USART1_BASE 0x40013800
// // #define USART1_SR (*(volatile uint32_t *)(USART1_BASE + 0x00))
// // #define USART1_DR (*(volatile uint32_t *)(USART1_BASE + 0x04))
// // #define USART1_BRR (*(volatile uint32_t *)(USART1_BASE + 0x08))
// // #define USART1_CR1 (*(volatile uint32_t *)(USART1_BASE + 0x0C))
// // #define USART1_CR2 (*(volatile uint32_t *)(USART1_BASE + 0x10))
// // #define USART1_CR3 (*(volatile uint32_t *)(USART1_BASE + 0x14))

// // // SysTick
// // #define SYSTICK_CTRL (*(volatile uint32_t *)0xE000E010)
// // #define SYSTICK_LOAD (*(volatile uint32_t *)0xE000E014)
// // #define SYSTICK_VAL (*(volatile uint32_t *)0xE000E018)

// // volatile uint32_t systick_ms = 0;

// // // ============ Clock Configuration ============
// // void clock_init(void) {
// //     // Enable HSE oscillator
// //     RCC_CR |= (1 << 16);  // HSEON
// //     while (!(RCC_CR & (1 << 17)));  // Wait for HSERDY
    
// //     // Configure PLL: HSE * 9 = 72MHz
// //     RCC_CFGR |= (0b111 << 18);  // PLLMUL = 9
// //     RCC_CFGR |= (1 << 16);      // PLLSRC = HSE
// //     RCC_CR |= (1 << 24);        // PLLON
// //     while (!(RCC_CR & (1 << 25)));  // Wait for PLLRDY
    
// //     // Switch to PLL
// //     RCC_CFGR |= (0b10 << 0);  // SW = PLL
// //     while ((RCC_CFGR & (0b11 << 2)) != (0b10 << 2));  // Wait for SWS
// // }

// // // ============ SysTick Timer ============
// // void systick_init(void) {
// //     // Set reload value: 72MHz / 1000 = 72000 for 1ms interrupt
// //     SYSTICK_LOAD = 72000 - 1;
// //     SYSTICK_VAL = 0;
// //     SYSTICK_CTRL = 0x07;  // Enable, use CPU clock, enable interrupt
// // }

// // void delay_ms(uint32_t ms) {
// //     uint32_t start = systick_ms;
// //     while ((systick_ms - start) < ms);
// // }

// // void SysTick_Handler(void) {
// //     systick_ms++;
// // }

// // // ============ USART1 Configuration ============
// // void usart1_init(void) {
// //     // Enable USART1 and GPIOA clocks
// //     RCC_APB2ENR |= (1 << 14);  // USART1EN
// //     RCC_APB2ENR |= (1 << 2);   // IOPAEN
    
// //     // PA9 = TX (Alternate Push-Pull), PA10 = RX (Input Floating)
// //     GPIO_CRH(GPIOA_BASE) &= ~(0xFF << 4);  // Clear PA9, PA10
// //     GPIO_CRH(GPIOA_BASE) |= (0x0B << 4);   // PA9: AF-PP, 50MHz
// //     GPIO_CRH(GPIOA_BASE) |= (0x04 << 8);   // PA10: Input Floating
    
// //     // Configure USART1: 115200 baud
// //     // BRR = 72MHz / (16 * 115200) = 39.0625
// //     USART1_BRR = 39;  // Integer part
    
// //     // Enable USART: TE=1, RE=1, UE=1
// //     USART1_CR1 = (1 << 13) | (1 << 3) | (1 << 2);
// // }

// // void usart1_putchar(char c) {
// //     while (!(USART1_SR & (1 << 7)));  // Wait for TXE
// //     USART1_DR = c;
// // }

// // void usart1_puts(const char *str) {
// //     while (*str) {
// //         usart1_putchar(*str++);
// //     }
// // }

// // int fputc(int ch, FILE *f) {
// //     usart1_putchar(ch);
// //     return ch;
// // }

// // // ============ I2C1 Configuration ============
// // void i2c1_init(void) {
// //     // Enable I2C1 and GPIOB clocks
// //     RCC_APB1ENR |= (1 << 21);  // I2C1EN
// //     RCC_APB2ENR |= (1 << 3);   // IOPBEN
    
// //     // PB6 = SCL, PB7 = SDA (Open Drain Alternate)
// //     GPIO_CRL(GPIOB_BASE) &= ~(0xFF << 24);  // Clear PB6, PB7
// //     GPIO_CRL(GPIOB_BASE) |= (0xDD << 24);   // Mode = 1101 (50MHz AF-OD)
    
// //     // Reset I2C
// //     RCC_APB1RSTR |= (1 << 21);
// //     RCC_APB1RSTR &= ~(1 << 21);
    
// //     // Configure I2C: 400kHz
// //     I2C1_CR2 = 36;   // FREQ = 36MHz (APB1 / 2)
// //     I2C1_CCR = 90;   // For 400kHz: CCR = 72MHz / (2 * 400kHz) = 90
// //     I2C1_TRISE = 37; // TRISE = (1000ns / 1000000ns) * 36MHz + 1 = 37
    
// //     // Enable I2C
// //     I2C1_CR1 = (1 << 0);  // PE = 1
// // }

// // void i2c_start(void) {
// //     I2C1_CR1 |= (1 << 8);  // START
// //     while (!(I2C1_SR1 & (1 << 0)));  // Wait for SB
// // }

// // void i2c_stop(void) {
// //     I2C1_CR1 |= (1 << 9);  // STOP
// // }

// // void i2c_send_address(uint8_t addr, uint8_t write) {
// //     I2C1_DR = (addr << 1) | (write ? 0 : 1);
// //     while (!(I2C1_SR1 & (1 << 1)));  // Wait for ADDR
// //     volatile uint32_t tmp = I2C1_SR2;  // Clear ADDR
// //     (void)tmp;  // Avoid unused variable warning
// // }

// // uint8_t i2c_read_byte(int ack) {
// //     if (ack) {
// //         I2C1_CR1 |= (1 << 10);  // ACK = 1
// //     } else {
// //         I2C1_CR1 &= ~(1 << 10);  // ACK = 0
// //     }
    
// //     while (!(I2C1_SR1 & (1 << 6)));  // Wait for RXNE
// //     return I2C1_DR;
// // }

// // void i2c_write_byte(uint8_t data) {
// //     I2C1_DR = data;
// //     while (!(I2C1_SR1 & (1 << 2)));  // Wait for BTF
// // }

// // uint8_t i2c_read_reg(uint8_t addr, uint8_t reg) {
// //     i2c_start();
// //     i2c_send_address(addr, 1);  // Write
// //     i2c_write_byte(reg);
    
// //     i2c_start();
// //     i2c_send_address(addr, 0);  // Read
// //     uint8_t data = i2c_read_byte(0);  // NACK
// //     i2c_stop();
    
// //     return data;
// // }

// // void i2c_write_reg(uint8_t addr, uint8_t reg, uint8_t data) {
// //     i2c_start();
// //     i2c_send_address(addr, 1);  // Write
// //     i2c_write_byte(reg);
// //     i2c_write_byte(data);
// //     i2c_stop();
// // }

// // void i2c_read_multiple(uint8_t addr, uint8_t reg, uint8_t *data, uint8_t len) {
// //     i2c_start();
// //     i2c_send_address(addr, 1);  // Write
// //     i2c_write_byte(reg);
    
// //     i2c_start();
// //     i2c_send_address(addr, 0);  // Read
    
// //     for (int i = 0; i < len; i++) {
// //         data[i] = i2c_read_byte(i < len - 1);
// //     }
// //     i2c_stop();
// // }

// // // ============ MPU6050 Functions ============
// // #define MPU6050_ADDR 0x68
// // #define MPU6050_WHO_AM_I 0x75
// // #define MPU6050_PWR_MGMT_1 0x6B
// // #define MPU6050_ACCEL_XOUT_H 0x3B

// // void mpu6050_init(void) {
// //     // Check WHO_AM_I
// //     uint8_t who_am_i = i2c_read_reg(MPU6050_ADDR, MPU6050_WHO_AM_I);
// //     printf("WHO_AM_I: 0x%02X\r\n", who_am_i);
    
// //     if (who_am_i != 0x68) {
// //         printf("MPU6050 not found!\r\n");
// //         while (1);
// //     }
    
// //     // Wake up device (clear sleep bit)
// //     i2c_write_reg(MPU6050_ADDR, MPU6050_PWR_MGMT_1, 0x00);
// //     delay_ms(100);
    
// //     printf("MPU6050 initialized!\r\n");
// // }

// // void mpu6050_read(int16_t *aX, int16_t *aY, int16_t *aZ) {
// //     uint8_t buffer[6];
// //     i2c_read_multiple(MPU6050_ADDR, MPU6050_ACCEL_XOUT_H, buffer, 6);
    
// //     *aX = (int16_t)((buffer[0] << 8) | buffer[1]);
// //     *aY = (int16_t)((buffer[2] << 8) | buffer[3]);
// //     *aZ = (int16_t)((buffer[4] << 8) | buffer[5]);
// // }

// // // ============ Main Program ============
// // int main(void) {
// //     clock_init();
// //     systick_init();
// //     usart1_init();
// //     i2c1_init();
    
// //     usart1_puts("\r\n\r\n");
// //     printf("=== STM32F103 MPU6050 (Bare Metal) ===\r\n");
    
// //     mpu6050_init();
    
// //     int16_t aX, aY, aZ;
    
// //     while (1) {
// //         mpu6050_read(&aX, &aY, &aZ);
// //         printf("AccelX: %6d  AccelY: %6d  AccelZ: %6d\r\n", aX, aY, aZ);
// //         delay_ms(500);
// //     }
    
// //     return 0;
// // }

// // // ============ Startup Code ============
// // extern uint32_t _estack;

// // void reset_handler(void);
// // void nmi_handler(void) { while (1); }
// // void hardfault_handler(void) { while (1); }
// // void memmanage_handler(void) { while (1); }
// // void busfault_handler(void) { while (1); }
// // void usagefault_handler(void) { while (1); }
// // void svc_handler(void) { while (1); }
// // void pendsv_handler(void) { while (1); }

// // // Vector table
// // __attribute__((section(".vectors")))
// // uint32_t *vectors[] = {
// //     &_estack,
// //     (uint32_t *)reset_handler,
// //     (uint32_t *)nmi_handler,
// //     (uint32_t *)hardfault_handler,
// //     (uint32_t *)memmanage_handler,
// //     (uint32_t *)busfault_handler,
// //     (uint32_t *)usagefault_handler,
// //     0, 0, 0, 0,
// //     (uint32_t *)svc_handler,
// //     (uint32_t *)pendsv_handler,
// //     (uint32_t *)SysTick_Handler,
// // };

// // void reset_handler(void) {
// //     // Initialize BSS
// //     extern uint32_t _sbss, _ebss;
// //     for (uint32_t *dst = &_sbss; dst < &_ebss; dst++) {
// //         *dst = 0;
// //     }
    
// //     // Copy data section
// //     extern uint32_t _sdata, _edata, _sidata;
// //     for (uint32_t *dst = &_sdata, *src = &_sidata; dst < &_edata; ) {
// //         *dst++ = *src++;
// //     }
    
// //     main();
// //     while (1);
// // }



















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