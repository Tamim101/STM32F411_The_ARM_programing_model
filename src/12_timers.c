// #include "stm32f4xx.h"

// #define timer2en     (1U<<0)
// #define cr1_cen      (1U<<0)
// #include "timers.h"
// void tim2_1hz_init(void){
//     RCC->APB1ENR |= timer2en;  // clock access to tim2
//     TIM2->PSC  = 1600 - 1;   //1600000 / 1600 = 10000
//     TIM2->ARR  = 1000 - 1;   //100000 / 100000 = 1
//     TIM2->CNT  = 0;
//     TIM2->CR1  = cr1_cen;
// }