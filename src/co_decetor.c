/*
 * ============================================================
 *  MQ-7 GAS / CO DETECTOR — FIXED PROJECT VERSION
 *  STM32F103C8T6 Blue Pill — Bare Metal, zero HAL
 *
 *  PA0  -> MQ-7 AOUT:
 *          MQ7 AOUT --- 10k --- PA0 --- 10k --- GND
 *
 *  PA2  -> Passive buzzer +
 *  GND  -> Passive buzzer -
 *
 *  PB6  -> LCD SCL
 *  PB7  -> LCD SDA
 *
 *  5V   -> MQ-7 VCC + LCD VCC
 *  GND  -> MQ-7 GND + LCD GND
 *
 *  First boot:
 *  - Keep sensor in clean air
 *  - Device calibrates and saves R0 to Flash
 *
 *  Normal:
 *  - Reads gas every second
 *  - Shows CO PPM
 *  - Alarm when CO >= CO_ALARM_PPM
 *
 *  NOTE:
 *  MQ-7 without proper heating cycle is not professional accurate.
 *  This is good for project/demo gas detection.
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

#define FLASH_SAVE_ADDR  0x0800FC00UL

/*
 * Changed magic value.
 * This forces new calibration and ignores old wrong saved value.
 */
#define CALIB_MAGIC      0xCAFE5001UL

#define MQ7_RL           10000.0f
#define MQ7_VSUPPLY      5.0f
#define MQ7_VREF         3.3f
#define MQ7_ADC_MAX      4095.0f

/*
 * Because you use:
 * MQ7 AOUT --- 10k --- PA0 --- 10k --- GND
 *
 * PA0 receives half voltage.
 */
#define ADC_DIVIDER      2.0f

/*
 * In clean air, MQ-7 Rs/R0 is around 27.5.
 * So:
 * R0 = Rs / 27.5
 */
#define MQ7_CLEAN_AIR_FACTOR 27.5f

/*
 * Approximate MQ-7 CO formula:
 * ppm = CO_A * pow(Rs/R0, CO_B)
 */
#define CO_A             99.042f
#define CO_B             (-1.522f)

#define R0_DEFAULT       1000.0f
#define R0_MIN_VALID     50.0f
#define R0_MAX_VALID     50000.0f

/*
 * For testing, you can use:
 * CO_ALARM_PPM = 50
 * CO_CLEAR_PPM = 30
 *
 * After testing, use:
 * CO_ALARM_PPM = 200
 * CO_CLEAR_PPM = 150
 */
#define CO_ALARM_PPM     200.0f
#define CO_CLEAR_PPM     150.0f

#define BUZZ_HALF        3333U

#define LCD_ADDR         0x27U

#define BL  0x08U
#define EN  0x04U
#define RS  0x01U

/* ── Globals ───────────────────────────────────────────────── */
static volatile uint32_t g_ms = 0;
static float g_R0 = R0_DEFAULT;
static uint8_t g_lcd = LCD_ADDR;

/* ── SysTick ───────────────────────────────────────────────── */
void SysTick_Handler(void)
{
    g_ms++;
}

static void delay_ms(uint32_t ms)
{
    uint32_t s = g_ms;
    while((g_ms - s) < ms);
}

static void delay_us(uint32_t us)
{
    volatile uint32_t n = us * 8U;
    while(n--);
}

/* ── Clock 64MHz HSI ───────────────────────────────────────── */
static void clock_init(void)
{
    FLASH_ACR = (1U << 4) | 2U;

    RCC_CR |= (1U << 0);
    while(!(RCC_CR & (1U << 1)));

    RCC_CFGR = (4U << 8) | (3U << 14) | (0U << 16) | (14U << 18);

    RCC_CR |= (1U << 24);
    while(!(RCC_CR & (1U << 25)));

    RCC_CFGR |= 2U;
    while((RCC_CFGR & (3U << 2)) != (2U << 2));
}

/* ── Buzzer ────────────────────────────────────────────────── */
static void buzz_ms(uint32_t ms)
{
    uint32_t pairs = (ms * 8000UL) / BUZZ_HALF;

    for(uint32_t i = 0; i < pairs; i++)
    {
        GPIOA_BSRR = (1U << 2);
        volatile uint32_t n = BUZZ_HALF;
        while(n--);

        GPIOA_BRR = (1U << 2);
        n = BUZZ_HALF;
        while(n--);
    }

    GPIOA_BRR = (1U << 2);
}

static void beep(uint8_t count)
{
    for(uint8_t i = 0; i < count; i++)
    {
        buzz_ms(200);
        delay_ms(150);
    }
}

/* ── Flash ─────────────────────────────────────────────────── */
static void fl_unlock(void)
{
    if(FLASH_CR & (1U << 7))
    {
        FLASH_KEYR = 0x45670123UL;
        FLASH_KEYR = 0xCDEF89ABUL;
    }
}

static void fl_lock(void)
{
    FLASH_CR |= (1U << 7);
}

static void fl_wait(void)
{
    while(FLASH_SR & 1U);
}

static void fl_erase(uint32_t a)
{
    fl_unlock();
    fl_wait();

    FLASH_CR |= (1U << 1);
    FLASH_AR = a;
    FLASH_CR |= (1U << 6);

    fl_wait();
    FLASH_CR &= ~(1U << 1);
}

static void fl_w16(uint32_t a, uint16_t v)
{
    fl_unlock();
    fl_wait();

    FLASH_CR |= (1U << 0);
    *(volatile uint16_t*)a = v;

    fl_wait();
    FLASH_CR &= ~(1U << 0);
}

static void fl_w32(uint32_t a, uint32_t v)
{
    fl_w16(a, (uint16_t)(v & 0xFFFFU));
    fl_w16(a + 2U, (uint16_t)(v >> 16));
}

static float flash_load_r0(void)
{
    if(*(volatile uint32_t*)FLASH_SAVE_ADDR != CALIB_MAGIC)
    {
        return 0.0f;
    }

    uint32_t b = *(volatile uint32_t*)(FLASH_SAVE_ADDR + 4U);

    float r;
    uint8_t *s = (uint8_t*)&b;
    uint8_t *d = (uint8_t*)&r;

    for(uint8_t i = 0; i < 4; i++)
    {
        d[i] = s[i];
    }

    return r;
}

static void flash_save_r0(float r0)
{
    uint32_t b;
    uint8_t *s = (uint8_t*)&r0;
    uint8_t *d = (uint8_t*)&b;

    for(uint8_t i = 0; i < 4; i++)
    {
        d[i] = s[i];
    }

    fl_erase(FLASH_SAVE_ADDR);
    fl_w32(FLASH_SAVE_ADDR, CALIB_MAGIC);
    fl_w32(FLASH_SAVE_ADDR + 4U, b);
    fl_lock();
}

/* ── Software I2C ──────────────────────────────────────────── */
static void i2c_init(void)
{
    GPIOB_CRL &= ~(0xFFU << 24);
    GPIOB_CRL |=  (0x6U << 24) | (0x6U << 28);

    SCL_HIGH();
    SDA_HIGH();

    delay_ms(10);
}

static void i2c_start(void)
{
    SDA_HIGH();
    delay_us(5);
    SCL_HIGH();
    delay_us(5);
    SDA_LOW();
    delay_us(5);
    SCL_LOW();
    delay_us(5);
}

static void i2c_stop(void)
{
    SDA_LOW();
    delay_us(5);
    SCL_HIGH();
    delay_us(5);
    SDA_HIGH();
    delay_us(5);
}

static uint8_t i2c_byte(uint8_t b)
{
    for(uint8_t i = 0; i < 8; i++)
    {
        if(b & 0x80U)
            SDA_HIGH();
        else
            SDA_LOW();

        delay_us(3);
        SCL_HIGH();
        delay_us(5);
        SCL_LOW();
        delay_us(3);

        b <<= 1;
    }

    SDA_HIGH();
    delay_us(3);
    SCL_HIGH();
    delay_us(5);

    uint8_t a = (uint8_t)(1U - SDA_READ());

    SCL_LOW();
    delay_us(3);

    return a;
}

static void i2c_send(uint8_t addr, uint8_t data)
{
    i2c_start();

    if(i2c_byte((uint8_t)(addr << 1)))
    {
        i2c_byte(data);
    }

    i2c_stop();
    delay_us(100);
}

static uint8_t i2c_probe(uint8_t addr)
{
    i2c_start();
    uint8_t a = i2c_byte((uint8_t)(addr << 1));
    i2c_stop();
    delay_ms(1);

    return a;
}

/* ── LCD ───────────────────────────────────────────────────── */
static void lcd_pulse(uint8_t n)
{
    i2c_send(g_lcd, (uint8_t)(n | EN));
    delay_ms(2);

    i2c_send(g_lcd, (uint8_t)(n & ~EN));
    delay_ms(2);
}

static void lcd_nib(uint8_t v, uint8_t m)
{
    lcd_pulse((uint8_t)((v & 0xF0U) | BL | m));
    lcd_pulse((uint8_t)(((v << 4) & 0xF0U) | BL | m));
}

static void lcd_cmd(uint8_t c)
{
    lcd_nib(c, 0);
}

static void lcd_chr(uint8_t c)
{
    lcd_nib(c, RS);
}

static void lcd_str(const char *s)
{
    while(*s)
    {
        lcd_chr((uint8_t)*s++);
    }
}

static void lcd_clr(void)
{
    lcd_cmd(0x01);
    delay_ms(5);
}

static void lcd_pos(uint8_t c, uint8_t r)
{
    lcd_cmd(0x80U | (uint8_t)(c + (r ? 0x40U : 0U)));
}

static void lcd_init(void)
{
    delay_ms(200);

    i2c_send(g_lcd, (uint8_t)(0x30U | BL | EN));
    delay_ms(5);
    i2c_send(g_lcd, (uint8_t)(0x30U | BL));
    delay_ms(2);

    i2c_send(g_lcd, (uint8_t)(0x30U | BL | EN));
    delay_ms(2);
    i2c_send(g_lcd, (uint8_t)(0x30U | BL));
    delay_ms(2);

    i2c_send(g_lcd, (uint8_t)(0x30U | BL | EN));
    delay_ms(2);
    i2c_send(g_lcd, (uint8_t)(0x30U | BL));
    delay_ms(2);

    i2c_send(g_lcd, (uint8_t)(0x20U | BL | EN));
    delay_ms(2);
    i2c_send(g_lcd, (uint8_t)(0x20U | BL));
    delay_ms(2);

    lcd_cmd(0x28);
    delay_ms(2);

    lcd_cmd(0x08);
    delay_ms(2);

    lcd_cmd(0x01);
    delay_ms(5);

    lcd_cmd(0x06);
    delay_ms(2);

    lcd_cmd(0x0C);
    delay_ms(2);
}

static void lcd_num(uint32_t v, uint8_t w)
{
    char b[12];
    uint8_t i = 0;

    if(!v)
    {
        b[i++] = '0';
    }
    else
    {
        while(v)
        {
            b[i++] = (char)('0' + v % 10U);
            v /= 10U;
        }
    }

    while(i < w)
    {
        b[i++] = ' ';
    }

    for(uint8_t a = 0, e = (uint8_t)(i - 1); a < e; a++, e--)
    {
        char t = b[a];
        b[a] = b[e];
        b[e] = t;
    }

    b[i] = '\0';
    lcd_str(b);
}

/* ── ADC ───────────────────────────────────────────────────── */
static void adc_init(void)
{
    GPIOA_CRL &= ~(0xFU << 0);

    ADC1_SMPR2 = (7U << 0);
    ADC1_SQR1 = 0U;
    ADC1_SQR3 = 0U;

    ADC1_CR2 = (7U << 17) | (1U << 20);

    ADC1_CR2 |= (1U << 0);
    delay_ms(5);

    ADC1_CR2 |= (1U << 3);
    while(ADC1_CR2 & (1U << 3));

    ADC1_CR2 |= (1U << 2);
    while(ADC1_CR2 & (1U << 2));
}

static uint16_t adc_read(void)
{
    ADC1_SR &= ~(1U << 1);

    ADC1_CR2 |= (1U << 22);

    while(!(ADC1_SR & (1U << 1)));

    return (uint16_t)(ADC1_DR & 0xFFFU);
}

static uint16_t adc_avg(void)
{
    uint32_t s = 0;

    for(uint8_t i = 0; i < 32; i++)
    {
        s += adc_read();
        delay_ms(5);
    }

    return (uint16_t)(s / 32U);
}

/* ── MQ-7 Fixed Sensor Math ────────────────────────────────── */
static float adc_to_voltage(uint16_t raw)
{
    float pa0_voltage = ((float)raw / MQ7_ADC_MAX) * MQ7_VREF;

    /*
     * Real sensor voltage from voltage divider.
     */
    float sensor_voltage = pa0_voltage * ADC_DIVIDER;

    if(sensor_voltage < 0.02f)
        sensor_voltage = 0.02f;

    if(sensor_voltage > (MQ7_VSUPPLY - 0.02f))
        sensor_voltage = MQ7_VSUPPLY - 0.02f;

    return sensor_voltage;
}

static float voltage_to_rs(float sensor_voltage)
{
    /*
     * Rs = RL * (Vc - Vout) / Vout
     */
    float rs = MQ7_RL * (MQ7_VSUPPLY - sensor_voltage) / sensor_voltage;

    if(rs < 1.0f)
        rs = 1.0f;

    return rs;
}

static float adc_to_rs(uint16_t raw)
{
    float sensor_voltage = adc_to_voltage(raw);
    return voltage_to_rs(sensor_voltage);
}

static float rs_to_ppm(float rs)
{
    float ratio = rs / g_R0;

    if(ratio < 0.01f)
        ratio = 0.01f;

    if(ratio > 100.0f)
        ratio = 100.0f;

    float ppm = CO_A * powf(ratio, CO_B);

    if(ppm < 0.0f)
        ppm = 0.0f;

    if(ppm > 9999.0f)
        ppm = 9999.0f;

    return ppm;
}

static float read_co(void)
{
    uint16_t raw = adc_avg();
    float rs = adc_to_rs(raw);
    float ppm = rs_to_ppm(rs);

    return ppm;
}

/* ── Calibration Fixed ─────────────────────────────────────── */
static void do_calibration(void)
{
    float rs_sum = 0.0f;
    uint8_t count = 0;

    lcd_clr();
    lcd_pos(0, 0);
    lcd_str(" CALIBRATING... ");

    lcd_pos(0, 1);
    lcd_str(" CLEAN AIR ONLY ");

    /*
     * Warm-up short wait before reading.
     * For better result, power MQ-7 for 5-10 minutes before calibration.
     */
    delay_ms(3000);

    for(uint8_t i = 0; i < 100; i++)
    {
        uint16_t raw = adc_avg();
        float rs = adc_to_rs(raw);

        if(rs > 100.0f && rs < 300000.0f)
        {
            rs_sum += rs;
            count++;
        }

        lcd_pos(0, 1);
        lcd_str("Reading:");
        lcd_num((uint32_t)(i + 1), 3);
        lcd_str("/100 ");

        delay_ms(100);
    }

    if(count >= 50)
    {
        float rs_avg = rs_sum / (float)count;

        /*
         * THIS IS THE MAIN FIX:
         * R0 = Rs / 27.5 in clean air.
         */
        g_R0 = rs_avg / MQ7_CLEAN_AIR_FACTOR;
    }
    else
    {
        g_R0 = R0_DEFAULT;
    }

    if(g_R0 < R0_MIN_VALID || g_R0 > R0_MAX_VALID)
    {
        g_R0 = R0_DEFAULT;
    }

    flash_save_r0(g_R0);
}

/* ── LCD Screens ───────────────────────────────────────────── */
static void screen_normal(float co)
{
    uint32_t v = (uint32_t)(co > 9999.0f ? 9999.0f : co);

    lcd_pos(0, 0);
    lcd_str("CO:  ");
    lcd_num(v, 4);
    lcd_str(" PPM  ");

    lcd_pos(0, 1);
    lcd_str("STATUS: SAFE    ");
}

static void screen_alarm(float co)
{
    uint32_t v = (uint32_t)(co > 9999.0f ? 9999.0f : co);

    lcd_pos(0, 0);
    lcd_str("CO:  ");
    lcd_num(v, 4);
    lcd_str(" PPM  ");

    lcd_pos(0, 1);
    lcd_str("!!! GAS ALARM!!!");
}

/*
 * Optional debug screen.
 * Use this if you want to check raw ADC.
 */
static void screen_debug(uint16_t raw, float co)
{
    lcd_pos(0, 0);
    lcd_str("ADC:");
    lcd_num(raw, 4);
    lcd_str("        ");

    lcd_pos(0, 1);
    lcd_str("CO:");
    lcd_num((uint32_t)co, 4);
    lcd_str(" PPM    ");
}

/* ── MAIN ──────────────────────────────────────────────────── */
int main(void)
{
    clock_init();

    RCC_APB2ENR |= (1U << 0) | (1U << 2) | (1U << 3) | (1U << 9);

    /*
     * Disable JTAG, keep SWD.
     */
    AFIO_MAPR = (AFIO_MAPR & ~(7U << 24)) | (2U << 24);

    /*
     * PA2 buzzer output push-pull.
     */
    GPIOA_CRL &= ~(0xFU << 8);
    GPIOA_CRL |=  (0x3U << 8);
    GPIOA_BRR  =  (1U << 2);

    /*
     * SysTick 1ms at 64MHz.
     */
    STK_LOAD_R = 64000U - 1U;
    STK_VAL = 0U;
    STK_CTRL = 7U;

    delay_ms(300);

    i2c_init();
    adc_init();

    /*
     * Find LCD address.
     */
    while(1)
    {
        if(i2c_probe(0x27U))
        {
            g_lcd = 0x27U;
            break;
        }

        delay_ms(100);

        if(i2c_probe(0x3FU))
        {
            g_lcd = 0x3FU;
            break;
        }

        delay_ms(500);
    }

    lcd_init();

    lcd_clr();
    lcd_pos(0, 0);
    lcd_str("  GAS DETECTOR  ");
    lcd_pos(0, 1);
    lcd_str("   STARTING...  ");

    beep(2);
    delay_ms(1000);

    /*
     * Load saved calibration.
     */
    float saved = flash_load_r0();

    if(saved > R0_MIN_VALID && saved < R0_MAX_VALID)
    {
        g_R0 = saved;

        lcd_clr();
        lcd_pos(0, 0);
        lcd_str("  GAS DETECTOR  ");
        lcd_pos(0, 1);
        lcd_str(" CALIB LOADED   ");

        delay_ms(1500);
        lcd_clr();
    }
    else
    {
        lcd_clr();
        lcd_pos(0, 0);
        lcd_str(" FIRST CALIB... ");
        lcd_pos(0, 1);
        lcd_str(" CLEAN AIR ONLY ");

        delay_ms(2000);

        do_calibration();

        lcd_clr();
        lcd_pos(0, 0);
        lcd_str("  CALIB DONE!   ");
        lcd_pos(0, 1);
        lcd_str("  MONITORING... ");

        beep(3);
        delay_ms(2000);
        lcd_clr();
    }

    float co = 0.0f;
    uint8_t alarmed = 0;

    uint32_t lcd_ms = g_ms;
    uint32_t buz_ms = g_ms;

    screen_normal(co);

    while(1)
    {
        /*
         * Read sensor every second.
         */
        if((g_ms - lcd_ms) >= 1000U)
        {
            lcd_ms = g_ms;

            co = read_co();

            if(!alarmed)
            {
                screen_normal(co);

                if(co >= CO_ALARM_PPM)
                {
                    alarmed = 1;
                    lcd_clr();
                    screen_alarm(co);
                    buz_ms = g_ms;
                }
            }
            else
            {
                screen_alarm(co);

                if(co < CO_CLEAR_PPM)
                {
                    alarmed = 0;
                    GPIOA_BRR = (1U << 2);

                    lcd_clr();
                    lcd_pos(0, 0);
                    lcd_str("  GAS CLEARED   ");
                    lcd_pos(0, 1);
                    lcd_str("  SAFE NOW      ");

                    delay_ms(2000);

                    lcd_clr();
                    screen_normal(co);

                    lcd_ms = g_ms;
                }
            }
        }

        /*
         * Buzzer alarm pattern.
         */
        if(alarmed)
        {
            if((g_ms - buz_ms) >= 1000U)
            {
                buz_ms = g_ms;
                buzz_ms(500U);
            }
        }
    }

    return 0;
}