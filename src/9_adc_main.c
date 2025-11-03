#include <stdio.h>
#include <stdint.h>
#include "stm32f4xx.h"
#include "adc.h"
#include "uart.h"

uint32_t sensor_value;

int main(void)
{   
    // enable clock assess to gpio
    uart2_rxtx_int();
    pa1_adc_init();
    start_converstion();
    while(1){
      sensor_value = adc_read();
      printf("sensor value : %d\n\r",(int)sensor_value);
	}
}

