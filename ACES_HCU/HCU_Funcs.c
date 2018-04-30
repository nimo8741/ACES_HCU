/** @file HCU_Funcs.c
 *  @author Nick Moore
 *  @date February 14, 2018
 *  @brief Function implementations for the HCU Microcontroller.
 *
 *  This contains the implementations for the functions described
 *  in HCU_Funcs.h
 *
 *  @bug No known bugs.
 */


//! This is so that the delay functions work.  Sets clock to 1MHz
#define F_CPU 1000000UL   
#include "HCU_Funcs.h"
#include <avr/io.h>
#include <util/delay.h>   // This library is so that easy delay functions can be implemented
#include <avr/interrupt.h>
#include <float.h>



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
	DDRA = 0b10000000;          // Only PA7 is an output
	DDRB = 0b11011010;         
	DDRC = 0xFF;                // Make all outputs
	DDRD = 0xFF;                // Make all outputs
		
	
	opMode = 0;     // This sets the function mode to heating mode
	desired_temp = 0;
	duty_cycle = 0.55;    // Experimentally determined duty cycle which is pretty good. 0.53
	
	// Now calculate the number of pulses I expect per 0.262144 seconds (max time for an 8 bit timer with prescalar of 1024)
	float pulse_flow = (fuelFlow / density) * K_factor * max_time / 1000;
	V_per_pulse = pump_m * (fuelFlow / pulse_flow);
	desired_pulses = (uint8_t) pulse_flow;                                    // round down and convert to an 8 bit number.  I expect it to be 170 so it will fit.
	pulse_error_allow = (uint8_t)(desired_pulses * (fuelError / fuelFlow));   // This is the amount of pulses I can be off for it to still be considered a successes
	assign_bit(&MCUCSR,ISC2,1);                                               // This will cause interrupts for INT2 to be caused on the rising edge
	assign_bit(&GIFR, INTF2, 1);                                              // Make sure the interrupt flag is cleared
	
	
	// Configure the ADC
	ADCSRA |= 1 << ADPS2;   // This is so there is a prescalar of 16.  ADC needs frequency between 50-200kHz so 1,000,000/16 puts it in this range.
	ADCSRA |= 1 << ADEN;    // Enable the ADC
	ADMUX |= 1 << REFS0;    // Make AVCC (5V) the reference voltage

	sei();       // This sets the global interrupt flag to allow for hardware interrupts
	
	// Now enable the timer1 for 0.5 sec
	TIMSK |= 1 << TOIE1;                 // turn on overflow interrupts
	TCCR1B |= (1<<CS11);                 // This has a prescalar of 8
	TCNT1 = 3036;                        // This will load the value so that when using a prescalar of 8, it will overflow after 500ms
	
	saveTemps[0] = -100.0;        // Assign initial temperature values that for sure will be colder than the specified temps 
	saveTemps[1] = -100.0;
	saveTemps[2] = -100.0;
	saveTemps[3] = -100.0;
	saveTemps[4] = -100.0;
	saveTemps[5] = -100.0;
	saveTemps[6] = -100.0; 
	
	// Now I need to turn on all of the heaters as well as set the duty cycles for the PWMs which will be on timers 0 and 2
	// Start with the PWM for the ECU, this will be on timer0
	TCNT0 = 0;      // Clear the timer register to make sure I have the full range on the first cycle
	assign_bit(&TCCR0, WGM01, 1);
	assign_bit(&TCCR0, WGM00, 1);      // These two set the PWM Mode to "Fast PWM"
	assign_bit(&TCCR0, COM01, 1); 
	assign_bit(&TCCR0, COM00, 1);      // These two set the PWM type to inverting PWM
	
	OCR0 = 255 - (255*ECU_duty);       // This will set it to the specified duty by the #define
	
	TCCR0 |= (1 << CS02);              // This will start the PWM with a duty cycle of 65.536 ms
	
	// Now do the PWM for the second fuel line which will use Timer 2,  this will look very similar to the last few lines of code
	TCNT2 = 0;      // Clear the timer register to make sure I have the full range on the first cycle
	assign_bit(&TCCR2, WGM21, 1);
	assign_bit(&TCCR2, WGM20, 1);      // These two set the PWM Mode to "Fast PWM"
	assign_bit(&TCCR2, COM21, 1);
	assign_bit(&TCCR2, COM20, 1);      // These two set the PWM type to inverting PWM
		
	OCR2 = 255 - (255*F_line_duty);    // This will set it to the specified duty by the #define
		
	TCCR2 |= (1 << CS22);              // This will start the PWM with a duty cycle of 65.536 ms, just like before
	
	// Now turn on all the other heaters
	assign_bit(&PORTD, BatPin, 1);
	assign_bit(&PORTD, HopperPin, 1);
	assign_bit(&PORTD, FLine1Pin, 1);
	assign_bit(&PORTD, ESB_Pin, 1);     // I can omit doing this for the ECU and FuelLine1
	output_count = 0;
	hand_pwm = 7;     // This means that the fuel line 2 will have a 10 percent 
	pwm_count = 0;
				
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
		
	PORTB ^= (1 << Warm_LED);   // This will have the warming LED blink 0.5 sec on 0.5 sec off and the alive LED blinking twice as slow
		
	// Now reset the register
	TCNT1 = 3036;  // The interrupt will clear automatically when this function is called
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
 *  5) The reseting of the interrupt flag is cleared automatically
 *
 *  @param void
 *  @return void
 *  @see tempHeaterHelper
 */
void tempConversion(void)
{
	// First check if the ADC is done converting
	assign_bit(&ADMUX,MUX0,0);    // Assign channel to 0
	assign_bit(&ADMUX,MUX1,0);
	assign_bit(&ADMUX,MUX2,0);
	
	for (unsigned char i = 0; i < 6; i++)
	{
		ADCSRA |= 1 << ADSC;                             // Start the conversion
		while (!((1 << ADIF) & ADCSRA));
		
		while (bit_is_clear(ADCSRA, ADIF));				 // Hog execution until the ADC is done converting

		// Save this as a float for the respective variable
		uint8_t low_bits = ADCL;
		uint8_t high_bits = ADCH;						 // Do the shifting so that there is room made inside of the 16 bit register
		uint16_t result = (high_bits << 8) | low_bits;
		
		// Now I need to convert this 16 bit number into an actual temperature
		
		float act_temp = (float)(0.0048828125*result);   // This dumb thing converts it to a voltage
		act_temp = act_temp*208.8 - 79.6;
		saveTemps[i] = act_temp;
		
		if (i == 4){
			assign_bit(&ADMUX, MUX1,0);
		}
		// Now update the channel the ADC is using
		assign_bit(&ADMUX,MUX0,(i + 1) & 0x01);           // Assign bit 0
		assign_bit(&ADMUX,MUX1,((i + 1) >> 1) & 0x01);    // Assign bit 1
		assign_bit(&ADMUX,MUX2,((i + 1) >> 2) & 0x01);    // Assign bit 2
		
		if (i == 3){
			assign_bit(&ADMUX, MUX1,1);
		}

		
		assign_bit(&ADCSRA, ADIF, 1);     // write a logical 1 to clear the flag, page 216 in the data sheet

	}
	tempHeaterHelper();             // Call the helper function.  This will serve the added bonus of killing some time so that if capacitors need to charge for the next conversion, it has the time here.  Data sheet didn't say that it needed this though.
	if (opMode != 1)
		_delay_ms(250);                 // Delay for 1/4 of a second.   This will only impact modes 0 and 2
	
}

/** @brief Checks the recorded temperatures and ensures there is no overheating
 *
 *  This performs the following functions:
 *
 *  1) Compares all 6 actual temperatures with each of the desired temperatures.  If
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
 *  @note Need to confirm that heater operation is still sufficient for phase 1 and 2.
 *  @see tempConversion
 */
void tempHeaterHelper(void)
{
	for (uint8_t i = 0; i < 6; i++)
	{
		switch(i){
			case 0:                             // This is the case for the Lipo batteries   //////////////////////////////////////////////
				if (saveTemps[0] > TempBat )    // safety first so make sure that the temperature always turns off if one of the batteries is getting too hot
				{
					desired_temp |= 0x01;
					assign_bit(&PORTD, BatPin, 0);       // Turn the heater off if either of these get too high
				}
				else if(saveTemps[0] < TempBat)
				{
					assign_bit(&PORTD, BatPin, 1);    // Turn the heater back on to warm them up
				}
				break;
				
			case 1:       // This is the case for the Hopper    /////////////////////////////////////////////////
				if (saveTemps[1] < TempHopper)           // Temp is too low so turn on the heater
					assign_bit(&PORTD, HopperPin, 1);
				else if(saveTemps[1] > TempHopper)
				{
					assign_bit(&PORTD, HopperPin, 0);    // Too hot so turn off
					desired_temp |= 0x02;				
				}
				break;
				
			case 2:       // This is the case for the ECU  /////////////////////////////////////////////////
				if (saveTemps[2] < TempECU){
					if (!opMode){
						assign_bit(&TCCR0, COM01, 1);
						assign_bit(&TCCR0, COM00, 1);    // give the PWM its output pin back
						assign_bit(&TCCR0, CS02, 1);      // This will turn the PWM back on

					}
					else if (opMode == 2){
						// I need to implement the 10% DS
						if (pwm_count == hand_pwm){
							assign_bit(&PORTB,ECU_pin,1);   // turn it on for the one count
						}
						else{
							assign_bit(&PORTB, ECU_pin,0);  // turn it off otherwise
						}
					}
				}
				else if (saveTemps[2] > TempECU)
				{
					if (opMode != 1)
					{
						assign_bit(&TCCR0, CS02, 0);      // This will turn the PWM off
						// now force the PWM module off of the pin
						assign_bit(&TCCR0, COM01, 0);
						assign_bit(&TCCR0, COM00, 0);
						assign_bit(&PORTB,ECU_pin,0);     // and force the pin low
						desired_temp |= 0x04;
					}
					else{
						assign_bit(&PORTB,ECU_pin,0);    // drive the pin low if it is too high
					}
				}
				break;
				
			case 3:       // This is the case for Fuel Line 1  /////////////////////////////////////////////////
				if (saveTemps[3] < TempFLine1)
					assign_bit(&PORTD, FLine1Pin, 1);
				else if(saveTemps[3] > TempFLine1)
				{
					assign_bit(&PORTD, FLine1Pin, 0);
					desired_temp |= 0x08;
				}
				break;
				
			case 4:       // This is the case for Fuel Line 2 /////////////////////////////////////////////////
				if (saveTemps[4] < TempFLine2){
					if (!opMode){      // We are in the warming mode so this can use the PWM
						// force the PWM to be on
						assign_bit(&TCCR2, COM21, 1);
						assign_bit(&TCCR2, COM20, 1);   // This will set it to inverting mode
						assign_bit(&TCCR2, CS22, 1);    // this will turn the PWM on
					}
					else if (opMode == 2){
						// I need to implement the 10% DS
						if (pwm_count == hand_pwm){
							assign_bit(&PORTD,Fline2Pin,1);   // turn it on for the one count
						}
						else{
							assign_bit(&PORTD, Fline2Pin,0);  // turn it off otherwise
						}
					}
				}
				else if (saveTemps[4] > TempFLine2)
				{
					if (!opMode)           // We are in warming mode so this can use the PWM
					{
						assign_bit(&TCCR2, CS22, 0);       // Turn the PWM off
						// now need to disconnect the port from the PWM module
						assign_bit(&TCCR2, COM21, 0);
						assign_bit(&TCCR2, COM20, 0);
						assign_bit(&PORTD, Fline2Pin, 0);    // force the pin to be low
						desired_temp |= 0x10;
					}
				}
				break;
				
			case 5:       // This is the case for the ESB    /////////////////////////////////////////////////
				if (saveTemps[5] < TempESB)
					assign_bit(&PORTD, ESB_Pin, 1);
				else if(saveTemps[5] > TempESB)
				{
					assign_bit(&PORTD, ESB_Pin, 0);
					desired_temp |= 0x20;
				}
				break;
		}
		
	}
	
	if (desired_temp == 0x3F)      // Will go in here every time after it stops being mode 0 this was 3F
	{
		/* If desired_temp was 0111 1111, it would go to 1111 1111 with the or.
		*   Then the bitwise not (~) would make it 0000 0000.  And finally,
		*   the logical not (!) would make it 0000 0001 and it would go into the if statement.
		*   If desired_temp is anything but this, it will not go in here 
		*/
		if (!opMode)    // only do this if it has never gone in here before
			change_timers();                     // New initialization routine which will change the prescalars and such for the timers which will be serving different purposes
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
 *  3) Compare this against the desired flow rate and change the duty cycle on the PWM accordingly
 *
 *  4) Control the fuel flowing LED, blinking when not at the proper flow rate, steady when at the proper flow rate
 *
 *  5) Shut the pump off when there is no more fuel to pump and turn all LED's on constant
 *
 *  @param void
 *  @return void
 *  @note Need to do a pump test to ensure that the voltage to flow rate function is actually correct.
 */
void flowMeter(void)
{
	// First I need to enable interrupt on INT2
	pulse_count = 0;
	GICR |= (1 << INT2);     // enable INT2 external interrupts
	
	
	// Second I need to begin timer0
	TCNT0 = 0;                                 // Make sure the timer/counter register is cleared so the full range can be used
	assign_bit(&TIFR,TOV1,1);                  // Make sure the overflow flag is set
	assign_bit(&TCCR0,CS02,1);
	assign_bit(&TCCR0,CS01,0);
	assign_bit(&TCCR0,CS00,1);                 // TThis will start the timer with a prescalar of 1024
	
	while (!(TIFR & 0x01));                    // Hog the execution until the overflow flag is set
	
	assign_bit(&GICR, INT2, 0);                // disable external interrupts for INT2
	pump_count--;
	
	int8_t pulse_error = desired_pulses - pulse_count;   // This will be able to handle negative numbers
	measured_flow = V_per_pulse * (float) pulse_count;
	measured_flow = measured_flow / pump_m;
	flow_save[pump_count] = measured_flow;
	pulse_count_array[pump_count] = pulse_count;


	
	if (pump_lock){     // decrease the pump lock by one. 
		pump_lock--;
	}
	else if ((!pump_count))                          // There is either no more fuel or there is a stoppage.  This if statement might be the end of me...
	{
		assign_bit(&TCCR1B, CS10, 0);          // This should stop the PWM for the pump
		assign_bit(&TCCR1B, WGM12, 0);
		assign_bit(&TCCR1B, WGM13, 0);
		assign_bit(&TCCR1A, COM1B1, 0);
		assign_bit(&TCCR1A, COM1B0, 0);


		assign_bit(&PORTD, PD4, 0);            // This will drive the state of the pin low
		assign_bit(&PORTD, Fuel_LED, 1);
		TCNT2 = 60;                               // Value needed for the timer to run for 0.05 second
		assign_bit(&PORTD, Alive_LED, 1);         // Start with turning on the LED
		alive_counter = 0;                        // reset the hand made prescalar
		TCCR2 = 0x06;                             // This will start the Timer with a prescalar of 256 and stop the PWM stuff
		
		// Now need to turn off all of the heaters real quick
		assign_bit(&PORTD,PD0,0);
		assign_bit(&PORTD,PD1,0);
		assign_bit(&PORTD,PD2,0);
		assign_bit(&PORTD,PD3,0);
		assign_bit(&PORTD,PD7,0);
		assign_bit(&PORTB,PB3,0);
		
		opMode = 2;    // this is when I can view the flow data
		
	}
	else
	{
		// Now I need to compare the number of pulses I got with what I should have received
		float change = (float) pulse_error * V_per_pulse * ((float) ICR1) / pump_tot_V;   // Check page 94 in notebook for correct derivation.
		OCR1B -= (uint16_t) (change / 3.0);   // the larger the number, the slower it is to respond, but the less overshoot it has
		// The above line should immediately change the PWM as well
		
		if (pulse_error < 0)
			pulse_error = -pulse_error;          // Make it the absolute value 
		if (pulse_error <= pulse_error_allow)    // mission is a success
			PORTD |= (1 << Fuel_LED);            // Make the fuel LED just stay on
		else
			PORTD ^= (1 << Fuel_LED);            // Make the fuel LED blink saying that it is not done yet.
	}
}

/** @brief Checks if the ECU power circuit is closed and if it is not, it opens it.
 *
 *  @param[in] ECU_mode This variable denote which mode the system is configured in. 0 for dummy ECU, 1 for operational ECU
 *  @return void
 */
void ECU_toggle(uint8_t ECU_mode)
{
	assign_bit(&PORTA, ECUon_Pin, ECU_mode);   // make sure the ECU has its power circuit closed if it is an operational ECU
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
	if (val)      // This is for if I want the value to be a 1
	{
		val = (val << bit);
		*sfr |= val;
	}
	else             // This is for if I want the value to be a 0
	{
		val = ~(1 << bit);
		*sfr &= val;
	}
}

/** @brief Changes the mode of the used timers such that they can perform new functions in the pumping phase of operation.
 *
 *  This also performs the following functions:
 *  1) Turns the warming LED on so that it will remain constant
 * 
 *  2) Configures the INT2 line to accept external interrupts coming from the flow meter
 *
 *  3) Toggles the ECU to turn on (should it be present)
 *
 *  4) Sets up Timer1 to serve as the PWM controller for the flow meter
 * 
 *  5) Sets up Timer2 to command the blinking of the Alive_LED
 *
 *  @param void
 *  @return void
 *  @see flowMeter
 */
void change_timers(void)
{
	opMode = 1;                          // Change the operational mode
	assign_bit(&PORTB,Warm_LED,1);    // Turn on the LED to signal the heating sequence is complete
	pump_count = 39;   // this was 39, was 44
	ECU_toggle(ECU_present);
	
	// Now need to assign timer 2 to the alive LED
	
	if (!ECU_present)
	{
		// First change Timer 1 to serve as the PWM output port for the pump
		assign_bit(&TIMSK,TOIE1,0);    // remove overflow interrupts for timer 1
		assign_bit(&TCCR1B, CS11, 0);  // need to do this so that the prescalar will be set to 1 later
		TCCR1A |= (1 << WGM11);     // The sets one of the bits for the mode 14 waveform
		TCCR1B |= (1 << WGM12) | (1 << WGM13);   // This sets the other two bits for the waveform generation
		TCCR1A |= (1 << COM1B1) | (1 << COM1B0);   // These set the output mode
		ICR1 = 1000;     // this will set the period of oscillation to 10ms This is exactly 100 Hz on the o-scope.
		// This seems to cause the motor to operate smoothly even though the voltage across the pump oscillates much more
			
		OCR1B = ICR1 - (int)(ICR1*duty_cycle);     // This will set the count at which the PWM will change to on. Also make sure to round down to int
		assign_bit(&TCCR1B, CS10, 1);              // This should start the PWM with a prescalar of 1
		pump_lock = 5;                             // This should lock the pump at the starting duty for 2 second
		// Now the PWM should be running
			
		// Second change Timer0 to serve as the counter for the pulse train from the flow meter
		TCCR0 = 0;                                 // Turn off the PWM so the ECU stops being warmed
		assign_bit(&TCCR0,CS02,0);
		assign_bit(&TCCR0,CS01,0);
		assign_bit(&TCCR0,CS00,0);                 // TThis will make sure that the timer is stopped for now	
		
		// Third set the MCU Control and Status Register for the Interrupt Sense Control 2
		MCUCSR |= (1 << ISC2);                    // This will make interrupts occur on the rising edge, so the beginning of the pulse
		
		// Fourth set Timer2 for the 0.75/0.25 second blink of the Alive LED
		assign_bit(&PORTD, Alive_LED, 1);         // Start with turning on the LED
		TIMSK |= (1 << TOIE2);                    // Enable overflow interrupts on the timer
		TCNT2 = 11;                               // This is the value needed for the timer to run for 0.25 seconds
		alive_counter = 0;                        // Reset the hand made prescalar
		TCCR2 = 0x07;                             // Start the timer with a prescalar of 1024
		

	}
	else           // We are directly skipping the pumping phase so just set up the 0.1/0.9 second blink and turn on the pumping light
	{
		opMode = 2;                               // Change to the Exhaustion Mode
		assign_bit(&PORTD, Fuel_LED, 1);          // turn on the fuel LED
		
		// Now I need to set up Timer2 for the Alive LED
		TIMSK |= (1 << TOIE2);                    // Enable overflow interrupts on the timer
		TCNT2 = 60;                               // Value needed for the timer to run for 0.05 second
		assign_bit(&PORTD, Alive_LED, 1);         // Start with turning on the LED
		alive_counter = 0;                        // reset the hand made prescalar
		TCCR2 = 0x06;                             // This will start the Timer with a prescalar of 256 and stop the PWM stuff
		
		// now remove the other PWMS
		TCCR0 = 0;
		TCCR1B = 0;         // This set is to prevent any unexpected triggering of the heating circuits
		TCCR1A = 0;
		
	}
}

/** @brief Interrupt Service Routine which reads in the pulse train and increments a count.
 *
 *  @param[in] pulse_count This is the number which describes how many pulses have been received for the sampling period.  It is an implicit argument as it is a global variable which is not explicitly passed in.
 *  @return void
 *  @see flowMeter
 */
ISR(INT2_vect)
{
	pulse_count++;  // The interrupt flag will automatically be cleared by hardware
}


/** @brief Interrupt Service Routine which reads in the pulse train and increments a count.
 *
 *  This performs the following functions:
 *
 *  1) Switch to the right section of code depending on whether the code is in phase 1 or 2
 *
 *  2) Compares the @p alive_counter against the preset values to attain the correct blinking pattern
 *
 *  3) Toggle the LED (where appropriate), increment alive_counter (where appropriate), and reset the timer register
 *
 *  @param[in] alive_counter Variable to act as a custom prescalar for the timer
 *  @return void
 */
ISR(TIMER2_OVF_vect)
{
	if (opMode == 1)     // Operation mode 1 so do 0.75 sec on and 0.25 sec off
	{
		if (alive_counter == 2)
		{
			assign_bit(&PORTD, Alive_LED, 0);        // Turn the LED off since it is at the end of the 0.75 sec portion
			alive_counter++;
			TCNT2 = 11;
		}
		else if (alive_counter == 3)
		{
			assign_bit(&PORTD, Alive_LED, 1);        // Turn the LED back on since it is about to begin the 0.75 second portion
			alive_counter = 0;
			TCNT2 = 11;                              // Assign the correct value into the Timer register so that it goes for 0.25 sec
		}
		else                                         // This section is for if it is in the middle of the 0.75 sec portion
		{
			alive_counter++;
			TCNT2 = 11;
		}
	}
	else   // I am in operation mode 2 so I need to do 0.1 sec on 0.9 sec off
	{
		if (alive_counter == 1)
		{
			assign_bit(&PORTD, Alive_LED, 0);            // Turn off the LED since we are at the end of the 0.1 sec period
			alive_counter++;
			TCNT2 = 60;
		}
		else if (alive_counter == 19)
		{
			assign_bit(&PORTD, Alive_LED, 1);            // Turn on the LED since we are at the end of the 0.9 sec period
			alive_counter = 0;
			TCNT2 = 60;

		}
		else
		{
			alive_counter++;                             // At one of the intermediate points so just keep the LED how it is
			TCNT2 = 60; 
		}
	}
}
