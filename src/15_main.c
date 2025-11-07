// #include <stdio.h>
// #include <stdint.h>
// #include "stm32f4xx.h"
// #include "uart.h"
// #include "adc.h"
// #include  "systick.h"
// #include "timers.h"
// #include "intrupt.h"


// #define gpioaen         (1U<<0)
// #define pin5            (1U<<5)
// #define led_pin         pin5
// static void exti_callback(void);
// int time_stamp = 0;
// int main(void){
//     RCC->AHB1ENR |= gpioaen;
//     GPIOA->MODER |= (1U<<10);
//     GPIOA->MODER &=~ (1U<<11);
//     pc13_exit_init();
//     uart2_tx_int();
//     for(;;){
          
          
          
        
        
//     }
// }

// static void exti_callback(void){
//     printf("BTN pressed......\n\r");
//     GPIOA->ODR ^= led_pin;
// }

// void EXTI15_10_IRQHANDLER(void){
//     if((EXTI->PR & line13)!= 0){
//         EXTI->PR |= line13;
//         exti_callback();
//     }

// }