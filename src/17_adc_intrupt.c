#include <stdio.h>
#include "stm32f4xx.h"
#define GPIOAEN          (1U<<0)
#define PINS             (1U<<5)
#define LED_PIN          (1U<<5)
#define uart2en          (1u<<17)
#define sys_freq         16000000
#define uart_baudrate    115200
#define apb1_clk         sys_freq
#define crl1_te          (1U<<3)
#define crl1_ue          (1U<<13)
#define sr_txe           (1U<<7)
#define gpioaen          (1U<<0)
#include "timers.h"
uint32_t sensor_value;
void uar2_rx_intrupt_int(void);

// static void call_back_itrudpt(void){
//     sensor_value = ADC1->DR;
//     printf("serial data ..........\n\r");
// }

void ADC_IRQHandler(void) {
    if (ADC1->SR & (1U << 1)) {  // Check EOC flag
        call_back_itrudpt();
        ADC1->SR &= ~(1U << 1);   // Clear EOC flag
    }
}

int main(void){
    RCC->AHB1ENR |= gpioaen;
    GPIOA->MODER |= (1U<<11);
    GPIOA->MODER |= (1U<<12);
    GPIOA->ODR &=~  (1U<<3);
    for(;;){
        for(int i = 0;i < 100000;i++){
            if(sensor_value == '1'){
                GPIOA->ODR ^= LED_PIN;
            }else if(sensor_value == '0'){
                GPIOA->ODR &= ~LED_PIN;
            }
            else{
                GPIOA->ODR ^= LED_PIN;
            }
        }
    }

}
