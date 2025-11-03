#ifndef TIM2_H_
#define TIM2_H_
#define sr_uif   (1U<<0)
#define sr_cc1f   (1U<<1)
void tim2_1hz_init(void);
void tim3_pa5_input_copter(void);
void tim3_pa6_output_compare(void);

#endif