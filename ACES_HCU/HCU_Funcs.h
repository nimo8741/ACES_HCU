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

//Function #defines that don't need to have an entire function call
#define bit_is_set(sfr,bit) \
		(_SFR_BYTE(sfr) & _BV(bit))              //! Simple function define for testing if the bit is set

#define bit_is_clear(sfr,bit) \
		(!(_SFR_BYTE(sfr) & _BV(bit)))           //! Simple function define for testing if the bit is clear
				

///////////////////////////////////////////////////////////////////////////
//////////////////////// Project Constants ////////////////////////////////
///////////////////////////////////////////////////////////////////////////
#define fuelFlow 4.8          //! This is the desired mass flow rate of fuel in g/sec
#define fuelError 0.13        //! This is the acceptable error in terms of g/sec

#define TempBat 65         //! This is the desired temperature of the lipo batteries in degF          (ADC0)
#define TempHopper 88      //! This is the desired temperature of the hopper in degF                  (ADC2)
#define TempECU 73         //! This is the desired temperature of the ECU in degF                     (ADC3)
#define TempFLine1 85      //! This is the desired temperature of the fuel line to the pump in degF   (ADC4)
#define TempFLine2 88      //! This is the desired temperature of the fuel line to the engine in degF (ADC5)
#define TempESB 70         //! This is the desired temperature of the ESB in degF                     (ADC6) 


#define ECU_present 0    //! 0 means the dummy ECU is present, 1 means the real ECU is present


///////////////////////////////////////////////////////////////////////////
///////////////////////// Pin Assignments /////////////////////////////////
///////////////////////////////////////////////////////////////////////////

// PORT A Assignments
#define ECUon_Pin 7        //! Pin location for I/O port that will turn on the ECU

// PORT B Assignments
#define Warm_LED 1      //! Pin location for the warm up LED
#define ECU_pin 3       //! Pin location for the ECU heater circuit output

// PORT C Assignments

// PORT D Assignments
#define BatPin 0        //! Pin location for the Lipo Battery heating circuit
#define HopperPin 1     //! Pin location for the Hopper heating circuit
#define FLine1Pin 2     //! Pin location for the heating circuit for the first section of the fuel line
#define ESB_Pin 3       //! Pin location for the ESB heating circuit
#define Alive_LED 5     //! Pin location for the Alive LED
#define Fuel_LED 6      //! Pin location for the fuel rate LED
#define Fline2Pin 7     //! Pin location for the heating circuit for the second section of the fuel line

#define K_factor 110000                  //! This is the assumed K factor for the flow meter in pulses per liter
#define density 0.81                     //! This is the density of the kerosene in g/ml
#define max_time 0.262144                //! This is the maximum amount of time for an 8 bit timer with a prescalar of 1024
#define pump_m 0.382587                  //! This is the slope for the linear relationship between voltage and mass flow (put in mf, get volts)
#define pump_b 0.195783                  //! This is the y-intercept for the linear relationship between voltage and mass flow
#define pump_tot_V 6                     //! This is the total voltage which will be sent to the pump
#define V_per_pulse 0.0107393            //! This is the amount volts required to get a single change in pulse count

#define ECU_duty 0.5                     //! This represents a 50% duty cycle for the ECU heater
#define F_line_duty 0.20946              //! This represents a 20.946% duty cycle for the second fuel line heater

//////////////////////////////////////////////////////////////////////////
//////////////////////////////  Functions  ///////////////////////////////
//////////////////////////////////////////////////////////////////////////

void Initial(void);
void tempConversion(void);
void tempHeaterHelper(void);
void flowMeter(void);
void ECU_toggle(uint8_t ECU_mode);
void assign_bit(volatile uint8_t *sfr,uint8_t bit, uint8_t val);
void change_timers(void);


//////////////////////////////////////////////////////////////////////////
////////////////////////// Global Variables  /////////////////////////////
//////////////////////////////////////////////////////////////////////////
char opMode;                    //! This is the operational mode the system is in.  0 for heating, 1 for pumping, 2 for pumping has finished
float duty_cycle;               //! This is the value of the duty cycle in 0.XXXX
unsigned char cur_ADC;          //! This is the variable which will keep track of which channel the ADC is currently on
float saveTemps[6];             //! This is an array of the seven temperatures we are keeping track of
unsigned char desired_temp;     //! This is a byte which will flip bits 0-7 to denote when each component has reached its desired temp
uint8_t desired_pulses;         //! This is the number of pulse which should be observed during the 8 bit timing window
uint8_t pulse_error_allow;      //! This is the most amount of error in the difference in the pulse amounts to be considered a success
uint8_t pulse_count;            //! This is the variable which will keep track of how many pulses were actually received
uint8_t alive_counter;          //! This will help delay the alive_led so that it blinks twice as slow as the warming LED


/**
saveTemps[0]:   Battery
saveTemps[1]:   Hopper
saveTemps[2]:   ECU
saveTemps[3]:   Fuel Line 1
saveTemps[4]:   Fuel Line 2
saveTemps[5]:   ESB
*/


#endif /* HCU_FUNCS_H_ */