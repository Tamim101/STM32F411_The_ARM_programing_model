// #include <stm32f4xx.h>
// #include "adc.h"
// #define gpioaen      (1U<<0)
// #define abc1en       (1U<<8)

// #define adc_ch1      (1U<<0)
// #define adc_seq_len_1   0x00
// #define adc_cq1_len2   (1U<<0)
// #define cr2_swstart    (1U<<30)
// #define sr_eoc          (1U<<1)
// #define cr2_cont        (1U<<1)

// void pa1_adc_init(void){
//     //configure the abc gpoa pin
//     RCC->AHB1ENR |= gpioaen;
//     // enable clock access to gpioa
//     // set the mode of pai to analog
//     GPIOA->MODER |= (1U<<2);
//     GPIOA->MODER |= (1U<<3);

//     // configue the adc modeule 
//     RCC->AHB1ENR |= abc1en;
//     // eneable clock access to adc
//     ADC1->SQR3 = adc_ch1;
//     ADC1->SQR2 = adc_seq_len_1;
//     // cnfig adc parameters
//     ADC1->CR2 |= adc_cq1_len2;
// }
// void start_converstion(void){
//     ADC1->CR2 |= cr2_swstart;
//     ADC1->CR2 |= cr2_cont;
// }
// uint32_t adc_read(void){
//     while(!(ADC1->SR & sr_eoc)){

//     }
//     return (ADC1->DR);
// }