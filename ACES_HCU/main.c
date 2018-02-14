/** @file main.c
 *  @author Nick Moore
 *  @date February 14, 2018
 *  @brief Mainline code for the HCU.
 *
 *  @bug No known bugs.
 */ 
#include "HCU_Funcs.h"

int main(void)
{
    Initial();
    while (1) 
    {
		tempConversion();
		if (!ECU_present && (opMode == 1))    // Will only go in here if the ECU is not present and in pumping mode
			flowMeter();
    }
}

