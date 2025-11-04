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

static void_call_back_itrudpt(void){
    sensor_value = ADC1->DR;
    printf("serial data ..........\n\r");
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
            }elif(sensor_value == '0'){
                GPIOA->ODR ~= LED_PIN;
            }
            else{
                GPIOA->ODR ^= LED_PIN;
            }
        }
    }

}
void uar2_rx_intrupt_int(void){
    RCC->AHB1ENR |= GPIOAEN;
    GPIOA->MODER &=~(1U<<4);
    GPIOA->MODER |= (1U<<5);
    // set pa2 alternate funcition type to uart_tx af07
    GPIOA->AFR[0]  |= (1U<<0);
    GPIOA->AFR[0]  |= (1U<<9);
    GPIOA->AFR[0]  |= (1U<<10);
    GPIOA->AFR[0]  &=~(1U<<11);
    //enable clock access to uart2
    RCC->AHB1ENR |= uart2en;
    uart_set_baudrate(USART2,apb1_clk,uart_baudrate);
    USART2->CR1 = (crl1_te | crl1_ue);
    NVIC_EnableIRQ(USART2_IRQn);



    USART2->CR1 |= crl1_te;
    USART2->CR1 |= crl1_ue;

}
