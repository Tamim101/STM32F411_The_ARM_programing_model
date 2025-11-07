// #include "intrupt.h"

// #define gpiocen    (1U<<2)
// #define syscfgen   (1U<<14)

// void pc13_exit_init(void){
//     __disable_irq();   // global interrupts
//     RCC->AHB1ENR |= gpiocen;   // clock assiss

//     GPIOC->MODER &=~(1U<<26);
//     GPIOC->MODER &=~(1U<<27);  // set pc13 as input

//     RCC->APB2ENR |= syscfgen;  //enable clock access
//     SYSCFG->EXTICR[3] |= (1U<<5);  // port for exti13
//     EXTI->IMR |= (1U<<13);        // FALLING EDGE TIGGER

//     NVIC_EnableIRQ(EXTI15_10_IRQn); // LINE IN NVIC

//     __enable_irq();            // enable global interrupts
// }