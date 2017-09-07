/*******************************************************************************
* Copyright (C) 2014 Maxim Integrated Products, Inc., All Rights Reserved.
*
* Permission is hereby granted, free of charge, to any person obtaining a
* copy of this software and associated documentation files (the "Software"),
* to deal in the Software without restriction, including without limitation
* the rights to use, copy, modify, merge, publish, distribute, sublicense,
* and/or sell copies of the Software, and to permit persons to whom the
* Software is furnished to do so, subject to the following conditions:
*
* The above copyright notice and this permission notice shall be included
* in all copies or substantial portions of the Software.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
* OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
* IN NO EVENT SHALL MAXIM INTEGRATED BE LIABLE FOR ANY CLAIM, DAMAGES
* OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
* ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
* OTHER DEALINGS IN THE SOFTWARE.
*
* Except as contained in this notice, the name of Maxim Integrated 
* Products, Inc. shall not be used except as stated in the Maxim Integrated 
* Products, Inc. Branding Policy.
*
* The mere transfer of this software does not imply any licenses
* of trade secrets, proprietary technology, copyrights, patents,
* trademarks, maskwork rights, or any other form of intellectual
* property whatsoever. Maxim Integrated Products, Inc. retains all 
* ownership rights.
*******************************************************************************
*/

/* $Revision: 3568 $ $Date: 2014-11-13 16:17:25 -0600 (Thu, 13 Nov 2014) $ */

/* config.h is the required application configuration; RAM layout, stack, chip type etc. */
#include "mxc_config.h" 
#include "board.h"

#include "clkman.h"
#include "power.h"
#include "gpio.h"
#include "icc.h"
#include "pt.h"

#define PT_CLOCK        MXC_E_CLKMAN_CLK_SCALE_DIV_256 /* set Pulse Train CLK_SCALE for 0.09275 MHz (24MHz/256) */
#define PT_RATE         46899 /* 1Hz = 0.09375 MHz/(2*(46899-1)) */
#define PT_LENGTH       1 /* square wave */
#define PT_TRAIN        0 /* not used when PT_LENGTH = 1 */

#define PT_MODULE       6 /* any unused PT module that maps to our needed GPIO will work */
#define FUNC_SEL        1 /* PTModule 6 maps to FuncSel 1 on GPIO 2.6 */


/* used for setting up the Pulse Trains */
void blinkypt_demo_init(void)
{
    /* Place Pulse Trains in known stopped state */
    PT_Init();
    
    /* Setup Pulse Train on GPIO Port 1 Pin 6, Function Select 1 (PT_MODULE==6)*/
    GPIO_SetOutMode(GREEN_LED_PORT, GREEN_LED_PIN, MXC_E_GPIO_OUT_MODE_NORMAL);
    GPIO_SetFuncSel(GREEN_LED_PORT, GREEN_LED_PIN, FUNC_SEL);
    PT_SetPulseTrain(PT_MODULE, PT_RATE, PT_LENGTH, PT_TRAIN );

    PT_Start();
}

#ifndef TOP_MAIN
int main(void)
{
    /* enable instruction cache */
    ICC_Enable();

    /* use the internal Ring Osc in 24MHz mode */
    CLKMAN_SetSystemClock(MXC_E_CLKMAN_SYSTEM_SOURCE_SELECT_24MHZ_RO);
    CLKMAN_WaitForSystemClockStable();
    
    /* set Pulse Train CLK_SCALE for 0.09275 MHz */
    CLKMAN_SetClkScale(MXC_E_CLKMAN_CLK_PT, PT_CLOCK);

    /* set systick to the RTC input 32.768kHz clock, not system clock; this is needed to keep JTAG alive */
    CLKMAN_SetRTOSMode(TRUE);

    /* setup Pulse Train */
    blinkypt_demo_init();

    for(;;) {
        /* default sleep mode is "LP2"; core powered up, ARM in "Wait For Interrupt" mode */
        PWR_Sleep();
    }
    return 0;
}
#endif /* TOP_MAIN */
