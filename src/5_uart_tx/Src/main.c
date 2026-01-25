#include "stm32f4xx.h"
#define GPIOAEN          (1U<<0)
#define PINS             (1U<<5)
#define LED_PIN          (1U<<5)
#define GPIOCEN          (1U<<2)
#define PIN13            (1U<<13)
#define BTN_PIN          PIN13
int main(void)
{
    RCC->AHB1ENR |=GPIOAEN;  // ENABLE CLOCK ACCESS TO GPIOA AND GPIOC*
	RCC->AHB1ENR |=GPIOCEN;

    GPIOA->MODER |=(1U<<10);  // SET PA5 AS OUTPUT PIN*
    GPIOA->MODER &=~(1U<<11);

	GPIOC->MODER &=~(1U<<26);  // SET PIN13 AS INPUT PIN*
	GPIOA->MODER &=~(1U<<27);
	while(1){
		 // CHEAK IF BTN IS PASSED
		if(GPIOC -> IDR & BTN_PIN){
           GPIOA->ODR ^= LED_PIN;
		}else{
			GPIOA->BSRR = (1U<<21);
		}
	}
}
