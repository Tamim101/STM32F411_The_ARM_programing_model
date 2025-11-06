#include "uart_dma.h"
#include "stm32f4xx.h"   // or your device header

#ifdef USE_FREERTOS
#include "FreeRTOS.h"
#include "semphr.h"
static SemaphoreHandle_t txDoneSem;
#endif

static volatile bool txBusy = false;

void uart2_dma_init(uint32_t baud)
{
    /* Clocks + GPIO AF: PA2=TX, PA3=RX (F4 example) */
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN;
    RCC->APB1ENR |= RCC_APB1ENR_USART2EN;
    RCC->AHB1ENR |= RCC_AHB1ENR_DMA1EN;

    GPIOA->MODER  &= ~((3u<<(2*2))|(3u<<(3*2)));
    GPIOA->MODER  |=  ((2u<<(2*2))|(2u<<(3*2)));   // AF
    GPIOA->AFR[0] |=  (7u<<(2*4)) | (7u<<(3*4));   // AF7

    /* UART */
    USART2->CR1 = 0;
   
    USART2->BRR = SystemCoreClock/2 / baud;       // adjust if different clock tree
    USART2->CR1 = USART_CR1_TE | USART_CR1_RE | USART_CR1_UE;
    USART2->CR3 = USART_CR3_DMAT;                 // enable DMA for TX

    
    DMA1_Stream6->CR &= ~DMA_SxCR_EN; while (DMA1_Stream6->CR & DMA_SxCR_EN) {}
    DMA1_Stream6->PAR   = (uint32_t)&USART2->DR;
    DMA1_Stream6->FCR   = 0;                      // direct mode OK
    DMA1_Stream6->CR    = (4u<<DMA_SxCR_CHSEL_Pos) | // CH4
                          DMA_SxCR_DIR_0 |        // mem->periph
                          DMA_SxCR_MINC |         // inc memory
                          DMA_SxCR_TCIE |         // complete IRQ
                          DMA_SxCR_PL_1;          // high priority
    NVIC_EnableIRQ(DMA1_Stream6_IRQn);
}

bool uart2_write_async(const uint8_t *buf, size_t len)
{
    if (txBusy || len==0) return false;
    txBusy = true;

    DMA1_Stream6->CR  &= ~DMA_SxCR_EN; while (DMA1_Stream6->CR & DMA_SxCR_EN) {}
    DMA1_Stream6->M0AR  = (uint32_t)buf;
    DMA1_Stream6->NDTR  = (uint32_t)len;
    DMA1_Stream6->CR   |= DMA_SxCR_EN;           // start
    return true;
}

bool uart2_is_busy(void){ return txBusy; }

#ifdef USE_FREERTOS
void uart2_bind_freertos(void){
    if (!txDoneSem) txDoneSem = xSemaphoreCreateBinary();
}

// Optional: blocking send for tasks (no busy loop)
bool uart2_write_async_blocking(const uint8_t *buf, size_t len, TickType_t to){
    if (!uart2_write_async(buf,len)) return false;
    if (xSemaphoreTake(txDoneSem, to) == pdTRUE) return true;
    return false;
}
#endif

void DMA1_Stream6_IRQHandler(void)
{
    if (DMA1->HISR & DMA_HISR_TCIF6) {
        DMA1->HIFCR = DMA_HIFCR_CTCIF6;
        DMA1_Stream6->CR &= ~DMA_SxCR_EN;
        txBusy = false;
#ifdef USE_FREERTOS
        BaseType_t hpw = pdFALSE;
        if (txDoneSem) xSemaphoreGiveFromISR(txDoneSem, &hpw);
        portYIELD_FROM_ISR(hpw);
#endif
    }
}
