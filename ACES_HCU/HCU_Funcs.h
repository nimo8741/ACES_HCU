/** @file HCU_Funcs.h
 *  @author Nick Moore
 *  @date February 14, 2018
 *  @brief Function prototypes, macros, constants, and global variables for the HCU Microcontroller.
 *
 *  @bug No known bugs.
 */ 
#include <avr/sfr_defs.h>
#include <float.h>

#ifndef HCU_FUNCS_H_
#define HCU_FUNCS_H_

//Function #defines that don't need to have an entire function call
//! Simple function define for testing if the bit is set
#define bit_is_set(sfr,bit) \
		(_SFR_BYTE(sfr) & _BV(bit))              

//! Simple function define for testing if the bit is clear
#define bit_is_clear(sfr,bit) \
		(!(_SFR_BYTE(sfr) & _BV(bit)))           
				

///////////////////////////////////////////////////////////////////////////
//////////////////////// Project Constants ////////////////////////////////
///////////////////////////////////////////////////////////////////////////
//! Desired mass flow rate of fuel in g/sec
#define fuelFlow 4.8 

//! Acceptable error in terms of g/sec         
#define fuelError 0.13        

//! Desired temperature of the lipo batteries in degF          (ADC0)
#define TempBat 10         

//! Desired temperature of the hopper in degF                  (ADC2)
#define TempHopper 10     

//! Desired temperature of the ECU in degF                     (ADC3) 
#define TempECU 10   

//! Desired temperature of the fuel line to the pump in degF   (ADC4)      
#define TempFLine1 10      

//! Desired temperature of the fuel line to the engine in degF (ADC5)
#define TempFLine2 10    

//! Desired temperature of the ESB in degF                     (ADC6)   
#define TempESB 10         

//! 0 means the dummy ECU is present, 1 means the real ECU is present
#define ECU_present 0    


///////////////////////////////////////////////////////////////////////////
///////////////////////// Pin Assignments /////////////////////////////////
///////////////////////////////////////////////////////////////////////////

///////////////// PORT A Assignments  ////////////////////

//! Pin location for I/O port that will turn on the ECU
#define ECUon_Pin 7        

///////////////// PORT B Assignments  ///////////////////

//! Pin location for the warm up LED
#define Warm_LED 1   

//! Pin location for the ECU heater circuit output   
#define ECU_pin 3       

///////////////// PORT C Assignments  ///////////////////
// This is reserved for the I2C communication and JTAG

///////////////// PORT D Assignments  ///////////////////

//! Pin location for the Lipo Battery heating circuit
#define BatPin 0     

//! Pin location for the Hopper heating circuit   
#define HopperPin 1     

//! Pin location for the heating circuit for the first section of the fuel line
#define FLine1Pin 2     

//! Pin location for the ESB heating circuit
#define ESB_Pin 3       

//! Pin location for the Alive LED
#define Alive_LED 5   

//! Pin location for the fuel rate LED  
#define Fuel_LED 6     

//! Pin location for the heating circuit for the second section of the fuel line
#define Fline2Pin 7    

///////////////////////////////////////////////////////////////////////////
///////////////////////// Project Constants ///////////////////////////////
///////////////////////////////////////////////////////////////////////////

//! Assumed K factor for the flow meter in pulses per liter
#define K_factor 110000      

//! Density of the kerosene in g/ml         
#define density 0.81  

//! Maximum amount of time for an 8 bit timer with a prescalar of 1024                   
#define max_time 0.262144      

//! Slope for the linear relationship between voltage and mass flow (put in mf, get volts)      
#define pump_m 0.382587  

//! Y-intercept for the linear relationship between voltage and mass flow                
#define pump_b 0.195783     

//! Total voltage which will be sent to the pump             
#define pump_tot_V 6.0     

//! Volts required to get a single change in pulse count              
#define V_per_pulse 0.0107393            

//! Duty cycle for the ECU heater (0.5 = 50%)
#define ECU_duty 0.5       

//! Duty cycle for the second fuel line heater             
#define F_line_duty 0.20946              

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
//! Operational mode the system is in.  0 for heating, 1 for pumping, 2 for pumping has finished
char opMode;           

//! Value of the duty cycle in 0.XXXX
float duty_cycle;    

//! Array of the six temperatures we are keeping track of       
float saveTemps[6]; 

//! Byte which will flip bits 0-7 to denote when each component has reached its desired temp            
unsigned char desired_temp;   

//! Number of pulse which should be observed during the 8 bit timing window  
uint8_t desired_pulses;      

//! Most amount of error in the difference in the pulse amounts to be considered a success   
uint8_t pulse_error_allow;  

//! Variable which will keep track of how many pulses were actually received    
uint8_t pulse_count;    

//! Variable to delay the alive_led so that it blinks twice as slow as the warming LED        
uint8_t alive_counter; 



/*   Descriptions of which physical components will be on which channels
saveTemps[0]:   Battery
saveTemps[1]:   Hopper
saveTemps[2]:   ECU
saveTemps[3]:   Fuel Line 1
saveTemps[4]:   Fuel Line 2
saveTemps[5]:   ESB
*/


#endif /* HCU_FUNCS_H_ */