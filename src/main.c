// #define PERIPH_BASE                  (0x40000000UL)
// #define AHBPERIPH_OFFSET             (0X00020000UL)
// #define AHBPERIPH_BASE               (PERIPH_BASE + AHBPERIPH_OFFSET)
// #define GPIOA_OFFSET                 (0x0000UL)

// #define GPIOA_BASE                   (AHBPERIPH_BASE + GPIOA_OFFSET)

// #define RCC_OFFSET                   (0x3800UL)
// #define RCC_BASE                     (AHBPERIPH_BASE + RCC_OFFSET)

// #define  AHB1EN_R_OFFSET             (0x30UL)
// #define  RCC_AHB1EN_R                (*(volatile unsigned int *) (RCC_BASE + AHB1EN_R_OFFSET))
// #define  GPIOA_MODE_R_OFFSET         (0x00UL)
// #define  GPIOA_MODE_R                (*(volatile unsigned int *) (GPIOA_BASE + GPIOA_MODE_R_OFFSET))

// #define OD_R_OFFSET                  (0x14UL)
// #define GPIOA_OD_R                   (*(volatile unsigned int *) (GPIOA_BASE + OD_R_OFFSET))

// #define GPIOAEN                      (1U<<0)
// #define LED_PIN                      PIN5

// int main(void){
//     RCC_AHB1EN_R |=  GPIOAEN;
//     GPIOA_MODE_R |= (1U<<10);  // 0 TO 10 BIT
//     GPIOA_MODE_R &=~(1U<<11);   // 11 TO 0 BIT 
//     while(1){
//         GPIOA_OD_R ^= LED_PIN;
//         for(int i = 0; i<100000; i++){
            
//         }

//     }
    

// }

// #include <stdint.h>
// #define PERIPH_BASE        0x40000000UL
// #define RCC ((RCC_TypeDef*)RCC_BASE)
// #define RCC ((RCC_TypeDef *)RCC_BASE)
// #define GPIOA ((GPIO_TypeDef*)GPIOA_BASE)
// #define GPIOA ((GPIO_TypeDef *)GPIOA_BASE)

// // RCC and GPIO base addresses for STM32F103 (F1)
// #define APB2PERIPH_BASE    (PERIPH_BASE + 0x00010000UL)

// #define RCC_BASE           (PERIPH_BASE + 0x00021000UL)      // 0x40021000
// #define GPIOA_BASE         (APB2PERIPH_BASE + 0x00000800UL)  // 0x40010800

// // RCC registers
// #define RCC_APB2ENR_OFFSET 0x18UL
// #define RCC_APB2ENR        (*(volatile uint32_t *)(RCC_BASE + RCC_APB2ENR_OFFSET))

// // GPIO registers (F1)
// #define GPIO_CRL_OFFSET    0x00UL
// #define GPIO_ODR_OFFSET    0x0CUL
// #define GPIOA_CRL          (*(volatile uint32_t *)(GPIOA_BASE + GPIO_CRL_OFFSET))
// #define GPIOA_ODR          (*(volatile uint32_t *)(GPIOA_BASE + GPIO_ODR_OFFSET))

// // Bit definitions
// #define IOPAEN             (1U << 2)   // RCC_APB2ENR: I/O port A clock enable
// #define LED_PIN            (1U << 5)   // PA5

// #define __IO volatile
// typedef struct {
//     volatile uint32_t CR;
//     volatile uint32_t CFGR;
//     volatile uint32_t CIR;
//     volatile uint32_t APB2RSTR;
//     volatile uint32_t APB1RSTR;
//     volatile uint32_t AHBENR;
//     volatile uint32_t APB2ENR;
//     volatile uint32_t APB1ENR;
// }RCC_TypeDef;
// typedef struct {
//     volatile uint32_t CRL;
//     volatile uint32_t CRH;
//     volatile uint32_t IDR;
//     volatile uint32_t ODR;
//     volatile uint32_t BSRR;
//     volatile uint32_t BRR;
//     volatile uint32_t LCKR;
// }GPIO_TypeDef;

// #define RCC      ((RCC_TypeDef*)RCC_BASE)
// #define GPIOA    ((GPIO_TypeDef*)GPIOA_BASE)

// #include "stm32f1xx.h"

// static void delay(volatile uint32_t t) {
//     while (t--) { __asm__("nop"); }
// }

// int main(void) {
//     RCC->APB2ENR |= RCC_APB2ENR_IOPCEN;          // Enable GPIOC clock

//     GPIOC->CRH &= ~(0xFUL << ((13U - 8U) * 4U)); // PC13 output push-pull 2MHz
//     GPIOC->CRH |=  (0x2UL << ((13U - 8U) * 4U));

//     GPIOC->BSRR = (1U << 13);                    // LED OFF (active-low)

//     while (1) {
//         GPIOC->BRR  = (1U << 13);                // ON
//         delay(200000);
//         GPIOC->BSRR = (1U << 13);                // OFF
//         delay(200000);
//     }
// }


// #include "stm32f1xx.h"

// #include <stdio.h>
// #include<stdint.h>
// #include "uart.h"
// int main(void)
// {
//     uart2_init();

//     while (1) {
//         uart2_write_string("Hello from STM32F103 USART2 (PA2)\r\n");

//     }
// }
// #include "stm32f1xx.h" // or "stm32f103xb.h" depending on your CMSIS setup

// // Blue Pill LED on PC13 is usually active-low
// #define LED_ON()   (GPIOC->BRR  = (1U << 13))  // drive low
// #define LED_OFF()  (GPIOC->BSRR = (1U << 13))  // drive high

// static void gpio_led_init(void) {
//     RCC->APB2ENR |= RCC_APB2ENR_IOPCEN;      // enable GPIOC clock

//     // PC13 output push-pull, 2 MHz
//     // CRH controls pins 8..15; PC13 is bits [23:20]
//     GPIOC->CRH &= ~(0xFU << 20);
//     GPIOC->CRH |=  (0x2U << 20);            // MODE13=10 (2MHz), CNF13=00 (PP)

//     LED_OFF();
// }

// static void uart1_init_115200(void) {
//     RCC->APB2ENR |= RCC_APB2ENR_IOPAEN;      // enable GPIOA clock
//     RCC->APB2ENR |= RCC_APB2ENR_USART1EN;    // enable USART1 clock

//     // PA9 (TX) = AF push-pull, 50 MHz
//     GPIOA->CRH &= ~(0xFU << 4);              // PA9 is bits [7:4]
//     GPIOA->CRH |=  (0xBU << 4);              // MODE9=11 (50MHz), CNF9=10 (AF PP)

//     // PA10 (RX) = input floating
//     GPIOA->CRH &= ~(0xFU << 8);              // PA10 is bits [11:8]
//     GPIOA->CRH |=  (0x4U << 8);              // MODE10=00, CNF10=01 (floating)

//     // Baud rate:
//     // For 72MHz PCLK2 and 115200 baud -> BRR = 0x0271
//     // If your PCLK2 isn't 72MHz, this value must change.
//     USART1->BRR = 0x0271;

//     USART1->CR1 = 0;
//     USART1->CR1 |= USART_CR1_RE;            // receiver enable
//     USART1->CR1 |= USART_CR1_TE;            // transmitter enable (optional)
//     USART1->CR1 |= USART_CR1_UE;            // USART enable
// }

// static char uart1_read_char_blocking(void) {
//     while (!(USART1->SR & USART_SR_RXNE)) {
//         // wait until a byte arrives
//     }
//     return (char)USART1->DR;                // reading DR clears RXNE
// }

// int main(void) {
//     gpio_led_init();
//     uart1_init_115200();

//     while (1) {
//         char c = uart1_read_char_blocking();

//         // Many terminals send '\r' or '\n' after keys; ignore them
//         if (c == '\r' || c == '\n') continue;

//         if (c == 'o' || c == 'O') {
//             LED_ON();
//         } else if (c == 'k' || c == 'K') {
//             LED_OFF();
//         }
//     }
// }
// int main(void) {
//     gpio_led_init();
//     uart1_init_115200();

//     while (1) {
//         char c = uart1_read_char_blocking();

//         // Many terminals send '\r' or '\n' after keys; ignore them
//         if (c == '\r' || c == '\n') continue;

//         if (c == 'o' || c == 'O') {
//             LED_ON();
//         } else if (c == 'k' || c == 'K') {
//             LED_OFF();
//         }
//     }
// }

// #include "main.h"

// ADC_HandleTypeDef hadc1;

// void SystemClock_Config(void);
// static void MX_GPIO_Init(void);
// static void MX_ADC1_Init(void);

// int main(void)
// {
//   HAL_Init();
//   SystemClock_Config();
//   MX_GPIO_Init();
//   MX_ADC1_Init();

//   while (1)
//   {
//     HAL_ADC_Start(&hadc1);
//     HAL_ADC_PollForConversion(&hadc1, HAL_MAX_DELAY);

//     uint16_t adc_value = HAL_ADC_GetValue(&hadc1);

//     HAL_ADC_Stop(&hadc1);

//     // Put breakpoint here and watch adc_value
//     HAL_Delay(200);
//   }
// }

// #include "math.h"

// ADC_HandleTypeDef hadc1;

// void SystemClock_config(void);
// static void MX_GPIO_INit(void);
// static void MX_ADC1_INit(void);

// int main(void){
//     HAL_Init();
//     SystemClock_Config();
//     MX_GPIO_INit();
//     MX_ADC1_INit();
//     while (1)
//     {
//         HAL_ADC_Start(&hadc1);
//         HAL_ADC_PollForConversion(&hadc1, HAL_MAX_DELAY);
//         uint16_t adc_value = HAL_ADC_GetValue(&hadc1);
//         HAL_ADC_Stop(&hadc1);
//         HAL_Delay(200);
//     }
    
// }

// #define GPIOAEN     (1U<<0)
// #define ADC1EN      (1U<<8)
// void pa1_adc_init(void){
//     RCC->AHBENR |= GPIOAEN;  // ENABLE CLOCK ACCESS TO GPIOA
//     GPIOA->BRR |= (1U<<2);
//     GPIOA-> BRR |= (1U<<3);  // SET THE MODE OF PA1 TO ANALOG 
//     RCC->APB2ENR |= ADC1EN ;   // ENABLE CLOCK ACCESS TO ADC
//     while (1){
//         for (int i = 0 ; i < 100000;i++){
            
//         }
//     }


// }

// #include <stdio.h>
// #include <stdint.h>
// #include "stm32f1xx.h"
// #include "uart.h"
// #include "adc.h"
// uint32_t sensor_value;
// int main(void){
//     uart2_tx_init();
//     pal_adc_int();
//     start_converstion();
//     while(1){
//         sensor_value = adc_read();
//         printf("Sensor value : %d \n\r",sensor_value);
//     }
// }





#include <stdint.h>
#include "stm32f1xx.h"

#define LED_PIN   (1U << 5)   // PA5

static void delay(volatile uint32_t t) {
    while (t--) { __asm__("nop"); }
}

int main(void)
{
    // Enable GPIOA clock (F103: APB2)
    RCC->APB2ENR |= RCC_APB2ENR_IOPAEN;

    // PA5 output push-pull 2MHz (CRL, pin 5)
    GPIOA->CRL &= ~(0xFUL << (5U * 4U));
    GPIOA->CRL |=  (0x2UL << (5U * 4U));

    while (1) {
        GPIOA->ODR ^= LED_PIN;
        delay(300000);
    }
   
}

