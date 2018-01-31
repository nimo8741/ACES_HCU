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
		if (!opMode){  // This is in heating mode
			tempConversion();
		}
		else{ // This is in pumping mode
			tempConversion();
			if (!ECU_present)
				flowMeter();   //! The ECU is a dummy ECU so the HCU must manage the fuel flow
		}
    }
}

