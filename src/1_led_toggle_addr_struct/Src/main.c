#include <stdint.h>

#define PERIPH_BASE          (0x40000000UL)
#define AHB1PERIPH_OFFSET    (0x00020000UL)
#define AHB1PERIPH_BASE      (PERIPH_BASE + AHB1PERIPH_OFFSET)

#define GPIOA_OFFSET         (0x0000UL)
#define GPIOA_BASE           (AHB1PERIPH_BASE + GPIOA_OFFSET)

#define RCC_OFFSET           (0x3800UL)
#define RCC_BASE             (AHB1PERIPH_BASE + RCC_OFFSET)

#define GPIOAEN   (1U << 0)
#define LED_PIN   (1U << 5)   // PA5

typedef struct {
    volatile uint32_t DUMMY[12];  // 0x00..0x2C
    volatile uint32_t AHB1ENR;    // 0x30
} RCC_TypeDef;

// Correct GPIO register map for F4 (through AFR[1])
typedef struct {
    volatile uint32_t MODER;   // 0x00
    volatile uint32_t OTYPER;  // 0x04
    volatile uint32_t OSPEEDR; // 0x08
    volatile uint32_t PUPDR;   // 0x0C
    volatile uint32_t IDR;     // 0x10
    volatile uint32_t ODR;     // 0x14
    volatile uint32_t BSRR;    // 0x18  <- you were missing this
    volatile uint32_t LCKR;    // 0x1C
    volatile uint32_t AFR[2];  // 0x20, 0x24
} GPIO_TypeDef;

#define RCC   ((RCC_TypeDef *)RCC_BASE)
#define GPIOA ((GPIO_TypeDef*)GPIOA_BASE)

int main(void) {
    // Enable GPIOA clock
    RCC->AHB1ENR |= GPIOAEN;

    // PA5 as output: MODER5 = 01b (bits 11:10)
    GPIOA->MODER &= ~(3U << (5*2));
    GPIOA->MODER |=  (1U << (5*2));

    while (1) {
        GPIOA->BSRR = LED_PIN;           // set PA5 high
        for (volatile int i=0; i<100000; ++i) {}
        GPIOA->BSRR = (LED_PIN << 16);   // reset PA5 low
        for (volatile int i=0; i<100000; ++i) {}
    }
}
