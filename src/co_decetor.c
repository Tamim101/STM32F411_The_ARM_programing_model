/*
 * SENSOR RAW TEST — shows pure ADC number, no math
 * STM32F103C8T6 Blue Pill
 *
 * platformio.ini:
 *   [env:genericSTM32F103C8]
 *   platform  = ststm32
 *   board     = genericSTM32F103C8
 *   framework = cmsis
 *
 * WIRING:
 *   PA0 → MQ-9 A_OUT
 *   PB6 → LCD SCL
 *   PB7 → LCD SDA
 *   5V  → MQ-9 VCC + LCD VCC
 *   GND → MQ-9 GND + LCD GND
 *
 * WHAT IT SHOWS:
 *   Line 0: "RAW: XXXX      "   (0 to 4095)
 *   Line 1: "V: X.XXV       "   (0.00 to 3.30V)
 *
 * HOW TO INTERPRET:
 *   Clean air      → RAW 200 – 800
 *   Gas nearby     → RAW goes UP (800 – 3000)
 *   No connection  → RAW 0 or 4095 always
 *
 * THIS CODE HAS ZERO MATH — just raw sensor value.
 * If RAW changes when you bring gas near → sensor works.
 * If RAW never changes → wiring problem on PA0.
 */

#include <stdint.h>

#define FLASH_ACR    (*(volatile uint32_t*)0x40022000UL)
#define RCC_CR       (*(volatile uint32_t*)0x40021000UL)
#define RCC_CFGR     (*(volatile uint32_t*)0x40021004UL)
#define RCC_APB2ENR  (*(volatile uint32_t*)0x40021018UL)
#define AFIO_MAPR    (*(volatile uint32_t*)0x40010004UL)
#define GPIOA_CRL    (*(volatile uint32_t*)0x40010800UL)
#define GPIOB_CRL    (*(volatile uint32_t*)0x40010C00UL)
#define GPIOB_IDR    (*(volatile uint32_t*)0x40010C08UL)
#define GPIOB_BSRR   (*(volatile uint32_t*)0x40010C10UL)
#define GPIOB_BRR    (*(volatile uint32_t*)0x40010C14UL)
#define ADC1_SR      (*(volatile uint32_t*)0x40012400UL)
#define ADC1_CR2     (*(volatile uint32_t*)0x40012408UL)
#define ADC1_SMPR2   (*(volatile uint32_t*)0x40012414UL)
#define ADC1_SQR1    (*(volatile uint32_t*)0x4001242CUL)
#define ADC1_SQR3    (*(volatile uint32_t*)0x40012434UL)
#define ADC1_DR      (*(volatile uint32_t*)0x4001244CUL)
#define STK_CTRL     (*(volatile uint32_t*)0xE000E010UL)
#define STK_LOAD_R   (*(volatile uint32_t*)0xE000E014UL)
#define STK_VAL      (*(volatile uint32_t*)0xE000E018UL)

#define SCL_HIGH()  GPIOB_BSRR=(1U<<6)
#define SCL_LOW()   GPIOB_BRR =(1U<<6)
#define SDA_HIGH()  GPIOB_BSRR=(1U<<7)
#define SDA_LOW()   GPIOB_BRR =(1U<<7)
#define SDA_READ()  ((GPIOB_IDR>>7)&1U)

static volatile uint32_t g_ms=0;
void SysTick_Handler(void){g_ms++;}
static void delay_ms(uint32_t ms){uint32_t s=g_ms;while((g_ms-s)<ms);}
static void delay_us(uint32_t us){volatile uint32_t n=us*8U;while(n--);}

static void clock_init(void){
    FLASH_ACR=(1U<<4)|2U;
    RCC_CR|=(1U<<0);while(!(RCC_CR&(1U<<1)));
    RCC_CFGR=(4U<<8)|(2U<<14)|(0U<<16)|(14U<<18);
    RCC_CR|=(1U<<24);while(!(RCC_CR&(1U<<25)));
    RCC_CFGR|=2U;while((RCC_CFGR&(3U<<2))!=(2U<<2));
}

/* I2C */
static void i2c_gpio_init(void){
    GPIOB_CRL&=~(0xFFU<<24);
    GPIOB_CRL|=(0x6U<<24)|(0x6U<<28);
    SCL_HIGH();SDA_HIGH();delay_ms(10);
}
static void i2c_start(void){
    SDA_HIGH();delay_us(5);SCL_HIGH();delay_us(5);
    SDA_LOW(); delay_us(5);SCL_LOW(); delay_us(5);
}
static void i2c_stop(void){
    SDA_LOW(); delay_us(5);SCL_HIGH();delay_us(5);SDA_HIGH();delay_us(5);
}
static uint8_t i2c_write_byte(uint8_t b){
    for(uint8_t i=0;i<8;i++){
        if(b&0x80U)SDA_HIGH();else SDA_LOW();
        delay_us(3);SCL_HIGH();delay_us(5);SCL_LOW();delay_us(3);b<<=1;
    }
    SDA_HIGH();delay_us(3);SCL_HIGH();delay_us(5);
    uint8_t ack=(uint8_t)(1U-SDA_READ());
    SCL_LOW();delay_us(3);return ack;
}
static void i2c_send(uint8_t addr,uint8_t data){
    i2c_start();
    if(i2c_write_byte((uint8_t)(addr<<1)))i2c_write_byte(data);
    i2c_stop();delay_us(100);
}
static uint8_t i2c_probe(uint8_t addr){
    i2c_start();uint8_t a=i2c_write_byte((uint8_t)(addr<<1));
    i2c_stop();delay_ms(1);return a;
}

/* LCD */
#define BL 0x08U
#define EN 0x04U
#define RS 0x01U
static uint8_t g_lcd=0x27U;
static void lcd_pulse(uint8_t n){
    i2c_send(g_lcd,(uint8_t)(n|EN));delay_ms(2);
    i2c_send(g_lcd,(uint8_t)(n&~EN));delay_ms(2);
}
static void lcd_write(uint8_t v,uint8_t m){
    lcd_pulse((uint8_t)((v&0xF0U)|BL|m));
    lcd_pulse((uint8_t)(((v<<4)&0xF0U)|BL|m));
}
static void lcd_cmd(uint8_t c)  {lcd_write(c,0);}
static void lcd_char(uint8_t c) {lcd_write(c,RS);}
static void lcd_str(const char*s){while(*s)lcd_char((uint8_t)*s++);}
static void lcd_clear(void)     {lcd_cmd(0x01);delay_ms(5);}
static void lcd_pos(uint8_t col,uint8_t row){
    lcd_cmd(0x80U|(uint8_t)(col+(row?0x40U:0x00U)));
}
static void lcd_init(void){
    delay_ms(200);
    i2c_send(g_lcd,(uint8_t)(0x30U|BL|EN));delay_ms(5);
    i2c_send(g_lcd,(uint8_t)(0x30U|BL));   delay_ms(2);
    i2c_send(g_lcd,(uint8_t)(0x30U|BL|EN));delay_ms(2);
    i2c_send(g_lcd,(uint8_t)(0x30U|BL));   delay_ms(2);
    i2c_send(g_lcd,(uint8_t)(0x30U|BL|EN));delay_ms(2);
    i2c_send(g_lcd,(uint8_t)(0x30U|BL));   delay_ms(2);
    i2c_send(g_lcd,(uint8_t)(0x20U|BL|EN));delay_ms(2);
    i2c_send(g_lcd,(uint8_t)(0x20U|BL));   delay_ms(2);
    lcd_cmd(0x28);delay_ms(2);lcd_cmd(0x08);delay_ms(2);
    lcd_cmd(0x01);delay_ms(5);lcd_cmd(0x06);delay_ms(2);
    lcd_cmd(0x0C);delay_ms(2);
}
static void lcd_int(int32_t v,uint8_t w){
    char buf[12];uint8_t i=0;
    uint32_t u=(v<0)?(uint32_t)(-v):(uint32_t)v;
    if(u==0U)buf[i++]='0';
    while(u>0U){buf[i++]=(char)('0'+(u%10U));u/=10U;}
    if(v<0)buf[i++]='-';
    while(i<w)buf[i++]=' ';
    for(uint8_t a=0,b=(uint8_t)(i-1U);a<b;a++,b--){
        char t=buf[a];buf[a]=buf[b];buf[b]=t;
    }
    buf[i]='\0';lcd_str(buf);
}

/* ADC */
static void adc_init(void){
    GPIOA_CRL&=~(0xFU<<0);          /* PA0 analog input */
    ADC1_SMPR2=(7U<<0);              /* max sample time  */
    ADC1_SQR1=0U; ADC1_SQR3=0U;
    ADC1_CR2=(7U<<17)|(1U<<20);
    ADC1_CR2|=(1U<<0); delay_ms(5);
    ADC1_CR2|=(1U<<3); while(ADC1_CR2&(1U<<3));
    ADC1_CR2|=(1U<<2); while(ADC1_CR2&(1U<<2));
}
static uint16_t adc_read(void){
    ADC1_CR2|=(1U<<0)|(1U<<22);
    while(!(ADC1_SR&(1U<<1)));
    return (uint16_t)(ADC1_DR&0xFFFU);
}

int main(void){
    clock_init();
    RCC_APB2ENR|=(1U<<0)|(1U<<2)|(1U<<3)|(1U<<9);
    AFIO_MAPR=(AFIO_MAPR&~(7U<<24))|(2U<<24);

    STK_LOAD_R=64000U-1U;   /* 1ms SysTick */
    STK_VAL=0U; STK_CTRL=7U;
    delay_ms(300);

    i2c_gpio_init();
    adc_init();

    /* find LCD */
    while(1){
        if(i2c_probe(0x27U)){g_lcd=0x27U;break;}
        delay_ms(100);
        if(i2c_probe(0x3FU)){g_lcd=0x3FU;break;}
        delay_ms(500);
    }
    lcd_init();
    lcd_clear();

    lcd_pos(0,0); lcd_str("SENSOR RAW TEST ");
    lcd_pos(0,1); lcd_str("Bring gas near..");
    delay_ms(3000);
    lcd_clear();

    uint32_t last=g_ms;

    while(1){
        if((g_ms-last)>=500U){   /* update every 0.5 seconds */
            last=g_ms;

            uint16_t raw=adc_read();

            /* voltage in mV */
            uint32_t mv=((uint32_t)raw*3300U)/4095U;

            /* Line 0: raw ADC number */
            lcd_pos(0,0);
            lcd_str("RAW:");
            lcd_int((int32_t)raw,4);
            lcd_str("        ");

            /* Line 1: voltage */
            lcd_pos(0,1);
            lcd_str("V: ");
            lcd_int((int32_t)(mv/1000U),1);
            lcd_char('.');
            uint32_t frac=(mv%1000U)/10U;
            if(frac<10U) lcd_char('0');
            lcd_int((int32_t)frac,2);
            lcd_str("V          ");
        }
    }
    return 0;
}