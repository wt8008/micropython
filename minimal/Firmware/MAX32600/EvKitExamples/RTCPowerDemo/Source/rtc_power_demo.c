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

/* $Revision: 3665 $ $Date: 2014-12-03 15:15:25 -0600 (Wed, 03 Dec 2014) $ */

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
#include "rtc.h"

static void rtc_wakeup(void)
{
    /* enable GPIOs */
    PWR_EnableGPIO();

    /* wait for 327 system ticks = 327 / 32,768Hz = 9.979 msec */
    SysTick_Wait(327);

    /* disable GPIOs */
    PWR_DisableGPIO();

    /* clear power sequencer wake-up flags */
    PWR_ClearFlags();
}

static void push_button_irq_handler(void)
{
    /* disable button released interrupt so that it won't keep going off */
    GPIO_SetIntMode(SW1_PORT, SW1_PIN, MXC_E_GPIO_INT_MODE_DISABLE, NULL);

    /* set 1Hz continuous alarm using RTC prescale mask */
    RTC_SetContAlarm(MXC_E_RTC_PRESCALE_DIV_2_12, NULL);

    /* set mode to LP1 and call rtc_wakeup on wake-up event */
    PWR_SetMode(MXC_E_PWR_MODE_LP1, rtc_wakeup);

    /* disable GPIOs */
    PWR_DisableGPIO();

    /* clear power sequencer wake-up flags */
    PWR_ClearFlags();
}

int main(void)
{
    /* enable instruction cache */
    ICC_Enable();

    /* set systick to the RTC input 32.768kHz clock, not system clock; this is needed to keep JTAG alive */
    CLKMAN_SetRTOSMode(TRUE);

    /* enable chzy regulator during sleep mode */
    PWR_EnableDevSleep(MXC_E_PWR_DEVICE_CHZY);
    /* enable real-time clock during sleep mode  */
    PWR_EnableDevSleep(MXC_E_PWR_DEVICE_RTC);
    /* disable VDD3 SVM during sleep mode */
    PWR_DisableDevSleep(MXC_E_PWR_DEVICE_SVM3);
    /* disable VREG 18 SVM during sleep mode */
    PWR_DisableDevSleep(MXC_E_PWR_DEVICE_SVM1);
    /* enable real-time clock during run mode  */
    PWR_EnableDevRun(MXC_E_PWR_DEVICE_RTC);

    /* only enable GPIO wake-up detect */
    PWR_DisableAllWakeupEvents();
    PWR_EnableWakeupEvent(MXC_E_PWR_EVENT_RTC_PRESCALE_CMP);

    /* turn on trickle charger: no diode + 250ohm */
    PWR_SetTrickleCharger(MXC_E_PWR_TRICKLE_CHARGER_NO_DIODE_W_250_OHM);

    /* set RTC prescale value to 1Hz and enable timer */
    RTC_SetPrescale(MXC_E_RTC_PRESCALE_DIV_2_12);
    RTC_Enable();

    /* initialize power sequencer to a known state */
    PWR_Init();

    /* enable GPIO clock */
    CLKMAN_SetClkScale(MXC_E_CLKMAN_CLK_GPIO, MXC_E_CLKMAN_CLK_SCALE_ENABLED);

    /* set input mode for SW1 to inverted (button push is 1) */
    GPIO_SetInMode(SW1_PORT, SW1_PIN, MXC_E_GPIO_IN_MODE_INVERTED);

    /* set interrupt mode for SW1 to falling edge (trigger on button release) */
    GPIO_SetIntMode(SW1_PORT, SW1_PIN, MXC_E_GPIO_INT_MODE_FALLING_EDGE, push_button_irq_handler);

    /* set output mode for LEDs */
    GPIO_SetOutMode(GREEN_LED_PORT, GREEN_LED_PIN, MXC_E_GPIO_OUT_MODE_OPEN_DRAIN_W_PULLUP);

    /* turn on green LED */
    GPIO_SetOutVal(GREEN_LED_PORT, GREEN_LED_PIN, LED_ON);

    for(;;) {
        PWR_Sleep();
    }
    return 0;
}
