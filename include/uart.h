#ifndef UART_H_
#define UART_H_
#include <stdint.h>
#include <stm32f4xx.h>
void uart2_tx_int(void);
void uart2_read(void);
void uart2_rxtx_int(void);


#endif
