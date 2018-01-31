/*
 * ACES_HCU.c
 *
 * Created: 1/16/2018 10:35:41 PM
 * Author : Nick
 */ 

#include "HCU_Funcs.h"

int main(void)  // This is for Nick Moore
{
    Initial();
    while (1) 
    {
		tempConversion();
		if (!ECU_present && (opMode == 1))    //! Will only go in here if the ECU is not present and in pumping mode
			flowMeter();
    }
}

