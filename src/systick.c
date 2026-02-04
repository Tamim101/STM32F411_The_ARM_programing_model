#include "stm32f1xx.h"
#include <stdint.h>

#define SYSTICK_FREQ_HZ 72000000UL  // typical Blue Pill HCLK
#define TICKS_PER_MS    (SYSTICK_FREQ_HZ / 1000UL)

void systickDelayMs(uint32_t ms)
{
    // Use processor clock (AHB) as SysTick clock
    SysTick->CTRL = 0;                 // disable
    SysTick->LOAD = TICKS_PER_MS - 1;  // 1ms reload
    SysTick->VAL  = 0;                 // clear current
    SysTick->CTRL = SysTick_CTRL_ENABLE_Msk | SysTick_CTRL_CLKSOURCE_Msk;

    for (uint32_t i = 0; i < ms; i++) {
        // wait until COUNTFLAG is set
        while ((SysTick->CTRL & SysTick_CTRL_COUNTFLAG_Msk) == 0) { }
    }

    SysTick->CTRL = 0; // optional: stop SysTick after delay
}
