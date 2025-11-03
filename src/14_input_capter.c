#include "stm32f4xx.h"

#define timer2en     (1U<<0)
#define time3en      (1U<<1)
#define cr1_cen      (1U<<0)
#define oc_toggle    ((1U<<4) | (1U<<5))
#define ccer_cc1e    (1U<<0)
#define gpioae       (1U<<0)
#define afr5_tim     (1U<<20)
#define afr6_tim     (1U<<25)
#define ccer_ccis    (1U<<0)
#include "timers.h"
void tim2_1hz_init(void){
    RCC->APB1ENR |= timer2en;  // clock access to tim2
    TIM2->PSC  = 1600 - 1;   //1600000 / 1600 = 10000
    TIM2->ARR  = 1000 - 1;   //100000 / 100000 = 1
    TIM2->CNT  = 0;
    TIM2->CR1  = cr1_cen;
}

// void tim2_pa3_output_compare(void){
//     RCC->AHB1ENR |= gpioae;
//     RCC->APB1ENR |= timer2en;  // clock access to tim2
//     GPIOA->MODER |= (1U<<10);
//     GPIOA->MODER &=~ (1U<<11);
//     GPIOA->AFR[0] |= afr5_tim;
//     TIM2->PSC  = 1600 - 1;   //1600000 / 1600 = 10000
//     TIM2->ARR  = 1000 - 1;   //100000 / 100000 = 1
//     TIM2->CNT  = 0;
//     TIM2->CCMR1 = oc_toggle;  // toggle mode
//     TIM2->CCER  |= ccer_cc1e;  // tim2 ch1 in compare mode
//     TIM2->CR1  = cr1_cen;
// }

void tim3_pa5_input_copter(void){
    RCC->AHB1ENR |= gpioae;
    RCC->APB1ENR |= timer2en;  // clock access to tim2
    GPIOA->MODER |= (1U<<10);
    GPIOA->MODER &=~ (1U<<11);
    GPIOA->AFR[0] |= afr5_tim;
    TIM2->PSC  = 1600 - 1;   //1600000 / 1600 = 10000
    TIM2->ARR  = 1000 - 1;   //100000 / 100000 = 1
    TIM2->CNT  = 0;
    TIM2->CCMR1 = oc_toggle;  // toggle mode
    TIM2->CCER  |= ccer_cc1e;  // tim2 ch1 in compare mode
    TIM2->CR1  = cr1_cen;
}

void tim3_pa6_input_copter(void){
    RCC->AHB1ENR |= gpioae;
    RCC->APB1ENR |= timer2en;  // clock access to tim6
    GPIOA->MODER |= (1U<<10);
    GPIOA->MODER &=~ (1U<<11);
    GPIOA->AFR[0] |= afr5_tim;
    TIM2->PSC  = 1600 - 1;   //1600000 / 1600 = 10000
    TIM2->ARR  = 1000 - 1;   //100000 / 100000 = 1
    TIM2->CNT  = 0;
    TIM2->CCMR1 = oc_toggle;  // toggle mode
    TIM2->CCER  |= ccer_cc1e;  // tim2 ch1 in compare mode
    TIM2->CR1  = cr1_cen;
    RCC->APB1ENR |= time3en;
    TIM3->CCMR1 = ccer_ccis;
    TIM3->CCER  = ccer_cc1e;
    TIM3->CR1  = cr1_cen;

}