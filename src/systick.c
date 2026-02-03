#include "stm32f1xx.h"

#define SYSTICK_LOAD_VAL      16000
#define CTRL_ENABLE           (1U<<0)
#define CTRL_CLKSRC           (1U<<2)
#define CTRL_COUNTFLAG        (1U<<16)

void systickDelayMs(int n){
    SysTick->LOAD = SYSTICK_LOAD_VAL; // clock per millisecond

    //clear systick current value register 
    SysTick->VAL = 0;
    
    //systick and select internal clk_src
    SysTick->VAL = 1
}