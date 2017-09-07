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

#include <stdlib.h>
#include <stdint.h>

/* config.h is the required application configuration; RAM layout, stack, chip type etc. */
#include "mxc_config.h" 
#include "board.h"

#include "clkman.h"
#include "power.h"
#include "gpio.h"
#include "icc.h"
#include "afe.h"
#include "dac.h"
#include "adc.h"

/* these are used for the DAC, OPAMP and CLOCK Setups */
#define DAC_NUM         0 /* DAC0 */
#define OPAMP_NUM       3 /* OpAmpD */
#define DAC_RATE      373 /* DAC Rate Count */
#define PLL_BYPASS      0 /* Do NOT Bypass the PLL */
#define PLL_8MHZ_ENABLE 1 /* Enable PLL 8 MHz */
#define LOOPS           0 /* Run until Stopped */

/* 16 point sine wave 16bit ('const' will leave it in flash) */
static const uint16_t sine_wave16bit[] = {
        0x4000,
        0x4c7c,
        0x587e,
        0x638f,
        0x6d42,
        0x7537,
        0x7b21,
        0x7ec6,
        0x8000,
        0x7ec6,
        0x7b21,
        0x7537,
        0x6d42,
        0x638f,
        0x587e,
        0x4c7c,
        0x4000,
        0x3384,
        0x2782,
        0x1c71,
        0x12be,
        0xac9,
        0x4df,
        0x13a,
        0x0,
        0x13a,
        0x4df,
        0xac9,
        0x12be,
        0x1c71,
        0x2782,
        0x3384,
};


static uint32_t data_samples16bit = sizeof(sine_wave16bit)/2;
static dac_transport_t dac_wave_handle;

/* used for starting and stopping the waveform */
void trigger_wave(void) {
    static uint8_t running = 0;
    
    if(!running) {
        /* Turn on YELLOW LED */
        GPIO_SetOutVal(YELLOW_LED_PORT, YELLOW_LED_PIN, LED_ON);
        DAC_PatternStart(&dac_wave_handle);
        running = 1;
    } else {
        /* Turn off YELLOW LED */
        GPIO_SetOutVal(YELLOW_LED_PORT, YELLOW_LED_PIN, LED_OFF);
        DAC_PatternStop(&dac_wave_handle);
        running = 0;
    }
}

/* used for setting up the Voltage Reference, OpAmp, and DAC */
static void dacsine_demo_init(void)
{
    /* enable the AFE Power */
    MXC_PWRMAN->pwr_rst_ctrl_f.afe_powered = 1;

    /* setup of Voltage Reference*/
    AFE_ADCVRefEnable(MXC_E_AFE_REF_VOLT_SEL_1500);
    AFE_DACVRefEnable(MXC_E_AFE_REF_VOLT_SEL_1500, MXC_E_AFE_DAC_REF_REFADC);

    /* set positive input to OpAmpD to DAC0 in follower mode (OpAmpD is the SMA connector on EvKit) */
    AFE_OpAmpSetup(OPAMP_NUM, MXC_E_AFE_OPAMP_MODE_OPAMP, MXC_E_AFE_OPAMP_POS_IN_DAC0P, MXC_E_AFE_OPAMP_NEG_IN_OUTx, MXC_E_AFE_IN_MODE_COMP_NCH_PCH);
    AFE_OpAmpEnable(OPAMP_NUM);

    /* DAC setup */
    DAC_Enable(DAC_NUM, MXC_E_DAC_PWR_MODE_FULL);
    DAC_SetRate(DAC_NUM, DAC_RATE, MXC_E_DAC_INTERP_MODE_2_TO_1);
    DAC_SetStartMode(DAC_NUM, MXC_E_DAC_START_MODE_FIFO_NOT_EMPTY);

    /* sine wave config; handle is allocated heap RAM and used for start/stop; could be free()'d if no longer needed */
    DAC_PatternConfig(DAC_NUM, &dac_wave_handle, sine_wave16bit, data_samples16bit, LOOPS, NULL, NULL);
}

#ifndef TOP_MAIN
int main(void)
{
    /* enable instruction cache */
    ICC_Enable();

    /* use the internal Ring Osc in 24MHz mode */
    CLKMAN_PLLConfig(MXC_E_CLKMAN_PLL_INPUT_SELECT_24MHZ_RO, MXC_E_CLKMAN_PLL_DIVISOR_SELECT_8MHZ, MXC_E_CLKMAN_STABILITY_COUNT_2_13_CLKS, PLL_BYPASS, PLL_8MHZ_ENABLE);
    CLKMAN_PLLEnable();
    CLKMAN_SetSystemClock(MXC_E_CLKMAN_SYSTEM_SOURCE_SELECT_24MHZ_RO);
    CLKMAN_WaitForSystemClockStable();
    
    /* adc clock is needed for OpAmp Configuration */
    CLKMAN_SetADCClock(MXC_E_CLKMAN_ADC_SOURCE_SELECT_PLL_8MHZ, MXC_E_ADC_CLK_MODE_FULL);
    
    /* set DAC0 CLK_SCALE for 24 MHz */
    CLKMAN_SetClkScale(MXC_E_CLKMAN_CLK_DAC0, MXC_E_CLKMAN_CLK_SCALE_ENABLED);
    
    /* set systick to the RTC input 32.768kHz clock, not system clock; this is needed to keep JTAG alive */
    CLKMAN_SetRTOSMode(TRUE);

    /* setup DAC and YELLOW LED */
    dacsine_demo_init();

    /* initialize the SW1_TEST button on the MAX32600 EVKIT */
    CLKMAN_SetClkScale(MXC_E_CLKMAN_CLK_GPIO, MXC_E_CLKMAN_STABILITY_COUNT_2_23_CLKS);

    /* set SW1 GPIO for input and interrupt driven */
    GPIO_SetInMode(SW1_PORT, SW1_PIN, MXC_E_GPIO_IN_MODE_INVERTED);

    /* set interrupt for when button is released */
    /* the interrupt is a falling edge when the button is released, because the GPIO was set to an inverted level in GPIO_SetInMode */
    GPIO_SetIntMode(SW1_PORT, SW1_PIN, MXC_E_GPIO_INT_MODE_FALLING_EDGE, trigger_wave);

    /* turn on Green LED */
    GPIO_SetOutMode(GREEN_LED_PORT, GREEN_LED_PIN, MXC_E_GPIO_OUT_MODE_NORMAL);
    GPIO_SetOutVal(GREEN_LED_PORT, GREEN_LED_PIN, 0);

    /* init yellow LED to off */
    GPIO_SetOutMode(YELLOW_LED_PORT, YELLOW_LED_PIN, MXC_E_GPIO_OUT_MODE_NORMAL);
    GPIO_SetOutVal(YELLOW_LED_PORT, YELLOW_LED_PIN, LED_OFF);
    
    for(;;) {
        /* default sleep mode is "LP2"; core powered up, ARM in "Wait For Interrupt" mode */
        PWR_Sleep();
    }
    return 0;
}
#endif /* TOP_MAIN */
