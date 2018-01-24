/** @file HCU_Funcs.c
 *  @brief Function implementations for the Master Microcontroller.
 *
 *  This contains the implementations for the functions described
 *  in HCU_Funcs.h
 *
 *  @author Nick Moore
 *  @author Alex Bertman
 *  @author Alex Johnson
 *  @author Matt Robak
 *  @author Nick Taylor
 *
 *  @bug No known bugs.
 */

#define F_CPU 8000000UL   // This is so that the delay functions work.  Sets clock to 8MHz
#include "HCU_Funcs.h"
#include <avr/io.h>
#include <util/delay.h>   // This library is so that easy delay functions can be implemented
#include <avr/interrupt.h>

#define dMassFlow 4.8    //! This is the desired mass flow rate of fuel in g/sec

#define TempHBat 65     //! This is the desired temperature of the Heater battery in degF          (ADC0)
#define TempEBat 65     //! This is the desired temperature of the Engine battery in degF          (ADC1)
#define TempHopper 85   //! This is the desired temperature of the hopper in degF                  (ADC2)
#define TempECU 70      //! This is the desired temperature of the ECU in degF                     (ADC3)
#define TempFLine 85    //! This is the desired temperature of the fuel line to the pump in degF   (ADC4)
#define TempELine 85    //! This is the desired temperature of the fuel line to the engine in degF (ADC5)
#define TempESB 70      //! This is the desired temperature of the ESB in degF                     (ADC6)      

/** @brief Initializes the microcontroller for its mainline execution.
 *
 *  This performs the following functions:
 *  
 *  1) Sets up the port used for the Alive LED to be an output, MOSFET control
 *  ports to be outputs, and the temperature (ADC lines) to be inputs.
 *
 *  2) Enables the interrupts for timer1 overflow.  No other interrupts
 *
 *  3) Initializes the global variables defined in the .h to the appropriate values.
 *     i.e. The mode is set to 0 (heating mode)
 *
 *  @param Void
 *  @return Void
 */
void Initial(void)
{
	sei();       //! This sets the global interrupt flag to allow for hardware interrupts
	// This is for Alex Johnson
}

/** @brief Interrupt service routine for the timer which controls the alive LED
 *
 *  This performs the following functions:
 *  
 *  1) Reloads the TCNT1H first and then the TCNT1L register (high byte first)
 *
 *  2) Toggles the state of the state of the output pin for the LED
 *
 *  3) The reseting of the interrupt flag is cleared automatically (page 113 of data sheet, make sure of this)
 *
 *  @param TIMER1_OVF_vect    The interrupt vector for the overflow of timer 1 
 *  @return Void
 */
ISR(TIMER1_OVF_vect)
{
	// remember to write to the high byte first and low byte second
	// This is for Nick Taylor
}


/** @brief Check for the completion of the ADC conversion, saves the result, and changes the channel
 *
 *  This performs the following functions:
 *
 *  1) Check to see if the ADC has completed its conversion.  If it hasn't then return.
 *
 *  2) Perform a 10 bit ADC read and save that to its appropriate global variable
 *  
 *  3) Changes the ADC multiplexer to the next channel in the cycle.  This
 *     means we want MUX4-0 to cycle through 00000 to 00111 (page 215)
 *
 *  4) Change the global variable which denotes which temperature sensor we are currently measuring on
 *
 *  5) The reseting of the interrupt flag is cleared automatically (page 113 of data sheet, make sure of this)
 *
 *  @param void
 *  @return void
 */
void tempConversion(void)
{
	// I would recommend reading through pages 201-218 of the data sheet
	// This is for Matt Robak/Nick Taylor
}

/** @brief Checks the recorded temperatures and ensures there is no overheating
 *
 *  This performs the following functions:
 *
 *  1) Compares all 7 actual temperatures with each of the desired temperatures.  If
 *     the desired temperature is reached, that heater is placed in a keep warm mode
 *
 *  2) In "keep warm" if temp falls below desired, turn on heater.  If goes above, turn off. 
 *  
 *  3) Once all temperatures have reached their desired temperatures, change the mode to fuel pumping
 *     and illuminate the warming complete light.
 *
 *  4) Continue the "keep warm" state after the mode has been changed.
 *
 *  @param void
 *  @return void
 */
void tempHeaterHelper(void)
{
	//  This is for Matt Robak/Nick Taylor
}


/** @brief Assigns the proper PWM signal to the pump to result in the desired mass flow
 *
 *  This performs the following functions:
 *
 *  1) Compares the actual value of mass flow rate to the desired value
 *
 *  2) Use control law to calculate the optimal duty cycle
 *  
 *  3) Send this pulse width to the dedicated PWM line on the microcontroller
 *
 *  4) Wait an appropriate amount of time until this is called again
 *
 *  @param massFlow A decimal number describing the current mass flow rate in g/sec
 *  @return void
 */
void pumpOperation(float massFlow)
{
	// This is for Alex Bertman/Alex Johnson
}

/** @brief Reads/times the pulse train coming from the flow meter and passes this to pumpOperation
 *
 *  This performs the following functions:
 *
 *  1) Record the number of pulses that occur within an amount of time that gives to an accurate measurement
 *
 *  2) Use the conversion factors from the flow meter's data sheet to convert this to g/sec
 *  
 *  3) Pass this value to pumpOperation
 *
 *  4) Prepare the needed timers/counters to perform this function again
 *
 *  @param void
 *  @return void
 */
void flowMeter(void)
{
	// This is for Alex Bertman/Alex Johnson
}