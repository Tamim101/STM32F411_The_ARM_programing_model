#include "stm32f4xx.h"
#define GPIOAEN          (1U<<0)
#define PINS             (1U<<5)
#define LED_PIN          (1U<<5)
int main(void)
{

	while(1){
		
	}
}
void uar2_tx_int(void){
    RCC->AHB1ENR |= GPIOAEN;
    GPIOA->MODER &=~(1U<<4);
    GPIOA->MODER != (1U<<5);
}