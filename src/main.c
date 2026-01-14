#define PERIPH_BASE                  (0x40000000UL)
#define AHBPERIPH_OFFSET             (0X00020000UL)
#define AHBPERIPH_BASE               (PERIPH_BASE + AHBPERIPH_BASE)
#define GPIOA_OFFSET                 (0x0000UL)

#define GPIOA_BASE                   (AHBPERIPH_BASE + AHBPERIPH_OFFSET)

#define RCC_OFFSET                   (0x3800UL)
#define RCC_BASE                     (AHBPERIPH_BASE + RCC_OFFSET)

#define  AHB1EN_R_OFFSET             (0x30UL)
#define  GPIOA_MODE_R                (*(volatile unsigned int *) (GPIOA_BASE + GPIOA_MODE_R))

#define OD_R_OFFSET                  (0x14UL)
#define GPIOA_OD_R                   (*(volatile unsigned int *) (GPIOA_BASE + OD_R_OFFSET))

#define GPIOAEN                      (1U<<0)
#define LED_PIN                      PIN5