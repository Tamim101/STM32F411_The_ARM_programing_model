// #include "stm32f4xx.h"

// #define systick_load_val        16000 
// #define ctrl_enable             (1U<<0)
// #define ctrl_clksrc             (1U<<2)
// #define ctrl_countflag          (1U<<16)

// void sys_tick_delay_ms(int delay){
//     SysTick->LOAD   =  systick_load_val;  // number of clock per millisecound
//     SysTick-> VAL   = 0;                  // clear systick current value register
//     SysTick-> CTRL  = ctrl_enable | ctrl_clksrc;
//     for(int i = 0; i < delay; i++){
//         while((SysTick->CTRL & ctrl_countflag)==0){

//         }
//     }
//     SysTick->CTRL = 0;
// }