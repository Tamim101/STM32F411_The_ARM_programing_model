/*
 * STM32F103C8T6 Blue Pill + MPU6050
 * Fixed command parsing - handles \r \n \r\n from any terminal
 *
 * WIRING:
 *   PA9  TX  --> USB-UART RX
 *   PA10 RX  --> USB-UART TX
 *   GND      --> USB-UART GND
 *   PC13     --> on-board LED
 *   PB6 SCL  --> MPU6050 SCL
 *   PB7 SDA  --> MPU6050 SDA
 *   3.3V     --> MPU6050 VCC
 *   GND      --> MPU6050 GND + AD0
 *
 * SERIAL: 115200 baud
 */

#include <stdint.h>
#include <math.h>

/* ---- registers ---- */
#define RCC_BASE     0x40021000UL
#define GPIOA_BASE   0x40010800UL
#define GPIOB_BASE   0x40010C00UL
#define GPIOC_BASE   0x40011000UL
#define USART1_BASE  0x40013800UL
#define I2C1_BASE    0x40005400UL
#define SYSTICK_BASE 0xE000E010UL

#define RCC_CFGR    (*(volatile uint32_t*)(RCC_BASE+0x04))
#define RCC_APB2ENR (*(volatile uint32_t*)(RCC_BASE+0x18))
#define RCC_APB1ENR (*(volatile uint32_t*)(RCC_BASE+0x1C))
#define GPIOA_CRH   (*(volatile uint32_t*)(GPIOA_BASE+0x04))
#define GPIOB_CRL   (*(volatile uint32_t*)(GPIOB_BASE+0x00))
#define GPIOB_ODR   (*(volatile uint32_t*)(GPIOB_BASE+0x0C))
#define GPIOC_CRH   (*(volatile uint32_t*)(GPIOC_BASE+0x04))
#define GPIOC_ODR   (*(volatile uint32_t*)(GPIOC_BASE+0x0C))
#define USART1_SR   (*(volatile uint32_t*)(USART1_BASE+0x00))
#define USART1_DR   (*(volatile uint32_t*)(USART1_BASE+0x04))
#define USART1_BRR  (*(volatile uint32_t*)(USART1_BASE+0x08))
#define USART1_CR1  (*(volatile uint32_t*)(USART1_BASE+0x0C))
#define I2C1_CR1    (*(volatile uint32_t*)(I2C1_BASE+0x00))
#define I2C1_CR2    (*(volatile uint32_t*)(I2C1_BASE+0x04))
#define I2C1_OAR1   (*(volatile uint32_t*)(I2C1_BASE+0x08))
#define I2C1_DR     (*(volatile uint32_t*)(I2C1_BASE+0x10))
#define I2C1_SR1    (*(volatile uint32_t*)(I2C1_BASE+0x14))
#define I2C1_SR2    (*(volatile uint32_t*)(I2C1_BASE+0x18))
#define I2C1_CCR    (*(volatile uint32_t*)(I2C1_BASE+0x1C))
#define I2C1_TRISE  (*(volatile uint32_t*)(I2C1_BASE+0x20))
#define STK_CTRL (*(volatile uint32_t*)(SYSTICK_BASE+0x00))
#define STK_LOAD (*(volatile uint32_t*)(SYSTICK_BASE+0x04))
#define STK_VAL  (*(volatile uint32_t*)(SYSTICK_BASE+0x08))

/* ---- globals ---- */
static volatile uint32_t ms = 0;
static volatile uint8_t  streaming = 0;

/* RX ring buffer */
#define RXB 128
static volatile char    rbuf[RXB];
static volatile uint8_t rh = 0, rt = 0;

static float gxo=0, gyo=0, gzo=0;

/* ---- ISRs ---- */
void SysTick_Handler(void){ ms++; }

void USART1_IRQHandler(void){
    if(USART1_SR & (1u<<5)){
        char c = (char)(USART1_DR & 0xFF);
        uint8_t n = (rh+1) % RXB;
        if(n != rt){ rbuf[rh]=c; rh=n; }
    }
}

/* ---- delay ---- */
static void dms(uint32_t d){ uint32_t s=ms; while((ms-s)<d); }

/* ---- clock HSI 8MHz ---- */
static void clock_init(void){
    RCC_CFGR &= ~3UL;
    while((RCC_CFGR & (3UL<<2)) != 0);
    STK_LOAD = 7999; STK_VAL = 0;
    STK_CTRL = (1u<<2)|(1u<<1)|(1u<<0);
}

/* ---- GPIO ---- */
static void gpio_init(void){
    RCC_APB2ENR |= (1u<<2)|(1u<<3)|(1u<<4)|(1u<<0)|(1u<<14);
    RCC_APB1ENR |= (1u<<21);
    /* PC13 LED output 2MHz push-pull */
    GPIOC_CRH &= ~(0xFUL<<20); GPIOC_CRH |= (0x2UL<<20);
    GPIOC_ODR |= (1u<<13);
    /* PA9 TX alt push-pull 50MHz */
    GPIOA_CRH &= ~(0xFUL<<4);  GPIOA_CRH |= (0xBUL<<4);
    /* PA10 RX input floating */
    GPIOA_CRH &= ~(0xFUL<<8);  GPIOA_CRH |= (0x4UL<<8);
    /* PB6 PB7 AF open-drain 50MHz for I2C */
    GPIOB_CRL &= ~(0xFFUL<<24); GPIOB_CRL |= (0xFFUL<<24);
    /* Internal pullups on PB6 PB7 via ODR
       Note: with AF open-drain mode the ODR pullup trick
       does not apply -- we rely on module's built-in pullups */
}

/* ---- UART ---- */
static void uart_init(void){
    USART1_BRR = 69;  /* 8MHz/115200 */
    USART1_CR1 = (1u<<13)|(1u<<3)|(1u<<2)|(1u<<5);
    *((volatile uint32_t*)0xE000E104) |= (1u<<5); /* NVIC IRQ37 */
}

static void pc(char c){ while(!(USART1_SR&(1u<<7))); USART1_DR=(uint32_t)c; }
static void ps(const char *s){ while(*s) pc(*s++); }

static void pi(int32_t n){
    if(n<0){pc('-');n=-n;}
    if(n==0){pc('0');return;}
    char t[12]; int i=0;
    while(n>0){t[i++]='0'+(n%10);n/=10;}
    while(i--) pc(t[i]);
}

static void pf(float f, int d){
    if(f<0.0f){pc('-');f=-f;}
    pi((int32_t)f); pc('.');
    float r=f-(int32_t)f;
    for(int i=0;i<d;i++){r*=10.0f;pc('0'+(int)r);r-=(int)r;}
}

static void ph(uint8_t v){
    pc("0123456789ABCDEF"[v>>4]);
    pc("0123456789ABCDEF"[v&0xF]);
}

/* non-blocking read from ring buffer */
static int gc(char *c){
    if(rh==rt) return 0;
    *c=rbuf[rt]; rt=(rt+1)%RXB; return 1;
}

/* ---- I2C ---- */
static void i2c_reset(void){
    I2C1_CR1=(1u<<15); dms(10); I2C1_CR1=0; dms(5);
    I2C1_CR2=8; I2C1_CCR=80; I2C1_TRISE=9;
    I2C1_OAR1=0x4000;
    I2C1_CR1=(1u<<0)|(1u<<10); /* PE | ACK */
    dms(5);
}

static void i2c_init(void){ i2c_reset(); }

static int iw(uint32_t bit){
    uint32_t t=ms;
    while(!(I2C1_SR1 & bit)){
        uint32_t s=I2C1_SR1;
        if(s&(1u<<10)){ I2C1_SR1&=~(1u<<10); I2C1_CR1|=(1u<<9); return -1; }
        if(s&(1u<<8)) { I2C1_SR1&=~(1u<<8);  i2c_reset();        return -2; }
        if((ms-t)>15) { I2C1_CR1|=(1u<<9);                        return -4; }
    }
    return 1;
}

static int i2c_start(uint8_t ar){
    uint32_t t=ms;
    while(I2C1_SR2&(1u<<1)){ if((ms-t)>20){i2c_reset();return 0;} }
    I2C1_CR1|=(1u<<8);
    if(iw(1u<<0)<=0) return 0;
    I2C1_DR=ar;
    if(iw(1u<<1)<=0) return 0;
    (void)I2C1_SR1; (void)I2C1_SR2;
    return 1;
}

static int i2c_wb(uint8_t d){
    if(iw(1u<<7)<=0) return 0;
    I2C1_DR=d;
    if(iw(1u<<2)<=0) return 0;
    return 1;
}

static void i2c_stop(void){
    I2C1_CR1|=(1u<<9);
    uint32_t t=ms; while((I2C1_SR2&(1u<<1))&&((ms-t)<5));
}

static void mw(uint8_t r, uint8_t v){
    i2c_start(0x68<<1); i2c_wb(r); i2c_wb(v); i2c_stop(); dms(2);
}

static void mr(uint8_t r, uint8_t *b, uint8_t l){
    i2c_start(0x68<<1); i2c_wb(r);
    I2C1_CR1|=(1u<<8); iw(1u<<0);
    if(l==1) I2C1_CR1&=~(1u<<10); else I2C1_CR1|=(1u<<10);
    I2C1_DR=(0x68<<1)|1; iw(1u<<1);
    (void)I2C1_SR1; (void)I2C1_SR2;
    for(uint8_t i=0;i<l;i++){
        if(i==l-1){ I2C1_CR1&=~(1u<<10); I2C1_CR1|=(1u<<9); }
        iw(1u<<6); b[i]=(uint8_t)I2C1_DR;
    }
}

/* ---- I2C scan ---- */
static void scan(void){
    ps("[SCAN] Scanning 0x01-0x77...\r\n");
    int found=0;
    for(uint8_t a=1;a<0x78;a++){
        uint32_t t=ms;
        while(I2C1_SR2&(1u<<1)){ if((ms-t)>20){i2c_reset();break;} }
        I2C1_CR1|=(1u<<8);
        t=ms; while(!(I2C1_SR1&(1u<<0))){ if((ms-t)>5) break; }
        I2C1_DR=(uint8_t)(a<<1);
        t=ms;
        while(1){
            uint32_t s=I2C1_SR1;
            if(s&(1u<<1)){
                (void)I2C1_SR1;(void)I2C1_SR2;
                I2C1_CR1|=(1u<<9); dms(2);
                ps("  Found: 0x"); ph(a); ps("\r\n");
                found++; break;
            }
            if(s&(1u<<10)){ I2C1_SR1&=~(1u<<10); I2C1_CR1|=(1u<<9); dms(1); break; }
            if((ms-t)>5){ I2C1_CR1|=(1u<<9); i2c_reset(); dms(2); break; }
        }
    }
    if(!found) ps("  No devices found.\r\n");
    else { pi(found); ps(" device(s) found\r\n"); }
}

/* ---- IMU print ---- */
static void imu_pretty(void){
    uint8_t a[6],g[6],t[2];
    mr(0x3B,a,6); mr(0x41,t,2); mr(0x43,g,6);
    float ax=(int16_t)((a[0]<<8)|a[1])/16384.0f;
    float ay=(int16_t)((a[2]<<8)|a[3])/16384.0f;
    float az=(int16_t)((a[4]<<8)|a[5])/16384.0f;
    float gx=(int16_t)((g[0]<<8)|g[1])/131.0f-gxo;
    float gy=(int16_t)((g[2]<<8)|g[3])/131.0f-gyo;
    float gz=(int16_t)((g[4]<<8)|g[5])/131.0f-gzo;
    int16_t rt=(int16_t)((t[0]<<8)|t[1]);
    float tc=rt/340.0f+36.53f;
    float roll=atan2f(ay,az)*57.2957795f;
    float pitch=atan2f(-ax,sqrtf(ay*ay+az*az))*57.2957795f;
    ps("------------------------------------------\r\n");
    ps(" ax="); pf(ax,4); ps("g  ay="); pf(ay,4); ps("g  az="); pf(az,4); ps("g\r\n");
    ps(" gx="); pf(gx,2); ps("dps  gy="); pf(gy,2); ps("dps  gz="); pf(gz,2); ps("dps\r\n");
    ps(" Roll="); pf(roll,2); ps("  Pitch="); pf(pitch,2); ps("\r\n");
    ps(" Temp="); pf(tc,2); ps("C\r\n");
    ps("------------------------------------------\r\n");
}

static void imu_csv(uint32_t ts){
    uint8_t a[6],g[6],t[2];
    mr(0x3B,a,6); mr(0x41,t,2); mr(0x43,g,6);
    float ax=(int16_t)((a[0]<<8)|a[1])/16384.0f;
    float ay=(int16_t)((a[2]<<8)|a[3])/16384.0f;
    float az=(int16_t)((a[4]<<8)|a[5])/16384.0f;
    float gx=(int16_t)((g[0]<<8)|g[1])/131.0f-gxo;
    float gy=(int16_t)((g[2]<<8)|g[3])/131.0f-gyo;
    float gz=(int16_t)((g[4]<<8)|g[5])/131.0f-gzo;
    int16_t rt=(int16_t)((t[0]<<8)|t[1]);
    float tc=rt/340.0f+36.53f;
    float roll=atan2f(ay,az)*57.2957795f;
    float pitch=atan2f(-ax,sqrtf(ay*ay+az*az))*57.2957795f;
    pi(ts);pc(',');pf(ax,3);pc(',');pf(ay,3);pc(',');pf(az,3);pc(',');
    pf(gx,2);pc(',');pf(gy,2);pc(',');pf(gz,2);pc(',');
    pf(roll,2);pc(',');pf(pitch,2);pc(',');pf(tc,2);ps("\r\n");
}

/* ================================================================
   COMMAND PROCESSOR
   Key fix: accept BOTH \r and \n as end of command.
   Ignore the second byte of \r\n pairs (skip empty commands).
   ================================================================ */
static void run_cmd(char *s){
    /* trim */
    int l=0; while(s[l]) l++;
    while(l>0 && (s[l-1]=='\r'||s[l-1]=='\n'||s[l-1]==' ')) s[--l]='\0';
    /* lowercase */
    for(int i=0;i<l;i++) if(s[i]>='A'&&s[i]<='Z') s[i]+=32;

    /* echo what we received so user can see it was processed */
    ps("\r\n[CMD] '");
    ps(s);
    ps("'\r\n");

    if(l==0){ ps("> "); return; }

    if     (s[0]=='i'&&s[1]=='m'&&s[2]=='u'&&s[3]=='\0') imu_pretty();
    else if(s[0]=='s'&&s[1]=='t'&&s[2]=='r'){
        streaming=1;
        ps("time_ms,ax,ay,az,gx,gy,gz,roll,pitch,temp\r\n");
    }
    else if(s[0]=='s'&&s[1]=='t'&&s[2]=='o'){
        streaming=0; ps("stopped\r\n");
    }
    else if(s[0]=='s'&&s[1]=='c'&&s[2]=='a') scan();
    else if(s[0]=='h'){
        ps("Commands:\r\n");
        ps("  imu    - read sensor once\r\n");
        ps("  stream - 10Hz CSV\r\n");
        ps("  stop   - stop stream\r\n");
        ps("  scan   - I2C bus scan\r\n");
        ps("  help   - this list\r\n");
    }
    else{ ps("Unknown: '"); ps(s); ps("'\r\n"); }

    ps("> ");
}

/* ================================================================
   MAIN
   ================================================================ */
int main(void)
{
    clock_init();
    gpio_init();
    uart_init();
    i2c_init();
    dms(200);

    ps("\r\n=== STM32F103 + MPU6050 ===\r\n");
    ps("HSI 8MHz | UART 115200 | I2C 50kHz\r\n\r\n");

    /* LED blink proof */
    ps("[BOOT] LED blink...\r\n");
    for(int i=0;i<6;i++){ GPIOC_ODR^=(1u<<13); dms(150); }
    GPIOC_ODR|=(1u<<13);
    ps("[BOOT] done\r\n\r\n");

    /* Scan I2C */
    scan();

    /* Wake MPU6050 */
    ps("\r\n[INIT] Wake MPU6050...\r\n");
    mw(0x6B,0x00); dms(100);

    uint8_t who=0; mr(0x75,&who,1);
    ps("[INIT] WHO_AM_I=0x"); ph(who); ps("\r\n");

    if(who==0x68||who==0x72){
        ps("[OK]  MPU6050 found!\r\n");
        mw(0x6B,0x01); mw(0x19,0x09); mw(0x1A,0x03);
        mw(0x1B,0x00); mw(0x1C,0x00); dms(100);
        ps("[CAL] Keep STILL 1 second...\r\n");
        for(int i=0;i<200;i++){
            uint8_t b[6]; mr(0x43,b,6);
            gxo+=(int16_t)((b[0]<<8)|b[1])/131.0f;
            gyo+=(int16_t)((b[2]<<8)|b[3])/131.0f;
            gzo+=(int16_t)((b[4]<<8)|b[5])/131.0f;
            dms(5);
        }
        gxo/=200.0f; gyo/=200.0f; gzo/=200.0f;
        ps("[OK]  Ready!\r\n");
    } else {
        ps("[WARN] MPU6050 not found (WHO_AM_I=0x"); ph(who); ps(")\r\n");
        ps("[WARN] Commands still work -- use 'scan' to diagnose\r\n");
    }

    ps("\r\nType 'help' then press Enter\r\n");
    ps("> ");

    /* ---- command loop ---- */
    char    cb[32];
    uint8_t cl = 0;
    uint8_t last_was_cr = 0;   /* track \r so we skip the \n in \r\n */
    uint32_t ls=0, ll=0;

    while(1){
        uint32_t now=ms;

        /* LED heartbeat */
        if(!streaming && (now-ll)>=1000){ ll=now; GPIOC_ODR^=(1u<<13); }

        /* streaming */
        if(streaming && (now-ls)>=100){
            ls=now; GPIOC_ODR^=(1u<<13);
            imu_csv(now);
        }

        /* read chars from ring buffer */
        char c;
        while(gc(&c)){

            if(c=='\n' && last_was_cr){
                /* this \n is the second byte of \r\n -- skip it */
                last_was_cr=0;
                continue;
            }
            last_was_cr = (c=='\r');

            if(c=='\r' || c=='\n'){
                /* end of command */
                cb[cl]='\0';
                run_cmd(cb);
                cl=0;
            }
            else if(c=='\b' || c==127){
                /* backspace */
                if(cl>0){
                    cl--;
                    ps("\b \b");
                }
            }
            else if(c>=' ' && c<127){
                /* printable character -- add to buffer and echo */
                if(cl<31){
                    cb[cl++]=c;
                    pc(c);
                }
            }
        }
    }
}