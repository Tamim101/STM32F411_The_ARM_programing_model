 #include "exti.h"
 #define GPIOCEN          (1U<<2)
 #define SYSCFGEN         (1U<<14)
 void pc13_exti_init(void){
    // disable global interrupts 
    _disable_irq();
    // enable clock access for gpio 
    RCC->AHB1ERN |= GPIOCEN;
    // enable clock access for syscfg 
    GPIOC->MODER &=~ (1U<<26);
    GPIOC-> MODER &=~(1U<<27);
    // select port for exit
    RCC->APB2ENR |= SYSCFGEN;
    //unmark exit
    SYSCFG->EXTICR[3] |= (1U<<5);
    // enable exit13 line in nvic 
    EXTI->FTSR |= (1U<<13);
    // enable global intrrupts 
    NVIC_ENABLEIRQ(EXTI15_10_IRQn);
    __enable_irq();
 }