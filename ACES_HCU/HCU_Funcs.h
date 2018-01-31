/** @file HCU_Funcs.h
 *  @brief Function prototypes for the HCU Master Microcontroller.
 *
 *  This contains the function prototypes for the 
 *  Master Microcontroller as well as any macros,
 *  constants or global variables what will be
 *  needed.
 *
 *  @author Nick Moore
 *  @bug No known bugs.
 */ 
#include<avr/sfr_defs.h>

#ifndef HCU_FUNCS_H_
#define HCU_FUNCS_H_
#define ECU_present 0    //! 0 means the dummy ECU is present, 1 means the real ECU is present

//////////////////////////////////////////////////////////////////////////
                          //Functions//
//////////////////////////////////////////////////////////////////////////

void Initial(void);
void tempConversion(void);
void tempHeaterHelper(void);
void flowMeter(void);
void ECU_toggle(uint8_t ECU_mode);
void assign_bit(volatile uint8_t *sfr,uint8_t bit, uint8_t val);
void change_timers(void);


//////////////////////////////////////////////////////////////////////////
						//Global Variables//
//////////////////////////////////////////////////////////////////////////
char opMode;                    //! This is the operational mode the system is in.  0 for heating, 1 for pumping
float duty_cycle;               //! This is the value of the duty cycle in 0.XXXX
unsigned char cur_ADC;          //! This is the variable which will keep track of which channel the ADC is currently on
float saveTemps[7];             //! This is an array of the seven temperatures we are keeping track of
unsigned char desired_temp;     //! This is a byte which will flip bits 0-7 to denote when each component has reached its desired temp
uint8_t desired_pulses;         //! This is the number of pulse which should be observed during the 8 bit timing window
uint8_t pulse_count;            //! This is the variable which will keep track of how many pulses were actually received

/**
saveTemps[0]:   Heater Battery
saveTemps[1]:   Engine Battery
saveTemps[2]:   Hopper
saveTemps[3]:   ECU
saveTemps[4]:   Fuel Line 1
saveTemps[5]:   Fuel Line 2
saveTemps[6]:   ESB
*/


#endif /* HCU_FUNCS_H_ */