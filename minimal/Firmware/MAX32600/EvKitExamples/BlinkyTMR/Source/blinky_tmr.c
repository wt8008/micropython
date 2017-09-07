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
#include "tmr.h"

#define TMR_MODULE      0
#define DUTY            50 /* %duty cycle */
#define FUNC_SEL_TMR    6  /* timer module 0 maps to GPIO 2.2 on func select 6 */

#define BLINK_RESOLUTION MXC_E_TMR_PERIOD_UNIT_SEC
#define BLINK_PERIOD 1

int main(void)
{
    tmr32_config_t tmr; /* pointer to spaced allocated in TMRxx_Config() */
    uint32_t ticks;
    uint8_t  prescale;

    /* enable instruction cache */
    ICC_Enable();

    /* use the internal Ring Osc in 24MHz mode */
    CLKMAN_SetSystemClock(MXC_E_CLKMAN_SYSTEM_SOURCE_SELECT_24MHZ_RO);
    CLKMAN_WaitForSystemClockStable();

    /* set systick to the RTC input 32.768kHz clock, not system clock; this is needed to keep JTAG alive */
    CLKMAN_SetRTOSMode(TRUE);

    /* set output mode for GPIO port 1 pin 6 */
    GPIO_SetOutMode(GREEN_LED_PORT, GREEN_LED_PIN, MXC_E_GPIO_OUT_MODE_NORMAL);

    /* set function select for GPIO */
    GPIO_SetFuncSel(GREEN_LED_PORT, GREEN_LED_PIN, FUNC_SEL_TMR);
    
    /* get ticks and prescale value for set period and resolution */
    TMR32_PeriodToTicks(BLINK_PERIOD, BLINK_RESOLUTION, &ticks, &prescale);
    
    /* configure the timer in PWM mode */
    TMR32_Config(&tmr, TMR_MODULE, MXC_E_TMR_MODE_PWM, ticks, prescale, DUTY, 0);

    /* start timer */
    TMR32_Start(&tmr, 0);

    for(;;) {
        /* default sleep mode is "LP2"; core powered up, ARM in "Wait For Interrupt" mode */
        PWR_Sleep();
    }
    return 0;
}
