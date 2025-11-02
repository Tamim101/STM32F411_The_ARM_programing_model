// // LED on PA5 (Nucleo/Discovery boards commonly wire LED to PA5)
// #include <stdint.h>

// #define PERIPH_BASE          (0x40000000UL)
// #define AHB1PERIPH_OFFSET    (0x00020000UL)
// #define AHB1PERIPH_BASE      (PERIPH_BASE + AHB1PERIPH_OFFSET)

// #define GPIOA_OFFSET         (0x0000UL)
// #define GPIOA_BASE           (AHB1PERIPH_BASE + GPIOA_OFFSET)

// #define RCC_OFFSET           (0x3800UL)
// #define RCC_BASE             (AHB1PERIPH_BASE + RCC_OFFSET)

// #define GPIOAEN              (1U << 0)   // RCC AHB1ENR bit for GPIOA clock
// #define PIN5                 (1U << 5)   // PA5 mask

// typedef struct {
//     volatile uint32_t DUMMY[12]; // offsets 0x00..0x2C (placeholder)
//     volatile uint32_t AHB1ENR;   // offset 0x30: AHB1 peripheral clock enable
// } RCC_TypeDef;

// typedef struct {
//     volatile uint32_t MODER;     // 0x00: mode
//     volatile uint32_t DUMMY2[4]; // 0x04..0x14 placeholders for simplicity
//     volatile uint32_t ODR;       // 0x14: output data
// } GPIO_TypeDef;

// #define RCC    ((RCC_TypeDef*)RCC_BASE)
// #define GPIOA  ((GPIO_TypeDef*)GPIOA_BASE)

// int main(void) {
//     // 1) Enable clock for GPIOA
//     RCC->AHB1ENR |= GPIOAEN;

//     // 2) Set PA5 as General Purpose Output: MODER5 = 01b (bits 11:10)
//     GPIOA->MODER &= ~(3U << (5 * 2));  // clear both bits (11:10)
//     GPIOA->MODER |=  (1U << (5 * 2));  // set bit10 = 1, bit11 = 0
// 	            // volatile → forces real register read/write (no optimization).

//                  // |= mask → set bit(s).

//                  // &= ~mask → clear bit(s).

//                  // ^= mask → toggle bit(s).

//                  // 1U << n → produce a mask for bit n (unsigned 1 shifted left n).

//                  // 3U << (5*2) → selects a 2-bit field (like MODER’s [11:10] for pin 5).

//                  // 3) Blink
//     while (1) {
//         GPIOA->ODR ^= PIN5; // toggle PA5
//         for (volatile int i = 0; i < 100000; i++) { /* crude delay */ }
//     }
// }
