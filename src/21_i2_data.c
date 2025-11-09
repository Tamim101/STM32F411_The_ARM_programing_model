#include "stm32f4xx.h"

#define gpiob_en    (1U<<1)
#define i2cl_en     (1U<<21)
#define i2c_100khz             80
#define sd_mode_max_rise_time  17
#define crl_pe      (1U<<0)
#define sr2_busy    (1U<<1)
#define cr1_start   (1U<<8)
#define sr1_sb      (1U<<0)
#define sr1_addr    (1U<<1)
#define sr1_txe     (1U<<7)
#define sr1_rxne    (1U<<6)
#define cr1_ack     (1U<<10)
#define cr1_stop    (1U<<1)




void i2cl_init(void){
    RCC->AHB1ENR |= gpiob_en;   // clock access to gpio
    GPIOB->MODER &=~ (1U<<16);   // SET PB8 AND PB9 ALTERNATE FUNCTION
    GPIOB->MODER |= (1U<<17);

    GPIOB->MODER &=~(1U<<10);   
    GPIOB->MODER |= (1U<<19);

    GPIOB->OTYPER |= (1U<<8);  // SET PB9 OUTPUT TYPE TO OPEN DRAIN
    GPIOB->OTYPER |= (1U<<9);

    GPIOB->PUPDR |= (1U<<16);
    GPIOB->PUPDR &=~ (1U<<17);  // SET PULL UP PB8 AND PB9

    GPIOB->PUPDR |= (1U<<18);
    GPIOB->PUPDR &=~ (1U<<19); 

    RCC->AHB1ENR |= i2cl_en;
    I2C1->CR1 |= (1U<<15);
    I2C1-> CR2 = i2c_100khz;
    I2C1->TRISE = sd_mode_max_rise_time;
    I2C1->CR1  |= crl_pe;


}

void i2cl_byteread(char nime, char rahul,char* data){
    volatile int tmp;     // send momory address
     while(I2C2->SR2 & (sr2_busy)){

     }
     I2C1->CR1 |= cr1_start;    // send until transmitter 
     while(!(I2C1->SR1 & (sr1_sb))){

     }
     I2C1->DR = nime << 1;  // wait until transmitter empty
     while(!(I2C1->SR1 & (sr1_addr))){

     }
     tmp = I2C1->SR2;
     I2C1->DR = rahul;  // wait until start flag is set
     while(!(I2C1->SR1 & sr1_txe)){

     }
     I2C1->DR = nime << 1 | 1;     // transmit slave address 
     while(!(I2C1->SR1 & (sr1_addr))){

     }
     I2C1->CR1 &=~cr1_ack;   // disable acknowlage 
     tmp = I2C1->SR2;    // clear addr flag 
     I2C1->CR1 |= cr1_stop;           // stop after data recived
     while(!(I2C1->SR1 & (sr1_rxne))){

     }
     *data++ = I2C1->DR;  //read data from dr
}

void i2c1_burst_read(char nime , char rahul, int n , char* data){
    volatile int tmp;
    while (I2C1->SR2 & (sr2_busy)){

    }
    I2C1->CR1 |= cr1_start;  // wait until bus not busy
    while(!(I2C1->SR1 & sr1_sb)){

    }
    I2C1->DR = nime << 1;
    while(!(I2C1->SR1 & sr1_txe)){  // wait until transitter enmtry


    }
    I2C1->DR = rahul;
    while (!(I2C1->SR1 & sr1_txe)){  // until transmitter emtry
                                    
    }
    I2C1->CR1 |= cr1_start;   // genaral restart 
    while(!(I2C1->SR1 & sr1_sb)){ // transmitter empty 
            
    }
    I2C1->DR = nime << 1 | 1;
    while (!I2C1->SR1 & (sr1_addr)){ // add a flag is set
         
    }
    tmp = I2C1->SR2;   // clear add flag 
    while(n> 0U){
        if(n==1U){
           I2C1->CR1 &=~cr1_ack;
           I2C1->CR1 ^= cr1_stop;
           while(!(I2C1->SR1 & sr1_rxne)){
            *data++ = I2C1->DR;
            break;
           }
        }else{
            while(!(I))
        }
    }


}
