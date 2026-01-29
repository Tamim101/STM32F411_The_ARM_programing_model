// #include "stm32f1xx.h"
// #include <stdint.h>
// #include "adc.h"
// #define GPIOAEN      (1U<<0)
// #define ADC1EN       (1U<<8)
// #define ADC_CH1      (1U<<0)
// #define ADC_SEQ_LEN   0x00 
// #define CR2_ADON     (1U<<0)
// #define CR2_SWSTART  (1U<<30)
// #define SR_EOC       (1U<<1)


// void pa1_adc_init(void){
//     RCC->AHBENR |= GPIOAEN;  // ENABLE CLOCK ACCESS TO GPIOA
//     GPIOA->BRR |= (1U<<2);
//     GPIOA-> BRR |= (1U<<3);  // SET THE MODE OF PA1 TO ANALOG 
//     RCC->APB2ENR |= ADC1EN ;   // ENABLE CLOCK ACCESS TO ADC
//     // while (1){
//     //     for (int i = 0 ; i < 100000;i++){
            
//     //     }
//     // }
//     ADC1->SQR3 = ADC_CH1;
//     ADC1->SQR1 = ADC_SEQ_LEN;
//     ADC1->CR2 |= CR2_ADON;


// }

// void start_converstion(void){
//     ADC1->CR2 |= CR2_SWSTART;  //START ADC CONVERSION 
// }

// uint32_t adc_read(void){
//     while (!(ADC1->SR & SR_EOC))
//     {
        
//     }
//     return (ADC1->DR);
    
// }

#include "stm32f1xx.h"
#include <stdint.h>
#include "adc.h"

#define IOPAEN      (1U<<2)   // RCC APB2ENR: GPIOA clock enable
#define ADC1EN      (1U<<9)   // RCC APB2ENR: ADC1 clock enable

#define CR2_ADON    (1U<<0)
#define CR2_CAL     (1U<<2)
#define CR2_RSTCAL  (1U<<3)
#define CR2_SWSTART (1U<<22)  // NOTE: on F1, SWSTART is bit 22
#define SR_EOC      (1U<<1)

void pa1_adc_init(void)
{
    // 1) Enable clocks: GPIOA + ADC1
    RCC->APB2ENR |= IOPAEN;
    RCC->APB2ENR |= ADC1EN;

    // 2) ADC prescaler (ADCPRE) in RCC->CFGR bits 15:14
    // Set to PCLK2/6 (good default). Clear then set.
    RCC->CFGR &= ~(3U << 14);
    RCC->CFGR |=  (2U << 14);   // 10: PCLK2/6

    // 3) Set PA1 as Analog input: CRL bits for pin1 are [7:4]
    GPIOA->CRL &= ~(0xFU << 4); // MODE1=00, CNF1=00

    // 4) Set sample time for channel 1 (SMPR2 bits [5:3])
    // e.g. 239.5 cycles for stability (especially with higher source impedance)
    ADC1->SMPR2 &= ~(7U << 3);
    ADC1->SMPR2 |=  (7U << 3);

    // 5) Regular sequence length = 1 conversion
    ADC1->SQR1 = 0;      // L[23:20] = 0000 => 1 conversion
    ADC1->SQR3 = 1;      // 1st conversion = channel 1

    // 6) Turn on ADC
    ADC1->CR2 |= CR2_ADON;

    // Small delay after enabling ADC (optional but common)
    for (volatile int i = 0; i < 10000; i++) {}

    // 7) Reset calibration
    ADC1->CR2 |= CR2_RSTCAL;
    while (ADC1->CR2 & CR2_RSTCAL) {}

    // 8) Start calibration
    ADC1->CR2 |= CR2_CAL;
    while (ADC1->CR2 & CR2_CAL) {}
}

void start_converstion(void)
{
    // Start a conversion
    ADC1->CR2 |= CR2_SWSTART;
}

uint32_t adc_read(void)
{
    // If you want continuous reads, trigger each time:
    start_converstion();

    while (!(ADC1->SR & SR_EOC)) {}
    return ADC1->DR;
}
