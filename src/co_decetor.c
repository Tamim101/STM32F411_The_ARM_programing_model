/*
 * ============================================================
 *  MQ-9 CO / LPG GAS MONITOR
 *  STM32F103C8T6 (Blue Pill) — Pure Bare Metal, zero HAL
 *
 *  BUZZER: GPIO toggle loop on PA2 — CONFIRMED WORKING METHOD
 *  Same technique as your test code that produced sound.
 *  No TIM2/TIM3 registers. No alternate function. Just toggle.
 *
 *  WIRING (unchanged):
 *  PA0  → MQ-9 AOUT: AOUT──10kΩ──PA0──20kΩ──GND
 *  PA1  → Recalibrate button (PA1 to GND = force recalib)
 *  PA2  → Buzzer (+)   ← plain push-pull GPIO, NOT AF
 *  GND  → Buzzer (-)
 *  PB6  → LCD SCL
 *  PB7  → LCD SDA
 *  5V   → MQ-9 VCC + LCD VCC
 *  GND  → MQ-9 GND + LCD GND
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
#define RCC_APB1ENR  (*(volatile uint32_t*)0x4002101CUL)
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
#define FLASH_PAGE63        0x0800F800UL
#define CALIB_MAGIC         0xBEEF4805UL
#define WARMUP_SEC          300U

#define CO_ALARM_PPM        200.0f
#define CO_CLEAR_PPM        195.0f
#define LPG_ALARM_PPM       1000.0f
#define LPG_CLEAR_PPM       950.0f

#define ADC_DIVIDER_RATIO   1.5f
#define MQ9_RL              10000.0f
#define MQ9_VSUPPLY         5.0f
#define MQ9_VREF            3.3f
#define MQ9_ADC_MAX         4095.0f
#define MQ9_R0_DEFAULT      192102.0f
#define CO_A                100.0f
#define CO_B                (-1.513f)
#define LPG_A               4.4f
#define LPG_B               (-1.265f)
#define LCD_ADDR_DEFAULT    0x27U
#define NORMAL_SCREEN_MS    3000U
#define DEBUG_SCREEN_MS     3000U

/*
 * ── BUZZER FREQUENCY ───────────────────────────────────────
 * At 64 MHz, loop body ≈ 4 cycles = 0.0625 µs per iteration.
 * Half-period count for target frequency:
 *   half_period_us = 1,000,000 / (2 × freq)
 *   loop_count     = half_period_us / 0.0625 = half_period_us × 16
 *
 * 2000 Hz → half=250µs → count=4000
 * 2400 Hz → half=208µs → count=3333
 * 3000 Hz → half=167µs → count=2667
 * 4000 Hz → half=125µs → count=2000
 *
 * Change BUZZ_HALF_COUNT to tune your buzzer frequency.
 * Try each value and use whichever sounds loudest.
 */
#define BUZZ_HALF_COUNT     3333U   /* ~2400 Hz — change to tune */

/* ── SysTick 1ms ───────────────────────────────────────────── */
static volatile uint32_t g_ms = 0;
void SysTick_Handler(void) { g_ms++; }

static void delay_ms(uint32_t ms)
{ uint32_t s=g_ms; while((g_ms-s)<ms); }

static void delay_us(uint32_t us)
{ volatile uint32_t n=us*8U; while(n--); }

/* ─────────────────────────────────────────────────────────────
 * BUZZER — GPIO toggle on PA2
 *
 * Exactly the same method as your working test code.
 * PA2 = plain push-pull output (0x3), NOT alternate function.
 *
 * buzz_ms(duration_ms):
 *   Toggles PA2 at ~2400 Hz for the given number of ms.
 *   Blocks until done — used for short beeps only.
 *
 * buzz_cycle_ms(duration_ms):
 *   Call this repeatedly in the alarm loop.
 *   Each call buzzes for duration_ms then returns.
 *   Put it in your 1-second loop to keep buzzer going.
 * ───────────────────────────────────────────────────────────── */

/*
 * Buzz PA2 for exactly duration_ms milliseconds.
 * Blocking — CPU toggles pin continuously during this time.
 * Used for beeps (short durations: 200ms, 300ms).
 */
static void buzz_ms(uint32_t duration_ms)
{
    /* cycles = how many full toggle pairs fit in duration_ms
     * Each pair = 2 half-periods
     * half-period ≈ BUZZ_HALF_COUNT × 0.0625µs
     * full period  ≈ BUZZ_HALF_COUNT × 0.125µs
     * pairs per ms = 1000µs / (BUZZ_HALF_COUNT × 0.125µs) */
    uint32_t pairs = (duration_ms * 8000UL) / BUZZ_HALF_COUNT;

    for(uint32_t i = 0; i < pairs; i++){
        GPIOA_BSRR = (1U<<2);                      /* PA2 HIGH */
        volatile uint32_t n = BUZZ_HALF_COUNT;
        while(n--);
        GPIOA_BRR  = (1U<<2);                      /* PA2 LOW  */
        n = BUZZ_HALF_COUNT;
        while(n--);
    }
    GPIOA_BRR = (1U<<2);                           /* end LOW  */
}

/*
 * Single beep helper.
 * Used for startup and calibration confirmation sounds.
 */
static void beep(uint8_t count)
{
    for(uint8_t i=0; i<count; i++){
        buzz_ms(200);
        delay_ms(150);
    }
}

/* ── Clock 64 MHz ──────────────────────────────────────────── */
static void clock_init(void)
{
    FLASH_ACR = (1U<<4)|2U;
    RCC_CR |= (1U<<0); while(!(RCC_CR&(1U<<1)));
    RCC_CFGR = (4U<<8)|(3U<<14)|(0U<<16)|(14U<<18);
    RCC_CR |= (1U<<24); while(!(RCC_CR&(1U<<25)));
    RCC_CFGR |= 2U;
    while((RCC_CFGR&(3U<<2))!=(2U<<2));
}

/* ── Flash ─────────────────────────────────────────────────── */
static void flash_unlock(void)
{
    if(FLASH_CR&(1U<<7)){
        FLASH_KEYR=0x45670123UL;
        FLASH_KEYR=0xCDEF89ABUL;
    }
}
static void flash_lock(void)  { FLASH_CR|=(1U<<7); }
static void flash_wait(void)  { while(FLASH_SR&1U); }

static void flash_erase_page(uint32_t addr)
{
    flash_unlock();flash_wait();
    FLASH_CR|=(1U<<1);FLASH_AR=addr;
    FLASH_CR|=(1U<<6);flash_wait();
    FLASH_CR&=~(1U<<1);
}

static void flash_write16(uint32_t addr,uint16_t val)
{
    flash_unlock();flash_wait();
    FLASH_CR|=(1U<<0);
    *(volatile uint16_t*)addr=val;
    flash_wait();FLASH_CR&=~(1U<<0);
}

static void flash_write32(uint32_t addr,uint32_t val)
{
    flash_write16(addr,    (uint16_t)(val&0xFFFFU));
    flash_write16(addr+2U, (uint16_t)(val>>16));
}

static float flash_read_r0(void)
{
    uint32_t magic=*(volatile uint32_t*)(FLASH_PAGE63);
    uint32_t bits =*(volatile uint32_t*)(FLASH_PAGE63+4U);
    if(magic!=CALIB_MAGIC) return 0.0f;
    float r0;
    uint8_t *s=(uint8_t*)&bits,*d=(uint8_t*)&r0;
    for(uint8_t i=0;i<4;i++) d[i]=s[i];
    return r0;
}

static void flash_save_r0(float r0)
{
    uint32_t bits;
    uint8_t *s=(uint8_t*)&r0,*d=(uint8_t*)&bits;
    for(uint8_t i=0;i<4;i++) d[i]=s[i];
    flash_erase_page(FLASH_PAGE63);
    flash_write32(FLASH_PAGE63,    CALIB_MAGIC);
    flash_write32(FLASH_PAGE63+4U, bits);
    flash_lock();
}

/* ── Software I2C ──────────────────────────────────────────── */
static void i2c_gpio_init(void)
{
    GPIOB_CRL &= ~(0xFFU<<24);
    GPIOB_CRL |=  (0x6U<<24)|(0x6U<<28);
    SCL_HIGH(); SDA_HIGH(); delay_ms(10);
}

static void i2c_start(void)
{ SDA_HIGH();delay_us(5);SCL_HIGH();delay_us(5);
  SDA_LOW(); delay_us(5);SCL_LOW(); delay_us(5); }

static void i2c_stop(void)
{ SDA_LOW();delay_us(5);SCL_HIGH();delay_us(5);SDA_HIGH();delay_us(5); }

static uint8_t i2c_write_byte(uint8_t b)
{
    for(uint8_t i=0;i<8;i++){
        if(b&0x80U){SDA_HIGH();}else{SDA_LOW();}
        delay_us(3);SCL_HIGH();delay_us(5);SCL_LOW();delay_us(3);b<<=1;
    }
    SDA_HIGH();delay_us(3);SCL_HIGH();delay_us(5);
    uint8_t ack=(uint8_t)(1U-SDA_READ());
    SCL_LOW();delay_us(3);return ack;
}

static void i2c_send(uint8_t addr,uint8_t data)
{
    i2c_start();
    if(i2c_write_byte((uint8_t)(addr<<1))) i2c_write_byte(data);
    i2c_stop();delay_us(100);
}

static uint8_t i2c_probe(uint8_t addr)
{
    i2c_start();
    uint8_t ack=i2c_write_byte((uint8_t)(addr<<1));
    i2c_stop();delay_ms(1);return ack;
}

/* ── LCD ───────────────────────────────────────────────────── */
#define BL 0x08U
#define EN 0x04U
#define RS 0x01U
static uint8_t g_lcd=LCD_ADDR_DEFAULT;

static void lcd_pulse(uint8_t n)
{ i2c_send(g_lcd,(uint8_t)(n|EN));delay_ms(2);
  i2c_send(g_lcd,(uint8_t)(n&~EN));delay_ms(2); }

static void lcd_nibble(uint8_t v,uint8_t mode)
{
    lcd_pulse((uint8_t)((v&0xF0U)|BL|mode));
    lcd_pulse((uint8_t)(((v<<4)&0xF0U)|BL|mode));
}

static void lcd_cmd(uint8_t c) { lcd_nibble(c,0);  }
static void lcd_chr(uint8_t c) { lcd_nibble(c,RS); }
static void lcd_str(const char*s){ while(*s)lcd_chr((uint8_t)*s++); }
static void lcd_clear(void){ lcd_cmd(0x01);delay_ms(5); }
static void lcd_pos(uint8_t col,uint8_t row)
{ lcd_cmd(0x80U|(uint8_t)(col+(row?0x40U:0x00U))); }

static void lcd_init(void)
{
    delay_ms(200);
    i2c_send(g_lcd,(uint8_t)(0x30U|BL|EN));delay_ms(5);
    i2c_send(g_lcd,(uint8_t)(0x30U|BL));    delay_ms(2);
    i2c_send(g_lcd,(uint8_t)(0x30U|BL|EN));delay_ms(2);
    i2c_send(g_lcd,(uint8_t)(0x30U|BL));    delay_ms(2);
    i2c_send(g_lcd,(uint8_t)(0x30U|BL|EN));delay_ms(2);
    i2c_send(g_lcd,(uint8_t)(0x30U|BL));    delay_ms(2);
    i2c_send(g_lcd,(uint8_t)(0x20U|BL|EN));delay_ms(2);
    i2c_send(g_lcd,(uint8_t)(0x20U|BL));    delay_ms(2);
    lcd_cmd(0x28);delay_ms(2);
    lcd_cmd(0x08);delay_ms(2);
    lcd_cmd(0x01);delay_ms(5);
    lcd_cmd(0x06);delay_ms(2);
    lcd_cmd(0x0C);delay_ms(2);
}

static void lcd_int(int32_t v,uint8_t w)
{
    char buf[12];uint8_t i=0;
    uint32_t u=(v<0)?(uint32_t)(-v):(uint32_t)v;
    if(u==0U){buf[i++]='0';}
    else{while(u>0U){buf[i++]=(char)('0'+(u%10U));u/=10U;}}
    if(v<0)buf[i++]='-';
    while(i<w)buf[i++]=' ';
    for(uint8_t a=0,b=(uint8_t)(i-1);a<b;a++,b--)
    {char t=buf[a];buf[a]=buf[b];buf[b]=t;}
    buf[i]='\0';lcd_str(buf);
}

static void lcd_uint(uint32_t v,uint8_t w)
{
    char buf[12];uint8_t i=0;
    if(v==0U){buf[i++]='0';}
    else{while(v>0U){buf[i++]=(char)('0'+(v%10U));v/=10U;}}
    while(i<w)buf[i++]=' ';
    for(uint8_t a=0,b=(uint8_t)(i-1);a<b;a++,b--)
    {char t=buf[a];buf[a]=buf[b];buf[b]=t;}
    buf[i]='\0';lcd_str(buf);
}

/* ── ADC ───────────────────────────────────────────────────── */
static void adc_init(void)
{
    GPIOA_CRL &= ~(0xFU<<0);
    ADC1_SMPR2  = (7U<<0);
    ADC1_SQR1=0U;ADC1_SQR3=0U;
    ADC1_CR2=(7U<<17)|(1U<<20);
    ADC1_CR2|=(1U<<0);delay_ms(5);
    ADC1_CR2|=(1U<<3);while(ADC1_CR2&(1U<<3));
    ADC1_CR2|=(1U<<2);while(ADC1_CR2&(1U<<2));
}

static uint16_t adc_read_single(void)
{
    ADC1_SR &=~(1U<<1);
    ADC1_CR2|= (1U<<22);
    while(!(ADC1_SR&(1U<<1)));
    return (uint16_t)(ADC1_DR&0xFFFU);
}

static uint16_t adc_average(void)
{
    uint32_t sum=0;
    for(uint8_t i=0;i<32;i++){sum+=adc_read_single();delay_ms(5);}
    return (uint16_t)(sum/32U);
}

/* ── MQ-9 Math ─────────────────────────────────────────────── */
static float g_R0=MQ9_R0_DEFAULT;

static float adc_to_rs(uint16_t raw)
{
    float v_pa0=((float)raw/MQ9_ADC_MAX)*MQ9_VREF;
    float v_mq =v_pa0*ADC_DIVIDER_RATIO;
    if(v_mq<0.01f)               v_mq=0.01f;
    if(v_mq>MQ9_VSUPPLY-0.01f)  v_mq=MQ9_VSUPPLY-0.01f;
    return MQ9_RL*(MQ9_VSUPPLY-v_mq)/v_mq;
}

static float rs_to_ppm(float rs,float A,float B)
{
    float ratio=rs/g_R0;
    if(ratio<0.01f) ratio=0.01f;
    if(ratio>100.0f)ratio=100.0f;
    float ppm=A*powf(ratio,B);
    if(ppm<0.0f)   ppm=0.0f;
    if(ppm>9999.0f)ppm=9999.0f;
    return ppm;
}

static void read_both_ppm(float*co,float*lpg,float*rs_out)
{
    uint16_t raw=adc_average();
    float    rs =adc_to_rs(raw);
    *co    =rs_to_ppm(rs,CO_A, CO_B);
    *lpg   =rs_to_ppm(rs,LPG_A,LPG_B);
    *rs_out=rs;
}

static void do_calibration(void)
{
    float sum=0.0f;uint8_t count=0;
    for(uint8_t i=0;i<100U;i++){
        float rs=adc_to_rs(adc_read_single());
        if(rs>1000.0f&&rs<2000000.0f){sum+=rs;count++;}
        delay_ms(100);
    }
    g_R0=(count>=50U)?(sum/(float)count):MQ9_R0_DEFAULT;
    flash_save_r0(g_R0);
}

/* ── LCD Screens ───────────────────────────────────────────── */
static void screen_warmup(uint32_t sec_left)
{
    lcd_pos(0,0);lcd_str("  CO MONITOR    ");
    lcd_pos(0,1);lcd_str("WARM UP ");
    lcd_int((int32_t)sec_left,3);lcd_chr('s');
    lcd_str("     ");
}

static void screen_normal(float co,float lpg)
{
    lcd_pos(0,0);lcd_str("CO: ");
    lcd_int((int32_t)(co >9999.0f?9999.0f:co), 4);
    lcd_str(" PPM    ");
    lcd_pos(0,1);lcd_str("LPG:");
    lcd_int((int32_t)(lpg>9999.0f?9999.0f:lpg),4);
    lcd_str(" PPM    ");
}

static void screen_debug(float rs,float r0)
{
    lcd_pos(0,0);lcd_str("Rs:");
    lcd_uint((uint32_t)(rs>9999999.0f?9999999.0f:rs),7);
    lcd_pos(0,1);lcd_str("R0:");
    lcd_uint((uint32_t)(r0>9999999.0f?9999999.0f:r0),7);
}

static void screen_alarm(float co,float lpg,uint8_t ca,uint8_t la)
{
    lcd_pos(0,0);lcd_str("CO: ");
    lcd_int((int32_t)(co >9999.0f?9999.0f:co), 4);
    lcd_str(ca?"!!!ALRM ":" PPM    ");
    lcd_pos(0,1);lcd_str("LPG:");
    lcd_int((int32_t)(lpg>9999.0f?9999.0f:lpg),4);
    lcd_str(la?"!!!ALRM ":" PPM    ");
}

/* ── Main ──────────────────────────────────────────────────── */
typedef enum{
    STATE_WARMUP=0,STATE_CALIB=1,STATE_MONITOR=2,STATE_ALARM=3
}State_t;

int main(void)
{
    clock_init();

    RCC_APB2ENR |= (1U<<0)|(1U<<2)|(1U<<3)|(1U<<9);

    AFIO_MAPR=(AFIO_MAPR&~(7U<<24))|(2U<<24);

    /*
     * PA1: input pull-up (recalibrate button)
     * CRL bits[7:4] = 1000, then BSRR bit1 = pull-up
     */
    GPIOA_CRL &= ~(0xFU<<4);
    GPIOA_CRL |=  (0x8U<<4);
    GPIOA_BSRR = (1U<<1);

    /*
     * PA2: plain push-pull output 50MHz — same as working test
     * CRL bits[11:8] = 0011
     */
    GPIOA_CRL &= ~(0xFU<<8);
    GPIOA_CRL |=  (0x3U<<8);
    GPIOA_BRR  =  (1U<<2);     /* start LOW */

    /* SysTick 1ms at 64MHz */
    STK_LOAD_R=64000U-1U;
    STK_VAL=0U;STK_CTRL=7U;

    delay_ms(300);
    i2c_gpio_init();
    adc_init();

    /* Find LCD */
    while(1){
        if(i2c_probe(0x27U)){g_lcd=0x27U;break;}
        delay_ms(100);
        if(i2c_probe(0x3FU)){g_lcd=0x3FU;break;}
        delay_ms(500);
    }

    lcd_init();
    lcd_clear();
    lcd_pos(0,0);lcd_str("  CO MONITOR    ");
    lcd_pos(0,1);lcd_str("   STARTING...  ");

    /* Double beep: power-on confirmation */
    beep(2);

    /* ── Decide initial state ── */
    float saved_r0=flash_read_r0();
    uint8_t force_recal=(uint8_t)(((GPIOA_IDR>>1)&1U)==0U);

    State_t  state;
    uint32_t warmup_start=g_ms;
    uint32_t last_lcd_ms =g_ms;
    uint32_t screen_ms   =g_ms;
    uint8_t  show_debug  =0;
    float    co_ppm=0.0f,lpg_ppm=0.0f,rs_now=0.0f;

    /*
     * ALARM buzzer state machine variables.
     * In alarm state the buzzer runs in 200ms on / 100ms off
     * pattern every second so the LCD still updates.
     */
    uint32_t alarm_buz_ms = 0;   /* ms into current alarm cycle */

    if(!force_recal&&saved_r0>1000.0f&&saved_r0<2000000.0f){
        g_R0 =saved_r0;
        state=STATE_MONITOR;
        lcd_clear();
        lcd_pos(0,0);lcd_str("  CO MONITOR    ");
        lcd_pos(0,1);lcd_str(" R0:");
        lcd_uint((uint32_t)g_R0,6);
        delay_ms(2000);
        lcd_clear();
    }else{
        g_R0 =MQ9_R0_DEFAULT;
        state=STATE_WARMUP;
        lcd_clear();
        lcd_pos(0,0);lcd_str("  CO MONITOR    ");
        lcd_pos(0,1);force_recal?lcd_str(" RECALIB MODE   ")
                                :lcd_str(" FIRST BOOT...  ");
        beep(force_recal?3:1);
        delay_ms(1500);
        lcd_clear();
    }

    /* ─────────────────────────────────────────────────────
     * MAIN LOOP
     * ───────────────────────────────────────────────────── */
    while(1){

        /* ════════════════════════════════════════════════
         * WARM-UP: countdown, no gas reading
         * ════════════════════════════════════════════════ */
        if(state==STATE_WARMUP){
            uint32_t elapsed=(g_ms-warmup_start)/1000U;
            if((g_ms-last_lcd_ms)>=1000U){
                last_lcd_ms=g_ms;
                uint32_t left=(elapsed<WARMUP_SEC)
                              ?(WARMUP_SEC-elapsed):0U;
                screen_warmup(left);
            }
            if(elapsed>=WARMUP_SEC) state=STATE_CALIB;
        }

        /* ════════════════════════════════════════════════
         * CALIBRATE: one time only in clean air
         * ════════════════════════════════════════════════ */
        else if(state==STATE_CALIB){
            lcd_clear();
            lcd_pos(0,0);lcd_str(" CALIBRATING... ");
            lcd_pos(0,1);lcd_str("  CLEAN AIR!    ");
            do_calibration();
            lcd_clear();
            lcd_pos(0,0);lcd_str("  CALIB DONE!   ");
            lcd_pos(0,1);lcd_str("R0:");
            lcd_uint((uint32_t)g_R0,7);
            beep(3);
            delay_ms(3000);
            lcd_clear();
            state=STATE_MONITOR;
            last_lcd_ms=screen_ms=g_ms;
        }

        /* ════════════════════════════════════════════════
         * MONITOR: real-time CO and LPG every 1 second
         * Display alternates: PPM screen ↔ debug Rs/R0
         * ════════════════════════════════════════════════ */
        else if(state==STATE_MONITOR){

            /* Sensor read + display every 1 second */
            if((g_ms-last_lcd_ms)>=1000U){
                last_lcd_ms=g_ms;
                read_both_ppm(&co_ppm,&lpg_ppm,&rs_now);

                if(!show_debug) screen_normal(co_ppm,lpg_ppm);
                else            screen_debug(rs_now,g_R0);

                /* Check alarm */
                if(co_ppm>=CO_ALARM_PPM||lpg_ppm>=LPG_ALARM_PPM){
                    lcd_clear();
                    show_debug=0;
                    alarm_buz_ms=g_ms;
                    state=STATE_ALARM;
                    uint8_t ca=(co_ppm >=CO_ALARM_PPM)?1U:0U;
                    uint8_t la=(lpg_ppm>=LPG_ALARM_PPM)?1U:0U;
                    screen_alarm(co_ppm,lpg_ppm,ca,la);
                }
            }

            /* Alternate screens every 3 seconds */
            if((g_ms-screen_ms)>=(show_debug?DEBUG_SCREEN_MS:NORMAL_SCREEN_MS)){
                screen_ms=g_ms;
                show_debug=(uint8_t)(!show_debug);
                lcd_clear();
                if(!show_debug) screen_normal(co_ppm,lpg_ppm);
                else            screen_debug(rs_now,g_R0);
            }
        }

        /* ════════════════════════════════════════════════
         * ALARM: danger detected
         *
         * Buzzer pattern: 800ms ON → 200ms OFF → repeat.
         * LCD updates every 1 second with live PPM.
         * Clears when BOTH gases drop below safe level.
         *
         * The buzz_ms() call blocks for 800ms while
         * toggling PA2. During this time LCD does not
         * update — this is intentional and acceptable.
         * The 1-second read cycle restarts after each buzz.
         * ════════════════════════════════════════════════ */
        else if(state==STATE_ALARM){

            /* Buzz 800ms ON */
            buzz_ms(800U);

            /* 200ms OFF — also use this gap to read sensor
             * and update LCD so display stays live          */
            read_both_ppm(&co_ppm,&lpg_ppm,&rs_now);
            uint8_t ca=(co_ppm >=CO_ALARM_PPM)?1U:0U;
            uint8_t la=(lpg_ppm>=LPG_ALARM_PPM)?1U:0U;
            screen_alarm(co_ppm,lpg_ppm,ca,la);

            delay_ms(200U);  /* silent gap */

            /* Check clear condition */
            if(co_ppm<CO_CLEAR_PPM&&lpg_ppm<LPG_CLEAR_PPM){
                GPIOA_BRR=(1U<<2);  /* ensure PA2 LOW */
                lcd_clear();
                show_debug=0;
                screen_ms=last_lcd_ms=g_ms;
                state=STATE_MONITOR;
                screen_normal(co_ppm,lpg_ppm);
            }
        }
    }

    return 0;
}