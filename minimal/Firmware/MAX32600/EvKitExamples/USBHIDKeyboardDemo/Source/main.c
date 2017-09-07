/*******************************************************************************
 * Copyright (C) 2015 Maxim Integrated Products, Inc., All Rights Reserved.
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
 *
 * $Id: main.c 3927 2015-01-16 21:29:45Z jeremy.brodt $
 *
 *******************************************************************************
 */

/* config.h is the required application configuration; RAM layout, stack, chip type etc. */
#include "mxc_config.h" 
#include "board.h"
#include "icc.h"
#include "clkman.h"
#include "power.h"
#include "usb_app.h"

//******************************************************************************
int main(void)
{
    /* enable instruction cache */
    ICC_Enable();

    /* enable systick and set to the RTC input 32.768KHz; this is needed to keep JTAG alive */
    CLKMAN_SetRTOSMode(TRUE);

    /* use external 8Mhz crystal */
    if (CLKMAN_HFXConfig(0, 4, 0) == 0) {
        CLKMAN_HFXEnable();
    }

    CLKMAN_PLLConfig(MXC_E_CLKMAN_PLL_INPUT_SELECT_HFX, MXC_E_CLKMAN_PLL_DIVISOR_SELECT_8MHZ, MXC_E_CLKMAN_STABILITY_COUNT_2_13_CLKS, FALSE, TRUE);
    CLKMAN_PLLEnable();

    CLKMAN_SetSystemClock(MXC_E_CLKMAN_SYSTEM_SOURCE_SELECT_PLL_48MHZ_DIV_2);
    CLKMAN_WaitForSystemClockStable();
    
    // Initialize the USB application and class driver
    if (usb_app_init() != 0) {
      while(1);
    }

    for(;;) {
        /* default sleep mode is "LP2"; core powered up, ARM in "Wait For Interrupt" mode */
        PWR_Sleep();
    }
    return 0;
}
