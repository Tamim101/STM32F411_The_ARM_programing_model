#include <stdio.h>
#include <stdint.h>
#include "stm32f4xx.h"
#include "uart.h"
#include "adc.h"
#include  "systick.h"
#include "timers.h"

#define gpioaen         (1U<<0)
#define pin5            (1U<<5)
#define led_pin         pin5



int main(void){
    RCC->AHB1ENR |= gpioaen;
    GPIOA->MODER |= (1U<<10);
    GPIOA->MODER &=~ (1U<<11);
    for(;;){
        while(!(TIM2->SR & sr_uif)){

        }
            printf("A secound passed !! \n \r");
            GPIOA->ODR ^= led_pin;
          
        
    }
}
