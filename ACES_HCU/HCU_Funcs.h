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


#ifndef HCU_FUNCS_H_
#define HCU_FUNCS_H_

//////////////////////////////////////////////////////////////////////////
                          //Functions//
//////////////////////////////////////////////////////////////////////////

void Initial(void);
void tempConversion(void);
void tempHeaterHelper(void);
void pumpOperation(float massFlow);
void flowMeter(void);

//////////////////////////////////////////////////////////////////////////
						//Global Variables//
//////////////////////////////////////////////////////////////////////////
float massFlowActual;  //! This is the actual mass flow rate which the flow meter is indicating with the pulse train
char opMode;           //! This is the operational mode the system is in.  0 for heating, 1 for pumping


#endif /* HCU_FUNCS_H_ */