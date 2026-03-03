// #include "stm32f1xx.h"

// #define TIM2EN      (1U<<0)
// #define CR1_CEN     (1U<<0)

// void tim2_1hz_init(void){
//     RCC->APB1ENR |= TIM2EN;
//     TIM2->PSC = 1600 - 1;
//     TIM2->ARR = 10000 - 1;
//     TIM2->CNT = 0;
//     TIM2->CR1 = CR1_CEN ;

// }

// void tim2_1hz_init(void){
//     RCC->APB1ENR |= TIM2EN;
//     TIM2->PSC = 1600 - 1;
//     TIM2->ARR = 10000 - 1;
//     TIM2->CNT = 0;
//     TIM2->CR1 = CR1_CEN ;
    
// }
#include "timers.h"
#include "stm32f1xx.h"

// Compute TIM2 clock from SystemCoreClock + APB1 prescaler.
// Works for the common STM32F1 clock tree.
static uint32_t tim2_get_clock_hz(void)
{
    // PPRE1 bits: RCC_CFGR[10:8]
    static const uint8_t apb_presc_table[8] = {1,1,1,1,2,4,8,16};

    uint32_t ppre1_bits = (RCC->CFGR >> 8) & 0x7U;
    uint32_t apb1_div   = apb_presc_table[ppre1_bits];

    uint32_t pclk1 = SystemCoreClock / apb1_div;

    // If APB1 prescaler != 1, timer clock = 2 * PCLK1
    return (apb1_div == 1U) ? pclk1 : (2U * pclk1);
}

void tim2_1hz_init(void)
{
    // 1) Enable TIM2 clock
    RCC->APB1ENR |= RCC_APB1ENR_TIM2EN;

    // 2) Stop timer during config
    TIM2->CR1 = 0;

    // We want: update_event = 1 Hz
    // Choose a 10 kHz timer tick, then ARR = 10000-1 => 1 Hz overflow
    const uint32_t tick_hz  = 10000U;  // timer counter frequency
    const uint32_t tim_clk  = tim2_get_clock_hz();

    uint32_t presc = tim_clk / tick_hz;     // (PSC+1)
    if (presc < 1U) presc = 1U;
    if (presc > 65536U) presc = 65536U;

    TIM2->PSC = (uint16_t)(presc - 1U);
    TIM2->ARR = (uint16_t)(tick_hz - 1U);

    TIM2->CNT = 0;

    // 3) Force load PSC/ARR immediately
    TIM2->EGR = TIM_EGR_UG;

    // UG may set UIF, so clear flags
    TIM2->SR = 0;

    // 4) Enable timer
    TIM2->CR1 |= TIM_CR1_CEN;
}