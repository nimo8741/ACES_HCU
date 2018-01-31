/** @file HCU_Funcs.c
 *  @brief Function implementations for the Master Microcontroller.
 *
 *  This contains the implementations for the functions described
 *  in HCU_Funcs.h
 *
 *  @author Nick Moore
 *
 *  @bug No known bugs.
 */

#define F_CPU 1000000UL   // This is so that the delay functions work.  Sets clock to 1MHz
#include "HCU_Funcs.h"
#include <avr/io.h>
#include <util/delay.h>   // This library is so that easy delay functions can be implemented
#include <avr/interrupt.h>

#define bit_is_set(sfr,bit) \
		(_SFR_BYTE(sfr) & _BV(bit))              //! Simple function define for testing if the bit is set

#define bit_is_clear(sfr,bit) \
		(!(_SFR_BYTE(sfr) & _BV(bit)))           //! Simple function define for testing if the bit is clear
				

#define fuelFlow 4.8    //! This is the desired mass flow rate of fuel in g/sec

#define TempHBatMin 65        //! This is the minimum desired temperature of the Heater battery in degF          (ADC0)
#define TempHBatMax 68        //! This is the maximum desired temperature of the Heater battery in degF
#define TempEBatMin 65        //! This is the minimum desired temperature of the Engine battery in degF          (ADC1)
#define TempEBatMax 68        //! This is the maximum desired temperature of the Engine battery in degF
#define TempHopperMin 85      //! This is the minimum desired temperature of the hopper in degF                  (ADC2)
#define TempHopperMax 88      //! This is the maximum desired temperature of the hopper in degF
#define TempECUMin 70         //! This is the minimum desired temperature of the ECU in degF                     (ADC3)
#define TempECUMax 73         //! This is the maximum desired temperature of the ECU in degF
#define TempFLine1Min 85      //! This is the minimum desired temperature of the fuel line to the pump in degF   (ADC4)
#define TempFLine1Max 88      //! This is the maximum desired temperature of the fuel line to the pump in degF
#define TempFLine2Min 85      //! This is the minimum desired temperature of the fuel line to the engine in degF (ADC5)
#define TempFLine2Max 88      //! This is the maximum desired temperature of the fuel line to the engine in degF
#define TempESBMin 70         //! This is the minimum desired temperature of the ESB in degF                     (ADC6)      
#define TempESBMax 73         //! This is the maximum desired temperature of the ESB in degF

/** The reason why there are maximum and minimum values for this is because
	there is a lot of loss associated with the toggling of the high power MOSFETS
*/

#define BatPin 0        //! Pin location for the Lipo Battery heating circuit
#define HopperPin 2     //! Pin location for the Hopper heating circuit
#define ECU_Pin 3       //! Pin location for the ECU heating circuit
#define FLine1Pin 4     //! Pin location for the heating circuit for the first section of the fuel line
#define FLine2Pin 5     //! Pin location for the heating circuit for the second section of the fuel line
#define ESB_Pin 6       //! Pin location for the ESB heating circuit

#define pumpPin 7
#define heatDonePin 8
#define ECUon_Pin 0                      //! Pin to close the power circuit for the ECU
#define K_factor 110000                  //! This is the assumed K factor for the flow meter in pulses per liter
#define density 0.81                     //! This is the density of the kerosene in g/ml
#define max_time 0.262144                //! This is the maximum amount of time for an 8 bit timer with a prescalar of 1024
#define pump_m 0.382587                  //! This is the slope for the linear relationship between voltage and mass flow (put in mf, get volts)
#define pump_b 0.195783                  //! This is the y-intercept for the linear relationship between voltage and mass flow
#define pump_tot_V 6                     //! This is the total voltage which will be sent to the pump
#define V_per_pulse 0.0233708            //! This is the amount volts required to get a single change in pulse count

/** @brief Initializes the microcontroller for its mainline execution.
 *
 *  This performs the following functions:
 *  
 *  1) Sets up the port used for the Alive LED to be an output, MOSFET control
 *  ports to be outputs, and the temperature (ADC lines) to be inputs. 0 is input and 1 is output
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
	// First setup the port directions for the PWM lines and the
	// make the entire B port outputs, A port inputs, C port outputs, D ports to inputs
	DDRA = 0x00;
	DDRB = 0xFF;
	DDRC = 0xFF;
	DDRD = 0x00;
	DDRD |= (1 << PB5);     //! This will allow the LED light to be an output
	
	cur_ADC = 0;     //! This is for ADC0    
	opMode = 0;     //! This sets the function mode to heating mode
	desired_temp = 0;
	duty_cycle = (pump_m*fuelFlow + pump_b) / pump_tot_V;    //! This is the initial guess for the fuel pump
	
	// Now calculate the number of pulses I expect per 0.262144 seconds (max time for an 8 bit timer with prescalar of 1024)
	float pulse_flow = (fuelFlow / density) * K_factor * max_time / 1000;
	desired_pulses = (uint8_t) pulse_flow;     // round down and convert to an 8 bit number.  I expect it to be 170 so it will fit.
	assign_bit(&MCUCSR,ISC2,1);      //! This will cause interrupts for INT2 to be caused on the rising edge
	assign_bit(&GIFR, INTF2, 1);     //! Make sure the interrupt flag is cleared
	
	// Configure the ADC
	ADCSRA |= 1 << ADPS2;   //! This is so there is a prescalar of 16.  ADC needs frequency between 50-200kHz so 1,000,000/16 puts it in this range.
	ADMUX |= 1 << ADLAR;    //! Cause the output registers to be left justified.  Check page 217 in the data sheet.
	ADCSRA |= 1 << ADEN;    //! Enable the ADC

	sei();       //! This sets the global interrupt flag to allow for hardware interrupts
	
	// Now enable the timer1 for 0.5 sec
	TIMSK |= 1 << TOIE1;   //! turn on overflow interrupts
	TCCR1B |= (1<<CS11);    //! This has a prescalar of 8
	TCNT1 = 3036;   //! This will load the value so that when using a prescalar of 8, it will overflow after 500ms
	
	saveTemps[0] = -100.0;        //! Assign initial temperature values that for sure will be colder than the specified temps 
	saveTemps[1] = -100.0;
	saveTemps[2] = -100.0;
	saveTemps[3] = -100.0;
	saveTemps[4] = -100.0;
	saveTemps[5] = -100.0;
	saveTemps[6] = -100.0; 
	
	ADCSRA |= (1 << ADSC);    //! Begin the first conversion

}

/** @brief Interrupt service routine for the timer which controls the alive LED
 *
 *  This performs the following functions:
 *  
 *  1) Reloads the TCNT1 register with 3036 (this will allow for 500ms period)
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
	// The LED is on PD5
	PORTD ^= (1 << PB5);
	
	// Now reset the register
	TCNT1 = 3036;  //! The interrupt will clear automatically when this function is called
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
	// First check if the ADC is done converting
	if (bit_is_clear(ADCSRA, ADIF))   // If this returns true, it means the conversion is not complete which means it is still running
	{
		return;   
	}
	else   // it is done converting
	{
		// Save this as a float for the respective variable
		uint8_t low_bits = ADCL;
		uint16_t ADC_res = (ADCH << 2) | (low_bits >> 6);    //! Do the shifting so that there is room made inside of the 16 bit register
		
		// Now I need to convert this 16 bit number into an actual temperature
		
		float act_temp = (float) ADC_res * 10.0 + 3;      // THIS CONVERSION SCHEME IS NOT CORRECT I ONLY HAVE IT HERE FOR FORM!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
		saveTemps[cur_ADC] = act_temp;
		cur_ADC++;
		if (cur_ADC / 8)   //! If the cur_ADC goes up to 8 then reset it at 0
			cur_ADC = 0;
			
		// Now update the channel the ADC is using 
		
		
		assign_bit(&ADMUX,MUX0,cur_ADC & 0x01);    //! Assign bit 0
		assign_bit(&ADMUX,MUX1,cur_ADC & 0x02);    //! Assign bit 1
		assign_bit(&ADMUX,MUX2,cur_ADC & 0x04);    //! Assign bit 2
				
		
		tempHeaterHelper();             //! Call the helper function.  This will serve the added bonus of killing some time so that if capacitors need to charge for the next conversion, it has the time here.  Data sheet didn't say that it needed this though.
		
		
		ADCSRA |= 1 << ADSC;   //! Start the next conversion
	}
}

/** @brief Checks the recorded temperatures and ensures there is no overheating
 *
 *  This performs the following functions:
 *
 *  1) Compares all 7 actual temperatures with each of the desired temperatures.  If
 *     the desired temperature is reached, that heater is placed in a keep warm mode
 *
 *  2) In "keep warm" if temp falls below minimum desired, turn on heater.  If goes above maximum, turn off. 
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
	for (uint8_t i = 0; i < 6; i++)
	{
		switch(i){
			case 0:       //! This is the case for the Heater battery
				if (saveTemps[0] > TempHBatMax || saveTemps[1] > TempEBatMax)    //! safety first so make sure that the temperature always turns off if one of the batteries is getting too hot
					assign_bit(&PORTC, BatPin, 0);       // Make sure this is the correct port and pin location
				else if(saveTemps[0] < TempHBatMin || saveTemps[1] < TempEBatMin)
					assign_bit(&PORTC, BatPin, 1);
				else                   //! It must have reached its target temperature 
					desired_temp |= 0x03;      
				break;
				
			case 1:       //! This is the case for the Hopper 
				if (saveTemps[2] < TempHopperMin)   //! Temp is too low so turn on the heater
					assign_bit(&PORTC, HopperPin, 1);
				else if(saveTemps[2] > TempHopperMax)
					assign_bit(&PORTC, HopperPin, 0);   //! Too hot so turn off
				else
					desired_temp |= 0x04;
				break;
				
			case 2:       //! This is the case for the ECU
				if (saveTemps[3] < TempECUMin)
					assign_bit(&PORTC, ECU_Pin, 1);
				else if (saveTemps[3] > TempECUMax)
					assign_bit(&PORTC, ECU_Pin, 0);
				else
					desired_temp |= 0x08;
				break;
				
			case 3:       //! This is the case for Fuel Line 1
				if (saveTemps[4] < TempFLine1Min)
					assign_bit(&PORTC, FLine1Pin, 1);
				else if(saveTemps[4] > TempFLine1Max)
					assign_bit(&PORTC, FLine1Pin, 0);
				else
					desired_temp |= 0x10;
				break;
				
			case 4:       //! This is the case for Fuel Line 2
				if (saveTemps[5] < TempFLine2Min)
					assign_bit(&PORTC, FLine2Pin, 1);
				else if (saveTemps[5] > TempFLine2Max)
					assign_bit(&PORTC, FLine2Pin, 0);
				else
					desired_temp |= 0x20;
				break;
				
			case 5:       //! This is the case for the ESB
				if (saveTemps[5] < TempESBMin)
					assign_bit(&PORTC, ESB_Pin, 1);
				else if(saveTemps[6] > TempESBMax)
					assign_bit(&PORTC, ESB_Pin, 0);
				else
					desired_temp |= 0x40;
				break;
		}
		
	}
	if (!(~(desired_temp | 0x80))) 
	{
		/** If desired_temp was 0111 1111, it would go to 1111 1111 with the or.
		*   Then the bitwise not (~) would make it 0000 0000.  And finally,
		*   the logical not (!) would make it 0000 0001 and it would go into the if statement.
		*   If desired_temp is anything but this, it will not go in here 
		*/
		change_timers();                     //! New initialization routine which will change the prescalars and such for the timers which will be serving different purposes
		opMode = 1;                          //! Change the operational mode
		assign_bit(&PORTC,heatDonePin,1);    //! Turn on the LED to signal the heating sequence is complete
		ECU_toggle(ECU_present);
		
		
	}
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
	// First I need to enable interrupt on INT2
	pulse_count = 0;
	GICR |= (1 << INT2);     //! enable INT2 external interrupts
	// Second I need to begin timer2
	
	TCNT0 = 0;                     //! Make sure the timer/counter register is cleared so the full range can be used
	assign_bit(&TCCR0,CS02,1);
	assign_bit(&TCCR0,CS01,0);
	assign_bit(&TCCR0,CS00,1);     //! TThis will start the timer with a prescalar of 1024
	
	while (!(TIFR & 0x01));    //! Hog the execution until the overflow flag is set
	
	assign_bit(&GICR, INT2, 0);   // disable external interrupts for INT2
	
	// Now I need to compare the number of pulses I got with what I should have received
	char pulse_error = desired_pulses - pulse_count;   //! This will be able to handle negative numbers
	OCR1B -= pulse_error * V_per_pulse * ICR1 / pump_tot_V;    //! Check page 94 in notebook for correct derivation.
	// The above line should immediately change the PWM as well
	
}

/** @brief Checks if the ECU power circuit is closed and if it is not, it opens it.
 *
 *  @param[in] ECU_mode This variable denote which mode the system is configured in. 0 for dummy ECU, 1 for operational ECU
 *  @return void
 */
void ECU_toggle(uint8_t ECU_mode)
{
	assign_bit(&PORTA, ECUon_Pin, ECU_mode);   //! make sure the ECU has its power circuit closed if it is an operational ECU
}

/** @brief Sets the specified bit to the specified value or does nothing if it already set to that.
 *
 *  @param[out] sfr Pointer to Special Function Register the bit is located in.
 *  @param[in] bit Denotes the bit which would like to be changed.
 *  @param[in] val The value, either 1 or 0, that the user would like the bit to be after the function call.
 *  @return void
 */
void assign_bit(volatile uint8_t *sfr,uint8_t bit, uint8_t val)
{
	// First I need to figure out if that bit is already set or not
	val ^= 0x01;      //! This will flip between 0 and 1
	val = (val << bit) ^ 0xFF;
	*sfr &= val;

}

/** @brief Changes the mode of the used timers such that they can perform new functions in the pumping phase of operation.
 *
 *  @param void
 *  @return void
 */
void change_timers(void)
{
	if (!opMode)   //! This is so that it only enters this section of code once
	{
		// First change Timer 1 to serve as the PWM output port for the pump
		assign_bit(&TIMSK,TOIE1,0);    //! remove overflow interrupts for timer 1
		TCCR1A |= (1 << WGM11);     //! The sets one of the bits for the mode 14 waveform
		TCCR1B |= (1 << WGM12) | (1 << WGM13);   //! This sets the other two bits for the waveform generation
		TCCR1A |= (1 << COM1B1) | (1 << COM1B0);   //! These set the output mode
		ICR1 = 20000;     //! this will set the period of oscillation to 20ms
			
		OCR1B = ICR1 - (int)(ICR1*duty_cycle);     //! This will set the count at which the PWM will change to on. Also make sure to round down to int
		
		// Now the PWM should be running
			
		// Second change Timer 2 to serve as the counter for the pulse train from the flow meter
		assign_bit(&TCCR0,CS02,0);
		assign_bit(&TCCR0,CS01,0);
		assign_bit(&TCCR0,CS00,0);     //! TThis will make sure that the timer is stopped for now
	}
		
}

/** @brief Interrupt Service Routine which reads in the pulse train and increments a count.
 *
 *  @param[in] pulse_count This is the number which describes how many pulses have been received for the sampling period.
 *  @return void
 */
ISR(INT2_vect)
{
	pulse_count++;  // The interrupt flag will automatically be cleared by hardware
}
