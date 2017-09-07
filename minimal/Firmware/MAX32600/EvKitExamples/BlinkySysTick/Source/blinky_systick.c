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

#include "systick.h"

/* SysTick will be tied to 32KHz RTC clock */
#define SYSTICK_PERIOD  32 /* make about 0.98ms timeout (32.768KHz/32 = 1024Hz) */

void blinky_init(void)
{
    /* setup GPIO for the LED */
    GPIO_SetOutMode(GREEN_LED_PORT, GREEN_LED_PIN, MXC_E_GPIO_OUT_MODE_OPEN_DRAIN_W_PULLUP);
    GPIO_SetOutVal(GREEN_LED_PORT, GREEN_LED_PIN, LED_ON);
}

void toggle_green_led(void)
{
    static uint8_t led_state;
    
    if(led_state) {
        led_state = 0;
    } else {
        led_state = 1;
    }
    GPIO_SetOutVal(GREEN_LED_PORT, GREEN_LED_PIN, led_state);
}

/* called in interrupt context */
static void systick_handler(uint32_t ticks)
{
    /* toggle state every 512 ticks (1024Hz/512 => 0.5 sec) for a 1Hz blink
       'ticks' lower 9 bits will be 0 every 512 itterations as a counter.
     */
    if((ticks & 0x1ff) == 0) /* use a bitmask for quick ISR level check */
    {
        toggle_green_led();
    }    
}

#ifndef TOP_MAIN
int main(void)
{
    /* enable instruction cache */
    ICC_Enable();

    /* use the internal Ring Osc in 24MHz mode */
    CLKMAN_SetSystemClock(MXC_E_CLKMAN_SYSTEM_SOURCE_SELECT_24MHZ_RO);
    
    /* enable systick and set to the RTC input 32.768KHz; this is needed to keep JTAG alive */
    CLKMAN_SetRTOSMode(TRUE);

    /* enable real-time clock during run mode, this is needed to drive the systick with the RTC crystal */
    PWR_EnableDevRun(MXC_E_PWR_DEVICE_RTC);

    /* setup LED */
    blinky_init();

    /* tie interrupt handler to systick */
    SysTick_Config(SYSTICK_PERIOD, systick_handler);
    
    for(;;) {
        /* default sleep mode is "LP2"; core powered up, ARM in "Wait For Interrupt" mode */
        PWR_Sleep();
    }
    return 0;
}
#endif /* TOP_MAIN */
