#include "uart.h"

#define apb1_clk         sys_freq
#define crl1_te          (1U<<3)
#define crl1_re          (1U<<2)
#define crl1_ue          (1U<<13)
#define sr_txe           (1U<<7)
#define sr_RXNE          (1U<<5)
#define GPIOAEN          (1U<<0)
#define PINS             (1U<<5)
#define LED_PIN          (1U<<5)
#define uart2en          (1u<<17)
#define sys_freq         16000000
#define uart_baudrate    115200

void uart2_write(int ch);
int __io_putchar(int ch){
    uart2_write(ch);
    return ch;
}

static uint16_t compute_uart_div(uint32_t periphCLK, uint32_t BaudRate);
static void uart_set_baudrate(USART_TypeDef *usartx, uint32_t periphclk,uint32_t BaudRate);
void uart2_rxtx_int(void);
void uart2_read(void);
void uart2_read(void);

void uart2_rxtx_int(void){
    RCC->AHB1ENR |= GPIOAEN;
    GPIOA->MODER &=~(1U<<4);
    GPIOA->MODER |= (1U<<5);
   
    // set pa2 alternate funcition type to uart_tx af07
    GPIOA->AFR[0]  |= (1U<<8);
    GPIOA->AFR[0]  |= (1U<<9);
    GPIOA->AFR[0]  |= (1U<<10);
    GPIOA->AFR[0]  &=~(1U<<11);
   
    //enable clock access to uart2
    RCC->AHB1ENR |= uart2en;
    uart_set_baudrate(USART2,apb1_clk,uart_baudrate);
    USART2->CR1 |= crl1_te;
    USART2->CR1 |= crl1_ue;
    USART2->CR1 |= (crl1_te| crl1_re);
    // pa3 mode
    
    GPIOA->MODER &=~(1U<<6);  // pa3 mode
    GPIOA->MODER |= (1U<<7);
    GPIOA->AFR[0]  |= (1U<<12);
    GPIOA->AFR[0]  |= (1U<<13);
    GPIOA->AFR[0]  |= (1U<<14);
    GPIOA->AFR[0]  &=~(1U<<15);
}

void uart2_read(void){
     while (!(USART2->SR & sr_RXNE)) { }
    return (USART2->DR & 0xFF);
}


void uart2_write(int ch){
    while(!(USART2->SR & sr_txe)){

    }
    USART2->DR = (ch & 0xFF);

}


void uart2_tx_int(void){
    RCC->AHB1ENR |= GPIOAEN;
    GPIOA->MODER &=~(1U<<4);
    GPIOA->MODER |= (1U<<5);
   
    // set pa2 alternate funcition type to uart_tx af07
    GPIOA->AFR[0]  |= (1U<<8);
    GPIOA->AFR[0]  |= (1U<<9);
    GPIOA->AFR[0]  |= (1U<<10);
    GPIOA->AFR[0]  &=~(1U<<11);
   
    //enable clock access to uart2
    RCC->AHB1ENR |= uart2en;
    uart_set_baudrate(USART2,apb1_clk,uart_baudrate);
    USART2->CR1 |= crl1_te;
    USART2->CR1 |= crl1_ue;
    // USART2->CR1 |= (crl1_te| crl1_re);
    // pa3 mode
    
    GPIOA->MODER &=~(1U<<6);  // pa3 mode
    GPIOA->MODER |= (1U<<7);
    // GPIOA->AFR[0]  |= (1U<<12);
    // GPIOA->AFR[0]  |= (1U<<13);
    // GPIOA->AFR[0]  |= (1U<<14);
    // GPIOA->AFR[0]  &=~(1U<<15);
}



static void uart_set_baudrate(USART_TypeDef *usartx, uint32_t periphclk,uint32_t BaudRate){
    /* Compute and program the USART BRR register using the helper to avoid unused-function warning */
    usartx->BRR = compute_uart_div(periphclk, BaudRate);
}

static uint16_t compute_uart_div(uint32_t periphCLK, uint32_t BaudRate){
    return ((periphCLK + (BaudRate/2U))/BaudRate);
}

