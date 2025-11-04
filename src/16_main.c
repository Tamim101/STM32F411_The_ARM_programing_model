#include <stdio.h>
#include <stdint.h>

#include "stm32f4xx.h"
#include "uart.h"
#define GPIOAEN          (1U<<0)
#define PINS             (1U<<5)
#define LED_PIN          (1U<<5)
#define SR_RXNE          (1U<<5)
char key;
int main(void)
{   
    // enable clock assess to gpio
    RCC->AHB1ENR |= GPIOAEN;

    //SET PA5 as output pin
    GPIOA->MODER |= (1U<<10);
    GPIOA->MODER |= (1U<<11);
    uar2_rx_intrupt_int();
    uart2_read();
    while(1){
        uart2_read();
        if(key == '1'){
            GPIOA->ODR |= LED_PIN;
        }else{
            GPIOA->ODR &=~ LED_PIN;
        }
        printf("hello its me stm32............\n\r");
		
	}
}

static void_uart_callback(void){
    key = USART2->DR;
    if(key == '1'){
        GPIOA->ODR |= LED_PIN;

    }else{
        GPIOA->ODR &=~LED_PIN;
    }
}

void USART2_IRQHandler(void){
    if(USART2->SR & SR_RXNE){
      uart_callback();
    }
}