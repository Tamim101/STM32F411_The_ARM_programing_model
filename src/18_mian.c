// #include <stdio.h>
// #include <stdint.h>
// #include "stm32f4xx.h"
// #include "uart.h"
// #include "adc.h"
// #include  "systick.h"

// #define gpioaen         (1U<<0)
// #define pin5            (1U<<5)
// #define led_pin         pin5



// int main(void){
//     RCC->AHB1ENR |= gpioaen;
//     GPIOA->MODER |= (1U<<10);
//     GPIOA->MODER &=~ (1U<<12);
//     uar2_rx_intrupt_int();
//     uart2_tx_int();
//     for(;;){
//        printf("A secound passed !! \n \r");
//        GPIOA->ODR ^= led_pin;
//        sys_tick_delay_ms(1000);
//     }
// }
// static void systick_callback(void){
//     printf("second passed \n");
// }

// void SysTick_handler(void){
//     systick_callback();
// }