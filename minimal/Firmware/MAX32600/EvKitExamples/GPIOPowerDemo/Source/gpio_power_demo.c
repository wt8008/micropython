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

#include <stdio.h>

/* config.h is the required application configuration; RAM layout, stack, chip type etc. */
#include "mxc_config.h" 
#include "board.h"

#include "clkman.h"
#include "power.h"
#include "tmr.h"
#include "gpio.h"
#include "icc.h"
#include "systick.h"

/* state variables */
static volatile uint8_t wakeup_flag = 0;
static volatile uint8_t lp1wu = 0; /* did we wake from lp1? init to 0 on first boot or boot from LP0 */

static void gpio_wakeup(void) /* callback fn: LP1 -> LP2 */
{
    PWR_EnableGPIO(); /* enable GPIOs */
    PWR_ClearGPIOWUD(SW1_PORT, SW1_PIN); /* clear GPIO wakeup detect latches */
    PWR_SetMode(MXC_E_PWR_MODE_LP2, NULL); /* go to LP2 */

    lp1wu = 1;       /* wakeup from LP1 */
    wakeup_flag = 1; /* set wake-up flag */
    GPIO_SetOutVal(GREEN_LED_PORT, GREEN_LED_PIN, LED_OFF); /* turn off green LED */
    GPIO_SetOutVal(YELLOW_LED_PORT, YELLOW_LED_PIN, LED_ON); /* turn on yellow LED */
}

static void gpio_sleep_setup(mxc_pwr_mode_t mode)
{
    /* enable cheezy reglulator during LP1 */
    if(mode == MXC_E_PWR_MODE_LP1)
        PWR_EnableDevSleep(MXC_E_PWR_DEVICE_CHZY);
    else
        PWR_DisableDevSleep(MXC_E_PWR_DEVICE_CHZY);

    PWR_SetGPIOWUD(SW1_PORT, SW1_PIN, SW1_ON); /* set up GPIO wake-up detect*/
    PWR_SetMode(mode, gpio_wakeup); /* set up sleep mode and callback function */
    PWR_DisableGPIO(); /* disable GPIOs */
    PWR_ClearFlags();  /* clear latches for wakeup detect*/
}

static void push_button_irq_handler(void)
{
    /* is this IRQ because of a wakeup button or a sleep button */
    if(!wakeup_flag)
    {
        if(!lp1wu) { /* go to LP1 */
            gpio_sleep_setup(MXC_E_PWR_MODE_LP1);
        } else { /* go to LP0 */
            gpio_sleep_setup(MXC_E_PWR_MODE_LP0);
        }
    } else {
        wakeup_flag = 0;
    }
}

int main(void)
{
    /* enable instruction cache */
    ICC_Enable();

    /* set systick to the RTC input 32.768kHz clock, not system clock; this is needed to keep JTAG alive */
    CLKMAN_SetRTOSMode(TRUE);

    /* check to see if it started from LP0 or fresh boot*/
    if(MXC_PWRSEQ->reg0_f.pwr_first_boot)
        wakeup_flag = 0;
    else 
        wakeup_flag = 1;

    /* disable VDD3 SVM during sleep mode */
    PWR_DisableDevSleep(MXC_E_PWR_DEVICE_SVM3);
    /* disable VREG18 SVM during sleep mode */
    PWR_DisableDevSleep(MXC_E_PWR_DEVICE_SVM1);
    /* disable real-time clock during sleep mode  */
    PWR_DisableDevSleep(MXC_E_PWR_DEVICE_RTC);

    /* turn on trickle charger: no diode + 250ohm */
    PWR_SetTrickleCharger(MXC_E_PWR_TRICKLE_CHARGER_NO_DIODE_W_250_OHM);

    /* only enable GPIO wake-up detect */
    PWR_DisableAllWakeupEvents();
    PWR_EnableWakeupEvent(MXC_E_PWR_EVENT_IO_FLAG);

    /* initialize power to a known state */
    PWR_Init();

    /* initialize the SW1_TEST button on the MAX32600 EVKIT */
    CLKMAN_SetClkScale(MXC_E_CLKMAN_CLK_GPIO, MXC_E_CLKMAN_CLK_SCALE_ENABLED);

    /* set SW1 GPIO for input and interrupt driven */
    GPIO_SetInMode(SW1_PORT, SW1_PIN, MXC_E_GPIO_IN_MODE_INVERTED);

    /* set interrupt for when button is released */
    GPIO_SetIntMode(SW1_PORT, SW1_PIN, MXC_E_GPIO_INT_MODE_FALLING_EDGE, push_button_irq_handler);

    /* set up GPIOs for LEDs */
    GPIO_SetOutMode(GREEN_LED_PORT, GREEN_LED_PIN, MXC_E_GPIO_OUT_MODE_OPEN_DRAIN_W_PULLUP);
    GPIO_SetOutMode(YELLOW_LED_PORT, YELLOW_LED_PIN, MXC_E_GPIO_OUT_MODE_OPEN_DRAIN_W_PULLUP);

    /* turn on green LED */
    GPIO_SetOutVal(GREEN_LED_PORT, GREEN_LED_PIN, LED_ON);

    for(;;) {
        PWR_Sleep();
    }
    return 0;
}
