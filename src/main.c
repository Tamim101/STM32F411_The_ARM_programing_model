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


#include "stm32f1xx.h"

// #define LED_PIN   (1U << 5)   // PA5
// #define BTN_PIN   (1U << 0)   // PA0

// static void delay(volatile uint32_t t) {
//     while (t--) { __asm__("nop"); }
// }

// int main(void) {
//     // Enable clocks for GPIOA
//     RCC->APB2ENR |= RCC_APB2ENR_IOPAEN;

//     // ---- PA5 as output push-pull 2 MHz ----
//     GPIOA->CRL &= ~(0xFUL << (5U * 4U));
//     GPIOA->CRL |=  (0x2UL << (5U * 4U));   // 0b0010 output PP 2MHz

//     // ---- PA0 as input with pull-up ----
//     // CNF=10, MODE=00 => 0b1000 = 0x8 for that pin nibble
//     GPIOA->CRL &= ~(0xFUL << (0U * 4U));
//     GPIOA->CRL |=  (0x8UL << (0U * 4U));   // input pull-up/pull-down
//     GPIOA->ODR |= BTN_PIN;                 // pull-up selected (ODR bit = 1)

//     while (1) {
//         // With pull-up: not pressed = 1, pressed = 0
//         if ((GPIOA->IDR & BTN_PIN) == 0) {
//             // pressed -> turn LED ON
//             GPIOA->BSRR = LED_PIN;         // PA5 HIGH
//         } else {
//             // not pressed -> turn LED OFF
//             GPIOA->BSRR = (LED_PIN << 16); // PA5 LOW
//         }

//         delay(30000); // small debounce
//     }
// }

