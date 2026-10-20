#include "stm32f1xx.h"

#include <stdio.h>

static void usart2_tx_init_115200(void);
static void usart2_write(int ch);

int __io_putchar(int ch) {
    usart2_write(ch);
    return ch;
}

int ultra_print_main(void) {
    usart2_tx_init_115200();
    while (1) {
        printf("hello its me stm32f103............\r\n");
    }
}

static void usart2_tx_init_115200(void) {
    // 1) Enable GPIOA + AFIO + USART2 clocks
    RCC->APB2ENR |= RCC_APB2ENR_IOPAEN | RCC_APB2ENR_AFIOEN;
    RCC->APB1ENR |= RCC_APB1ENR_USART2EN;

    // 2) PA2 = USART2_TX
    // CRL for PA2: CNF2[1:0]=10 (AF push-pull), MODE2[1:0]=10 (2 MHz)
    // Clear bits for PA2 (bits 11:8), then set
    GPIOA->CRL &= ~(0xF << (2 * 4));
    GPIOA->CRL |=  (0xA << (2 * 4)); // 0b1010

    // 3) Baud rate
    // If PCLK1 = 36MHz, 115200 => USARTDIV = 36,000,000/115,200 = 312.5
    // BRR = mantissa 312 (0x138), fraction .5 => 8 (0x8) because 0.5*16=8
    // BRR = 0x1388
    USART2->BRR = 0x1388;

    // 4) Enable transmitter + USART
    USART2->CR1 |= USART_CR1_TE;
    USART2->CR1 |= USART_CR1_UE;
}

static void usart2_write(int ch) {
    while (!(USART2->SR & USART_SR_TXE)) { }
    USART2->DR = (uint8_t)ch;
}



#include "uart.h"


#define SYSCLK_HZ   72000000UL
#define BAUDRATE    115200UL
#define LED         (1U<<10)
#define LED_PIN     LED



static void uart2_set_baud(uint32_t pclk1_hz, uint32_t baud)
{
    // USARTDIV = pclk / baud (oversampling by 16)
    // BRR format on F1: Mantissa in [15:4], Fraction in [3:0] (fraction out of 16)
    uint32_t usartdiv_x16 = (pclk1_hz + (baud / 2U)) / baud; // rounded (x16 already implicit below? see next lines)

    // Better: compute with fraction explicitly:
    // usartdiv = pclk / (16*baud)
    // mantissa = floor(usartdiv)
    // fraction = round((usartdiv - mantissa)*16)
    uint32_t div = (pclk1_hz + (baud/2U)) / baud; // pclk/baud (rounded)
    // Convert to BRR assuming oversampling by 16:
    // BRR = (pclk/baud) in this simplified integer form works for many common clocks,
    // but we will compute proper mantissa/fraction:
    uint32_t mant = pclk1_hz / (16U * baud);
    uint32_t frac = ((pclk1_hz % (16U * baud)) * 16U + (baud/2U)) / baud;

    if (frac > 15U) { mant += 1U; frac = 0U; }

    USART2->BRR = (mant << 4) | (frac & 0xFU);

    (void)usartdiv_x16;
    (void)div;
}

static void uart2_init(void)
{
    // 1) Enable clocks: GPIOA + AFIO on APB2, USART2 on APB1
    RCC->APB2ENR |= RCC_APB2ENR_IOPAEN | RCC_APB2ENR_AFIOEN;
    RCC->APB1ENR |= RCC_APB1ENR_USART2EN;

    // 2) Configure PA2 as Alternate Function Push-Pull, 2 MHz
    // F1 GPIO config per pin nibble: [CNF1 CNF0 MODE1 MODE0]
    // AF Push-Pull => CNF=10, Output 2MHz => MODE=10 => 0b1010 = 0xA
    GPIOA->CRL &= ~(0xFUL << (2U * 4U));      // clear PA2 config
    GPIOA->CRL |=  (0xAUL << (2U * 4U));      // set PA2 AF PP 2MHz

    // (Optional) PA3 as input floating for RX (not used here)
    // GPIOA->CRL &= ~(0xFUL << (3U * 4U));
    // GPIOA->CRL |=  (0x4UL << (3U * 4U));   // CNF=01 MODE=00 => floating input

    // 3) Set baud rate
    // On Blue Pill default: SYSCLK=72MHz, APB1 prescaler=2 => PCLK1=36MHz
    uint32_t pclk1 = 36000000UL;
    uart2_set_baud(pclk1, BAUDRATE);

    // 4) Enable USART, enable transmitter
    USART2->CR1 = 0;
    USART2->CR1 |= USART_CR1_TE;   // transmitter enable
    USART2->CR1 |= USART_CR1_UE;   // USART enable
}

static void uart2_write_char(char c)
{
    while (!(USART2->SR & USART_SR_TXE)) { } // wait TX empty
    USART2->DR = (uint16_t)c;
}

static void uart2_write_string(const char *s)
{
    while (*s) uart2_write_char(*s++);
}

static void delay(volatile uint32_t t)
{
    while (t--) { __asm__("nop"); }
}

int __io_putchar(int ch){
    uart2_write(ch);
    return ch;
}
