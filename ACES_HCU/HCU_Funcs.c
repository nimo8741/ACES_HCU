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
	// 0 are inputs 1 are outputs
	DDRA = 0b10000000;          //! Only PA7 is an output
	DDRB = 0b11011010;         
	DDRC = 0xFF;                //! Make all outputs
	DDRD = 0xFF;                //! Make all outputs
	
	cur_ADC = 0;     //! This is for ADC0    
	opMode = 0;     //! This sets the function mode to heating mode
	desired_temp = 0;
	duty_cycle = (pump_m*fuelFlow + pump_b) / pump_tot_V;    //! This is the initial guess for the fuel pump
	pulse_error_allow = (uint8_t)(desired_pulses * (fuelError / fuelFlow));   //! This is the amount of pulses I can be off for it to still be considered a sucesses
	
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
	
	// Now I need to turn on all of the heaters as well as set the duty cycles for the PWMs which will be on timers 0 and 2
	// Start with the PWM for the ECU, this will be on timer0
	TCNT0 = 0;      //! Clear the timer register to make sure I have the full range on the first cycle
	assign_bit(&TCCR0, WGM01, 1);
	assign_bit(&TCCR0, WGM00, 1);      //! These two set the PWM Mode to "Fast PWM"
	assign_bit(&TCCR0, COM01, 1); 
	assign_bit(&TCCR0, COM00, 1);      //! These two set the PWM type to inverting PWM
	
	OCR0 = 255 - (255*ECU_duty);       //! This will set it to the specified duty by the #define
	
	TCCR0 |= (1 << CS02);              //! This will start the PWM with a duty cycle of 65.536 ms
	
	// Now do the PWM for the second fuel line which will use Timer 2,  this will look very similar to the last few lines of code
	TCNT2 = 0;      //! Clear the timer register to make sure I have the full range on the first cycle
	assign_bit(&TCCR2, WGM21, 1);
	assign_bit(&TCCR2, WGM20, 1);      //! These two set the PWM Mode to "Fast PWM"
	assign_bit(&TCCR2, COM21, 1);
	assign_bit(&TCCR2, COM20, 1);      //! These two set the PWM type to inverting PWM
		
	OCR2 = 255 - (255*F_line_duty);       //! This will set it to the specified duty by the #define
		
	TCCR2 |= (1 << CS22);              //! This will start the PWM with a duty cycle of 65.536 ms, just like before
	
	// Now turn on all the other heaters
	assign_bit(&PORTD, BatPin, 1);
	assign_bit(&PORTD, HopperPin, 1);
	assign_bit(&PORTD, FLine1Pin, 1);
	assign_bit(&PORTD, ESB_Pin, 1);     // I can omit doing this for the ECU and FuelLine1
		
}

/** @brief Interrupt service routine for the timer which controls the alive LED and warming LED
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
	alive_counter++;
	if (alive_counter % 2 == 1)
		PORTD ^= (1 << Alive_LED);
		
	PORTB ^= (1 << Warm_LED);   //! This will have the warming LED blink 0.5 sec on 0.5 sec off and the alive LED blinking twice as slow
	
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
	for (unsigned char i = 0; i < 6; i++)
	{
		ADCSRA |= 1 << ADSC;   //! Start the conversion
		while (bit_is_clear(ADCSRA, ADIF));   //! Hog execution until the ADC is done converting

		// Save this as a float for the respective variable
		uint8_t low_bits = ADCL;
		uint16_t ADC_res = (ADCH << 2) | (low_bits >> 6);    //! Do the shifting so that there is room made inside of the 16 bit register
		
		// Now I need to convert this 16 bit number into an actual temperature
		float act_temp = (float) ADC_res * (5/1024);      //! Use the bin number and reference voltage to get the analog voltage again
		act_temp = 208.8*act_temp - 79.6;                 //! Derivation for this on page 98 in the notebook
		saveTemps[i] = act_temp;
		
		// Now update the channel the ADC is using
		
		
		assign_bit(&ADMUX,MUX0,i & 0x01);    //! Assign bit 0
		assign_bit(&ADMUX,MUX1,i & 0x02);    //! Assign bit 1
		assign_bit(&ADMUX,MUX2,i & 0x04);    //! Assign bit 2
		
		assign_bit(&ADCSRA, ADIF, 1);     //! write a logical 1 to clear the flag, page 216 in the data sheet

	}
	tempHeaterHelper();             //! Call the helper function.  This will serve the added bonus of killing some time so that if capacitors need to charge for the next conversion, it has the time here.  Data sheet didn't say that it needed this though.
	_delay_ms(250);                 //! Delay for 1/4 of a second.   Need to check that the Alive LED will still interrupt properly
	
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
			case 0:       //! This is the case for the Lipo batteries might need to incorporate ranges for this to actually work   /////////
				if (saveTemps[0] > TempBat )    //! safety first so make sure that the temperature always turns off if one of the batteries is getting too hot
				{
					desired_temp |= 0x01;
					assign_bit(&PORTD, BatPin, 0);       //! Turn the heater off if either of these get too high
				}
				else if(saveTemps[0] < TempBat)
				{
					assign_bit(&PORTD, BatPin, 1);    // Turn the heater back on to warm them up
				}
				break;
				
			case 1:       //! This is the case for the Hopper    /////////////////////////////////////////////////
				if (saveTemps[2] < TempHopper)   //! Temp is too low so turn on the heater
					assign_bit(&PORTD, HopperPin, 1);
				else if(saveTemps[2] > TempHopper)
				{
					assign_bit(&PORTC, HopperPin, 0);   //! Too hot so turn off
					desired_temp |= 0x02;				
				}
				break;
				
			case 2:       //! This is the case for the ECU  /////////////////////////////////////////////////
				if (saveTemps[3] < TempECU)
					if (!opMode)
						assign_bit(&TCCR0, CS02, 1);      //! This will turn the PWM back on
					else
						assign_bit(&PORTB, ECU_pin, 1);   //! Turn the heater on manually
				else if (saveTemps[3] > TempECU)
				{
					if (!opMode)
					{
						assign_bit(&TCCR0, CS02, 0);      //! This will turn the PWM off
						desired_temp |= 0x04;
					}
					else
						assign_bit(&PORTB, ECU_pin, 0);   //! Turn the heater off manually.  Don't do the same thing with desired_temp for the manual mode
				}
				break;
				
			case 3:       //! This is the case for Fuel Line 1  /////////////////////////////////////////////////
				if (saveTemps[4] < TempFLine1)
					assign_bit(&PORTD, FLine1Pin, 1);
				else if(saveTemps[4] > TempFLine1)
				{
					assign_bit(&PORTD, FLine1Pin, 0);
					desired_temp |= 0x08;
				}
				break;
				
			case 4:       //! This is the case for Fuel Line 2 /////////////////////////////////////////////////
				if (saveTemps[5] < TempFLine2){
					if (!opMode)      //! We are in the warming mode so this can use the PWM
						assign_bit(&TCCR2, CS22, 1);       //! Turn the PWM back on 
					else
						assign_bit(&PORTD,Fline2Pin,1);       //! Turn the heater on manually
				}
				else if (saveTemps[5] > TempFLine2)
				{
					if (!opMode)           //! We are in warming mode so this can use the PWM
					{
						assign_bit(&TCCR2, CS22, 0);       //! Turn the PWM off
						desired_temp |= 0x10;
					}
					else
						assign_bit(&PORTD, Fline2Pin, 0);    //! Turn the heater off manually
				}
				break;
				
			case 5:       //! This is the case for the ESB    /////////////////////////////////////////////////
				if (saveTemps[5] < TempESB)
					assign_bit(&PORTD, ESB_Pin, 1);
				else if(saveTemps[6] > TempESB)
				{
					assign_bit(&PORTD, ESB_Pin, 0);
					desired_temp |= 0x20;
				}
				break;
		}
		
	}
	if (!(~(desired_temp | 0xC0)))      //! Will go in here every time after it stops being mode 0
	{
		/** If desired_temp was 0111 1111, it would go to 1111 1111 with the or.
		*   Then the bitwise not (~) would make it 0000 0000.  And finally,
		*   the logical not (!) would make it 0000 0001 and it would go into the if statement.
		*   If desired_temp is anything but this, it will not go in here 
		*/
		if (!opMode)    //! only do this if it has never gone in here before
			change_timers();                     //! New initialization routine which will change the prescalars and such for the timers which will be serving different purposes
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
 *  5) Shut the pump off when there is no more fuel to pump and turn all LED's on constant
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
	
	while (!(TIFR & 0x01));        //! Hog the execution until the overflow flag is set
	
	assign_bit(&GICR, INT2, 0);   // disable external interrupts for INT2
	
	if (!pulse_count)    //! There is either no more fuel or there is a stoppage.  This if statement might be the end of me...
	{
		opMode = 2;    //!  This means that the pumping has concluded
		assign_bit(&TCCR1B, CS10, 0);              //! This should stop the PWM for the pump
		assign_bit(&PORTD, Alive_LED, 1);
		assign_bit(&PORTD, Fuel_LED, 1);
		assign_bit(&PORTB, Warm_LED, 1);    //! Make sure all of the LEDs are solid on to signify the end of the mission. (The warming will still continue until power is turned off)
		
	}
	else
	{
		// Now I need to compare the number of pulses I got with what I should have received
		char pulse_error = desired_pulses - pulse_count;   //! This will be able to handle negative numbers
		OCR1B -= pulse_error * V_per_pulse * ICR1 / pump_tot_V;    //! Check page 94 in notebook for correct derivation.
		// The above line should immediately change the PWM as well
		if (pulse_error < 0)
			pulse_error = -pulse_error;    //! Make it the absolute value 
		if (pulse_error < pulse_error_allow) //! mission is a success
			PORTD |= (1 << Fuel_LED);        //! Make the fuel LED just stay on
		else
			PORTD ^= (1 << Fuel_LED);       //! Make the fuel LED blink saying that it is not done yet.
	
		// Now since some time has elapsed, toggle the Alive_LED
		PORTD ^= (1 << Alive_LED);       //! So the Alive_LED should blink on ~0.25 sec off ~0.25 sec
	}
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
	opMode = 1;                          //! Change the operational mode
	assign_bit(&PORTB,Warm_LED,1);    //! Turn on the LED to signal the heating sequence is complete
	ECU_toggle(ECU_present);
	
	if (!ECU_present)
	{
		// First change Timer 1 to serve as the PWM output port for the pump
		assign_bit(&TIMSK,TOIE1,0);    //! remove overflow interrupts for timer 1
		TCCR1A |= (1 << WGM11);     //! The sets one of the bits for the mode 14 waveform
		TCCR1B |= (1 << WGM12) | (1 << WGM13);   //! This sets the other two bits for the waveform generation
		TCCR1A |= (1 << COM1B1) | (1 << COM1B0);   //! These set the output mode
		ICR1 = 20000;     //! this will set the period of oscillation to 20ms
			
		OCR1B = ICR1 - (int)(ICR1*duty_cycle);     //! This will set the count at which the PWM will change to on. Also make sure to round down to int
		assign_bit(&TCCR1B, CS10, 1);              //! This should start the PWM with a prescalar of 1
		// Now the PWM should be running
			
		// Second change Timer 2 to serve as the counter for the pulse train from the flow meter
		assign_bit(&TCCR0,CS02,0);
		assign_bit(&TCCR0,CS01,0);
		assign_bit(&TCCR0,CS00,0);     //! TThis will make sure that the timer is stopped for now	
		
		// Third set the MCU Control and Status Register for the Interrupt Sense Control 2
		MCUCSR |= (1 << ISC2);         //! This will make interrupts occur on the rising edge, so the beginning of the pulse
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
