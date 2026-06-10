/*
 * ============================================================
 *  MQ-7 CO MONITOR — MINIMAL WORKING VERSION
 *  STM32F103C8T6 (Blue Pill) — Bare Metal, zero HAL
 *
 *  SIMPLE: No heating cycle. No calibration state machine.
 *  Just reads sensor every second, shows PPM, buzzes alarm.
 *
 *  WIRING:
 *  PA0  → MQ-7 AOUT: AOUT──10kΩ──PA0──10kΩ──GND
 *  PA2  → Buzzer (+) passive buzzer direct
 *  GND  → Buzzer (-)
 *  PB6  → LCD SCL
 *  PB7  → LCD SDA
 *  5V   → MQ-7 VCC + LCD VCC
 *  GND  → MQ-7 GND + LCD GND
 *
 *  FIRST BOOT:
 *  Shows "CALIBRATING..." for 10 seconds in clean air.
 *  Saves R0 to Flash. Never calibrates again.
 *
 *  NORMAL:              ALARM (CO >= 200 ppm):
 *  CO:   12 PPM         CO:  250 PPM
 *  STATUS: SAFE         !!! CO ALARM !!!
 * ============================================================
 */

#include <stdint.h>
#include <math.h>

/* ── Registers ─────────────────────────────────────────────── */
#define FLASH_ACR    (*(volatile uint32_t*)0x40022000UL)
#define FLASH_KEYR   (*(volatile uint32_t*)0x40022004UL)
#define FLASH_SR     (*(volatile uint32_t*)0x4002200CUL)
#define FLASH_CR     (*(volatile uint32_t*)0x40022010UL)
#define FLASH_AR     (*(volatile uint32_t*)0x40022014UL)
#define RCC_CR       (*(volatile uint32_t*)0x40021000UL)
#define RCC_CFGR     (*(volatile uint32_t*)0x40021004UL)
#define RCC_APB2ENR  (*(volatile uint32_t*)0x40021018UL)
#define AFIO_MAPR    (*(volatile uint32_t*)0x40010004UL)
#define GPIOA_CRL    (*(volatile uint32_t*)0x40010800UL)
#define GPIOA_IDR    (*(volatile uint32_t*)0x40010808UL)
#define GPIOA_BSRR   (*(volatile uint32_t*)0x40010810UL)
#define GPIOA_BRR    (*(volatile uint32_t*)0x40010814UL)
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

/* ── I2C ───────────────────────────────────────────────────── */
#define SCL_HIGH()  GPIOB_BSRR=(1U<<6)
#define SCL_LOW()   GPIOB_BRR =(1U<<6)
#define SDA_HIGH()  GPIOB_BSRR=(1U<<7)
#define SDA_LOW()   GPIOB_BRR =(1U<<7)
#define SDA_READ()  ((GPIOB_IDR>>7)&1U)

/* ── Settings ──────────────────────────────────────────────── */
#define FLASH_PAGE63     0x0800F800UL
#define CALIB_MAGIC      0xCAFE2001UL

#define MQ7_RL           10000.0f
#define MQ7_VSUPPLY      5.0f
#define MQ7_VREF         3.3f
#define MQ7_ADC_MAX      4095.0f
#define ADC_DIVIDER      2.0f       /* 10k+10k voltage divider */
#define CO_A             99.042f
#define CO_B             (-1.522f)
#define R0_DEFAULT       10000.0f

#define CO_ALARM_PPM     200.0f     /* alarm ON  threshold */
#define CO_CLEAR_PPM     150.0f     /* alarm OFF threshold */

/* Buzzer half-period count at 64MHz (~2400Hz) */
#define BUZZ_HALF        3333U

#define LCD_ADDR         0x27U
#define BL  0x08U
#define EN  0x04U
#define RS  0x01U

/* ── Globals ───────────────────────────────────────────────── */
static volatile uint32_t g_ms  = 0;
static float             g_R0  = R0_DEFAULT;
static uint8_t           g_lcd = LCD_ADDR;

/* ── SysTick ───────────────────────────────────────────────── */
void SysTick_Handler(void) { g_ms++; }

static void delay_ms(uint32_t ms)
{ uint32_t s=g_ms; while((g_ms-s)<ms); }

static void delay_us(uint32_t us)
{ volatile uint32_t n=us*8U; while(n--); }

/* ── Clock 64MHz HSI ───────────────────────────────────────── */
static void clock_init(void)
{
    FLASH_ACR = (1U<<4)|2U;
    RCC_CR   |= (1U<<0);
    while(!(RCC_CR&(1U<<1)));
    RCC_CFGR  = (4U<<8)|(3U<<14)|(0U<<16)|(14U<<18);
    RCC_CR   |= (1U<<24);
    while(!(RCC_CR&(1U<<25)));
    RCC_CFGR |= 2U;
    while((RCC_CFGR&(3U<<2))!=(2U<<2));
}

/* ── Buzzer GPIO toggle (~2400Hz) ──────────────────────────── */
static void buzz_ms(uint32_t ms)
{
    uint32_t pairs=(ms*8000UL)/BUZZ_HALF;
    for(uint32_t i=0;i<pairs;i++){
        GPIOA_BSRR=(1U<<2);
        volatile uint32_t n=BUZZ_HALF; while(n--);
        GPIOA_BRR =(1U<<2);
        n=BUZZ_HALF; while(n--);
    }
    GPIOA_BRR=(1U<<2);
}

static void beep(uint8_t count)
{
    for(uint8_t i=0;i<count;i++){
        buzz_ms(200); delay_ms(150);
    }
}

/* ── Flash ─────────────────────────────────────────────────── */
static void fl_unlock(void){
    if(FLASH_CR&(1U<<7)){
        FLASH_KEYR=0x45670123UL; FLASH_KEYR=0xCDEF89ABUL;
    }
}
static void fl_lock(void)  { FLASH_CR|=(1U<<7); }
static void fl_wait(void)  { while(FLASH_SR&1U); }

static void fl_erase(uint32_t a){
    fl_unlock();fl_wait();
    FLASH_CR|=(1U<<1);FLASH_AR=a;
    FLASH_CR|=(1U<<6);fl_wait();
    FLASH_CR&=~(1U<<1);
}
static void fl_w16(uint32_t a,uint16_t v){
    fl_unlock();fl_wait();FLASH_CR|=(1U<<0);
    *(volatile uint16_t*)a=v;fl_wait();FLASH_CR&=~(1U<<0);
}
static void fl_w32(uint32_t a,uint32_t v){
    fl_w16(a,(uint16_t)(v&0xFFFFU));
    fl_w16(a+2U,(uint16_t)(v>>16));
}
static float flash_load_r0(void){
    if(*(volatile uint32_t*)FLASH_PAGE63!=CALIB_MAGIC) return 0.0f;
    uint32_t b=*(volatile uint32_t*)(FLASH_PAGE63+4U);
    float r; uint8_t *s=(uint8_t*)&b,*d=(uint8_t*)&r;
    for(uint8_t i=0;i<4;i++) d[i]=s[i];
    return r;
}
static void flash_save_r0(float r0){
    uint32_t b; uint8_t *s=(uint8_t*)&r0,*d=(uint8_t*)&b;
    for(uint8_t i=0;i<4;i++) d[i]=s[i];
    fl_erase(FLASH_PAGE63);
    fl_w32(FLASH_PAGE63,CALIB_MAGIC);
    fl_w32(FLASH_PAGE63+4U,b);
    fl_lock();
}

/* ── Software I2C ──────────────────────────────────────────── */
static void i2c_init(void){
    GPIOB_CRL&=~(0xFFU<<24);
    GPIOB_CRL|=(0x6U<<24)|(0x6U<<28);
    SCL_HIGH();SDA_HIGH();delay_ms(10);
}
static void i2c_start(void){
    SDA_HIGH();delay_us(5);SCL_HIGH();delay_us(5);
    SDA_LOW(); delay_us(5);SCL_LOW(); delay_us(5);
}
static void i2c_stop(void){
    SDA_LOW();delay_us(5);SCL_HIGH();delay_us(5);SDA_HIGH();delay_us(5);
}
static uint8_t i2c_byte(uint8_t b){
    for(uint8_t i=0;i<8;i++){
        if(b&0x80U)SDA_HIGH();else SDA_LOW();
        delay_us(3);SCL_HIGH();delay_us(5);SCL_LOW();delay_us(3);b<<=1;
    }
    SDA_HIGH();delay_us(3);SCL_HIGH();delay_us(5);
    uint8_t a=(uint8_t)(1U-SDA_READ());
    SCL_LOW();delay_us(3);return a;
}
static void i2c_send(uint8_t addr,uint8_t data){
    i2c_start();
    if(i2c_byte((uint8_t)(addr<<1))) i2c_byte(data);
    i2c_stop();delay_us(100);
}
static uint8_t i2c_probe(uint8_t addr){
    i2c_start();uint8_t a=i2c_byte((uint8_t)(addr<<1));
    i2c_stop();delay_ms(1);return a;
}

/* ── LCD ───────────────────────────────────────────────────── */
static void lcd_pulse(uint8_t n){
    i2c_send(g_lcd,(uint8_t)(n|EN));delay_ms(2);
    i2c_send(g_lcd,(uint8_t)(n&~EN));delay_ms(2);
}
static void lcd_nib(uint8_t v,uint8_t m){
    lcd_pulse((uint8_t)((v&0xF0U)|BL|m));
    lcd_pulse((uint8_t)(((v<<4)&0xF0U)|BL|m));
}
static void lcd_cmd(uint8_t c){ lcd_nib(c,0);  }
static void lcd_chr(uint8_t c){ lcd_nib(c,RS); }
static void lcd_str(const char*s){ while(*s)lcd_chr((uint8_t)*s++); }
static void lcd_clr(void){ lcd_cmd(0x01);delay_ms(5); }
static void lcd_pos(uint8_t c,uint8_t r){
    lcd_cmd(0x80U|(uint8_t)(c+(r?0x40U:0U)));
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
    lcd_cmd(0x28);delay_ms(2);
    lcd_cmd(0x08);delay_ms(2);
    lcd_cmd(0x01);delay_ms(5);
    lcd_cmd(0x06);delay_ms(2);
    lcd_cmd(0x0C);delay_ms(2);
}
static void lcd_num(uint32_t v,uint8_t w){
    char b[12];uint8_t i=0;
    if(!v){b[i++]='0';}
    else{while(v){b[i++]=(char)('0'+v%10U);v/=10U;}}
    while(i<w)b[i++]=' ';
    for(uint8_t a=0,e=(uint8_t)(i-1);a<e;a++,e--)
    {char t=b[a];b[a]=b[e];b[e]=t;}
    b[i]='\0';lcd_str(b);
}

/* ── ADC ───────────────────────────────────────────────────── */
static void adc_init(void){
    GPIOA_CRL &= ~(0xFU<<0);
    ADC1_SMPR2  = (7U<<0);
    ADC1_SQR1=0U; ADC1_SQR3=0U;
    ADC1_CR2=(7U<<17)|(1U<<20);
    ADC1_CR2|=(1U<<0); delay_ms(5);
    ADC1_CR2|=(1U<<3); while(ADC1_CR2&(1U<<3));
    ADC1_CR2|=(1U<<2); while(ADC1_CR2&(1U<<2));
}
static uint16_t adc_read(void){
    ADC1_SR &=~(1U<<1);
    ADC1_CR2|= (1U<<22);
    while(!(ADC1_SR&(1U<<1)));
    return (uint16_t)(ADC1_DR&0xFFFU);
}
static uint16_t adc_avg(void){
    uint32_t s=0;
    for(uint8_t i=0;i<32;i++){s+=adc_read();delay_ms(5);}
    return (uint16_t)(s/32U);
}

/* ── Sensor math ───────────────────────────────────────────── */
static float adc_to_rs(uint16_t raw){
    float vp=((float)raw/MQ7_ADC_MAX)*MQ7_VREF;
    float vs=vp*ADC_DIVIDER;
    if(vs<0.02f)               vs=0.02f;
    if(vs>MQ7_VSUPPLY-0.02f)  vs=MQ7_VSUPPLY-0.02f;
    return MQ7_RL*(MQ7_VSUPPLY-vs)/vs;
}
static float rs_to_ppm(float rs){
    float ratio=rs/g_R0;
    if(ratio<0.01f) ratio=0.01f;
    if(ratio>10.0f) ratio=10.0f;
    float ppm=CO_A*powf(ratio,CO_B);
    if(ppm<0.0f)   ppm=0.0f;
    if(ppm>9999.0f)ppm=9999.0f;
    return ppm;
}
static float read_co(void){
    return rs_to_ppm(adc_to_rs(adc_avg()));
}

/* ── Calibrate: 100 readings average in clean air ─────────── */
static void do_calibration(void){
    float sum=0.0f; uint8_t count=0;
    for(uint8_t i=0;i<100;i++){
        float rs=adc_to_rs(adc_read());
        if(rs>500.0f&&rs<200000.0f){sum+=rs;count++;}
        delay_ms(100);
    }
    g_R0=(count>=50)?(sum/(float)count):R0_DEFAULT;
    flash_save_r0(g_R0);
}

/* ── LCD screens ───────────────────────────────────────────── */
static void screen_normal(float co){
    uint32_t v=(uint32_t)(co>9999.0f?9999.0f:co);
    lcd_pos(0,0); lcd_str("CO:  ");
    lcd_num(v,4); lcd_str(" PPM  ");
    lcd_pos(0,1); lcd_str("STATUS: SAFE    ");
}
static void screen_alarm(float co){
    uint32_t v=(uint32_t)(co>9999.0f?9999.0f:co);
    lcd_pos(0,0); lcd_str("CO:  ");
    lcd_num(v,4); lcd_str(" PPM  ");
    lcd_pos(0,1); lcd_str("!!! CO ALARM !!!");
}

/* ═══════════════════════════════════════════════════════════
 * MAIN
 * ═══════════════════════════════════════════════════════════ */
int main(void)
{
    clock_init();

    RCC_APB2ENR |= (1U<<0)|(1U<<2)|(1U<<3)|(1U<<9);
    AFIO_MAPR=(AFIO_MAPR&~(7U<<24))|(2U<<24);

    /* PA2: push-pull output (buzzer) */
    GPIOA_CRL &= ~(0xFU<<8);
    GPIOA_CRL |=  (0x3U<<8);
    GPIOA_BRR  =  (1U<<2);

    /* SysTick 1ms at 64MHz */
    STK_LOAD_R=64000U-1U;
    STK_VAL=0U; STK_CTRL=7U;

    delay_ms(300);
    i2c_init();
    adc_init();

    /* Find LCD */
    while(1){
        if(i2c_probe(0x27U)){g_lcd=0x27U;break;}
        delay_ms(100);
        if(i2c_probe(0x3FU)){g_lcd=0x3FU;break;}
        delay_ms(500);
    }

    lcd_init();
    lcd_clr();
    lcd_pos(0,0); lcd_str("  CO MONITOR    ");
    lcd_pos(0,1); lcd_str("   STARTING...  ");
    beep(2);
    delay_ms(1000);

    /* Check Flash for saved R0 */
    float saved=flash_load_r0();

    if(saved>500.0f && saved<200000.0f){
        /* R0 already saved — load and go straight to monitor */
        g_R0=saved;
        lcd_clr();
        lcd_pos(0,0); lcd_str("  CO MONITOR    ");
        lcd_pos(0,1); lcd_str(" CALIB LOADED   ");
        delay_ms(1500);
        lcd_clr();
    }else{
        /* First boot — calibrate in clean air */
        lcd_clr();
        lcd_pos(0,0); lcd_str(" CALIBRATING... ");
        lcd_pos(0,1); lcd_str("  CLEAN AIR!    ");
        do_calibration();
        lcd_clr();
        lcd_pos(0,0); lcd_str("  CALIB DONE!   ");
        lcd_pos(0,1); lcd_str("  MONITORING... ");
        beep(3);
        delay_ms(2000);
        lcd_clr();
    }

    float    co      = 0.0f;
    uint8_t  alarmed = 0;
    uint32_t lcd_ms  = g_ms;
    uint32_t buz_ms  = g_ms;

    screen_normal(co);

    /* ─────────────────────────────────────────────────────
     * MAIN LOOP
     * Read sensor every 1 second.
     * If CO >= 200 ppm → alarm ON (buzz every second).
     * If CO <  150 ppm → alarm OFF.
     * ───────────────────────────────────────────────────── */
    while(1)
    {
        /* Read sensor and update LCD every 1 second */
        if((g_ms-lcd_ms)>=1000U){
            lcd_ms=g_ms;
            co=read_co();

            if(!alarmed){
                screen_normal(co);
                /* Check if CO crossed alarm threshold */
                if(co>=CO_ALARM_PPM){
                    alarmed=1;
                    lcd_clr();
                    screen_alarm(co);
                    buz_ms=g_ms;   /* start buzzer immediately */
                }
            }else{
                screen_alarm(co);
                /* Check if CO dropped below clear threshold */
                if(co<CO_CLEAR_PPM){
                    alarmed=0;
                    GPIOA_BRR=(1U<<2);   /* PA2 LOW = silence */
                    lcd_clr();
                    lcd_pos(0,0); lcd_str("  GAS CLEARED   ");
                    lcd_pos(0,1); lcd_str("  SAFE NOW      ");
                    delay_ms(2000);
                    lcd_clr();
                    screen_normal(co);
                    lcd_ms=g_ms;
                }
            }
        }

        /* Buzzer pattern during alarm:
         * 500ms ON → 500ms OFF → repeat */
        if(alarmed){
            if((g_ms-buz_ms)>=1000U){
                buz_ms=g_ms;
                buzz_ms(500U);   /* 500ms tone */
                /* 500ms silence happens naturally as
                 * loop runs sensor read etc.          */
            }
        }
    }

    return 0;
}