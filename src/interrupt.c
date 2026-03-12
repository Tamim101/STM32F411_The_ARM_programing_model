#include "stm32f1xx.h"

#define GPIOCEN   (1U << 4)   // RCC_APB2ENR_IOPCEN
#define AFIOEN    (1U << 0)   // RCC_APB2ENR_AFIOEN

void pc13_exti_init(void)
{
    __disable_irq();

    // Enable GPIOC and AFIO clocks
    RCC->APB2ENR |= GPIOCEN | AFIOEN;

    // PC13 as input floating
    // For STM32F1, pin 13 is configured in CRH
    // MODE13[1:0] = 00 (input)
    // CNF13[1:0]  = 01 (floating input)
    GPIOC->CRH &= ~(0xF << 20);
    GPIOC->CRH |=  (0x4 << 20);

    // Route EXTI13 to Port C
    AFIO->EXTICR[3] &= ~(0xF << 4);   // clear EXTI13 field
    AFIO->EXTICR[3] |=  (0x2 << 4);   // 0x2 = Port C for EXTI13

    // Unmask EXTI13
    EXTI->IMR |= (1U << 13);

    // Choose trigger edge
    EXTI->FTSR |= (1U << 13);         // falling edge
    EXTI->RTSR &= ~(1U << 13);

    // Enable EXTI15_10 interrupt in NVIC
    NVIC_EnableIRQ(EXTI15_10_IRQn);

    __enable_irq();
}