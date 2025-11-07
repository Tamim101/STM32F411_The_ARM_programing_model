// #include "stm32f4xx.h"

// #define systick_load_val        16000 
// #define ctrl_enable             (1U<<0)
// #define ctrl_clksrc             (1U<<2)
// #define ctrl_countflag          (1U<<16)
// #define ctrl_tickint            (1U<<1)

// #define one_sec_load            16000000
// uint32_t sensor_value;
// #define led_on                  (1U<<11)



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

// void systick_1hr_interrupt(void){
// //    SysTick->VAL  = 0;

// //    GPIOA->MODER |= (1U<<3);
// //    RCC->AHB1ENR |= ctrl_enable;
// //    RCC->AHB2ENR |= ctrl_clksrc;
// //    for(;;){
// //       for(int i = 0; i < one_sec_load; i++){
// //         if(sensor_value == '1'){
// //             GPIOA->ODR |= led_on;
// //         }else{
// //             GPIOA->ODR ^= led_on;
// //         }
// //       }
// //    }
//       SysTick->LOAD = one_sec_load -1;
//       SysTick->VAL = 0;
//       SysTick->CTRL |= ctrl_tickint;

// }