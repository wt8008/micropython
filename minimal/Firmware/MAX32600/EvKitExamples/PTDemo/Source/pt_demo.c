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

#define PT_RATE                      250
#define PT_LENGTH                      8
#define GPIO_PORT7_PIN0_TRAIN 0b00001110
#define GPIO_PORT7_PIN1_TRAIN 0b00100100
#define PT_PORT                        7
#define PIN_0                          0
#define PIN_1                          1
#define PT0                            0
#define PT4                            4
#define FUNC_SEL1                      1
#define FUNC_SEL2                      2

/* used for starting and stopping the pulse trains */
void trigger_wave(void) {
    static uint8_t running = 0;
    GPIO_SetOutMode(YELLOW_LED_PORT, YELLOW_LED_PIN, MXC_E_GPIO_OUT_MODE_NORMAL);
    
    if(!running) {
        /* Turn on YELLOW LED */
        GPIO_SetOutVal(YELLOW_LED_PORT, YELLOW_LED_PIN, LED_ON);
        PT_Start();
        running = 1;
    } else {
        /* Turn off YELLOW LED */
        GPIO_SetOutVal(YELLOW_LED_PORT, YELLOW_LED_PIN, LED_OFF);
        PT_Stop();
        running = 0;
    }
}

/* used for setting up the Pulse Trains */
static void pulsetrain_demo_init(void)
{
    /* Place Pulse Trains in known stopped state */
    PT_Init();

    /* Setup Pulse Train on GPIO Port 7 Pin 0 , Function Select 1 (PT0)*/
    GPIO_SetOutMode(PT_PORT, PIN_0, MXC_E_GPIO_OUT_MODE_NORMAL);
    GPIO_SetFuncSel(PT_PORT, PIN_0, FUNC_SEL1);
    PT_SetPulseTrain(PT0, PT_RATE, PT_LENGTH,GPIO_PORT7_PIN0_TRAIN );
    
    /* Setup Pulse Train on GPIO Port 7 Pin 1 , Function Select 2 (PT4)*/
    GPIO_SetOutMode(PT_PORT, PIN_1, MXC_E_GPIO_OUT_MODE_NORMAL);
    GPIO_SetFuncSel(PT_PORT, PIN_1, FUNC_SEL2);
    PT_SetPulseTrain(PT4, PT_RATE, PT_LENGTH,GPIO_PORT7_PIN1_TRAIN );
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
    CLKMAN_SetClkScale(MXC_E_CLKMAN_CLK_PT, MXC_E_CLKMAN_CLK_SCALE_DIV_256);

    /* set systick to the RTC input 32.768kHz clock, not system clock; this is needed to keep JTAG alive */
    CLKMAN_SetRTOSMode(TRUE);

    /* setup Pulse Train */
    pulsetrain_demo_init();

    /* Turn on Green LED */
    GPIO_SetOutMode(GREEN_LED_PORT, GREEN_LED_PIN, MXC_E_GPIO_OUT_MODE_NORMAL);
    GPIO_SetOutVal(GREEN_LED_PORT, GREEN_LED_PIN, 0);

    /* initialize the SW1_TEST button on the MAX32600 EVKIT */
    CLKMAN_SetClkScale(MXC_E_CLKMAN_CLK_GPIO, MXC_E_CLKMAN_STABILITY_COUNT_2_23_CLKS);

    /* set SW1 GPIO for input and interrupt driven */
    GPIO_SetInMode(SW1_PORT, SW1_PIN, MXC_E_GPIO_IN_MODE_INVERTED);

    /* set interrupt for when button is released */
    /* the interrupt is a falling edge when the button is released, because the GPIO was set to an inverted level in GPIO_SetInMode */
    GPIO_SetIntMode(SW1_PORT, SW1_PIN, MXC_E_GPIO_INT_MODE_FALLING_EDGE, trigger_wave);

    for(;;) {
        /* default sleep mode is "LP2"; core powered up, ARM in "Wait For Interrupt" mode */
        PWR_Sleep();
    }
    return 0;
}
#endif /* TOP_MAIN */
