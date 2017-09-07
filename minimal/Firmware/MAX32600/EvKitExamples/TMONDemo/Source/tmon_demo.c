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
#include "board.h"

/* config.h is the required application configuration; RAM layout, stack, chip type etc. */
#include "mxc_config.h" 

#include "clkman.h"
#include "power.h"
#include "gpio.h"
#include "icc.h"
#include "systick.h"
#include "uart.h"
#include "tmon.h"
#include "afe.h"
#include "adc.h"
#include "fixedptc.h"
#include "rtc.h"
#include "ioman.h"

#define OUTPUT_UART_PORT  0

#define MUX_CH_AIN9       9
#define MUX_CH_TMON       17
#define PGA_ACQ_CNT       1
#define ADC_ACQ_CNT       1
#define PGA_TRK_CNT       0x3F /* extent PGA tracking time to 64 clocks; can be lower but it may reduce the accuracy*/
#define ADC_SLP_CNT       0

static fixedpt TMON_Calculate(uint16_t *vbe, uint16_t *vr)
{
    /* Refer to TMONDemoAppNote.pdf for the formula */
    uint32_t tmp1, tmp2, temperature, ln, constant, i;
	int32_t adcOffsetUniPol = -27;

    temperature = vbe[0] - vbe[2] + vbe[3] - vbe[1];
    constant = fixedpt_rconst(0.264936405);

    for(i = 0; i < 4; i++)
		vr[i] = vr[i] - adcOffsetUniPol;

    temperature = fixedpt_fromint(temperature);
 
    tmp1 = fixedpt_div(vr[2], vr[0]);
    tmp2 = fixedpt_div(vr[1], vr[3]);

    ln = fixedpt_mul(tmp1, tmp2);
    ln = fixedpt_ln(ln);

    temperature = fixedpt_div(temperature, ln);
    temperature = fixedpt_mul(constant, temperature);
    temperature -= fixedpt_rconst(273.15);

    return temperature;
}

static void TMON_GetVbeSample(uint16_t index, uint16_t *sample)
{
    /* Setup tmon current source */
    TMON_SetCurrent(index);

    /* 
     * Set the between PGA track ending and ADC acquisition beginning; set ADC acquisition 
     * time from PGA; extent PGA tracking time to 64 clocks
     */
    ADC_SetRate(PGA_ACQ_CNT, ADC_ACQ_CNT, PGA_TRK_CNT, ADC_SLP_CNT);

    /*
     * Set ADC to single-mode-full-rate, output average only,
     * burst lenght to be 2^6, bi_pol to 1, set adc_range 
     * control to full
     */
    ADC_SetMode(MXC_E_ADC_MODE_SMPLCNT_FULL_RATE, MXC_E_ADC_AVG_MODE_FILTER_OUTPUT, 6, 1, MXC_E_ADC_RANGE_FULL);

    /* Setup ADC input multiplexor and enable the differential measurement */
    ADC_SetMuxSel(MUX_CH_TMON, 1);

    /* Enable ADC */
    ADC_Enable();

    /* Wait 0.5 ms to get stable ADC capture */
    SysTick_Wait(16);

    /* Read Vbe sample */
    *sample = ADC_ManualRead();
}

static void TMON_GetVrSample(uint16_t index, uint16_t *sample)
{
    /* Setup tmon current source */
    TMON_SetCurrent(index);

    /* 
     * Set ADC to single-mode-full-rate, output average only, burst lenght to be 2^7, 
     * bi_pol to 0, set adc_range control to full
     */
    ADC_SetMode(MXC_E_ADC_MODE_SMPLCNT_FULL_RATE, MXC_E_ADC_AVG_MODE_FILTER_OUTPUT, 7, 0, MXC_E_ADC_RANGE_FULL);

    /* Setup ADC input multiplexor and disable the differential measurement */
    ADC_SetMuxSel(MUX_CH_TMON, 0);

    /* Read Vr sample */
    *sample = ADC_ManualRead();
}

static fixedpt TMON_GetTemperature(void)
{
    uint16_t vbe[4] = {0}, vr[4] = {0}, i;

    /* Enable ADC reference voltage to 1.5V */
    AFE_ADCVRefEnable(MXC_E_AFE_REF_VOLT_SEL_1500);

    /* Setup ADC decimation filter to average 1 sample */
    ADC_SetMode(MXC_E_ADC_MODE_SMPLCNT_FULL_RATE, MXC_E_ADC_AVG_MODE_FILTER_OUTPUT, 0, 0, MXC_E_ADC_RANGE_FULL);

    /* Setup current source for internal temperature sensor*/
    TMON_Enable(0); 

    /* 
     * Set PGA gain to 2x and enable the PGA.
     * Note: The temperature snesor is a relatively high impedence source, 
     * so the acquisition time must be extended to fully settle. The 
     * PGA naturallyhas a longer acquisition time than the ADC does a 
     * given sample rate, so we will use the PGA and the following 
     * settings.
     */
    ADC_SetPGAMode(0, MXC_E_ADC_PGA_GAIN_2);

    for(i = 0; i < 4; i++)
    {
        /* Get Vbe samples */
        TMON_GetVbeSample(i, &vbe[i]);

        /* Get Vr samples */
        TMON_GetVrSample(i, &vr[i]);
    }

    /* Disable ADC reference voltage */
    AFE_ADCVRefDisable(0);
    
    /* Disable TMON */
    TMON_Disable();

    /* Disable ADC */
    ADC_Disable();

    /* Calculate the internal temperature based on the Vbe and Vr samples */
    return TMON_Calculate(vbe, vr);
}

static void rtc_one_second_irq_handler(void)
{
    fixedpt temp;
    char str[5] = {0};
    char str1[] = "\rInternal temperature: ";
    
    /* Get internal temperature*/
    temp = TMON_GetTemperature();

    /* Convert fixed point to string */
    fixedpt_str(temp, str, 1);

    /* Send out strings through UART0 */
    UART_Write(OUTPUT_UART_PORT, (uint8_t *)str1, sizeof(str1));
    UART_Write(OUTPUT_UART_PORT, (uint8_t *)str, sizeof(str));
}

int main(void)
{
    /* Enable instruction cache */
    ICC_Enable();

    /* Use external 8MHz crystal */
    if (CLKMAN_HFXConfig(0, 4, 0) == 0)
        CLKMAN_HFXEnable();

    /* Enable 8MHz PLL divider */
    if (CLKMAN_PLLConfig(MXC_E_CLKMAN_PLL_INPUT_SELECT_HFX, MXC_E_CLKMAN_PLL_DIVISOR_SELECT_8MHZ, MXC_E_CLKMAN_STABILITY_COUNT_2_13_CLKS, 0, 1) == 0)
        CLKMAN_PLLEnable();

    /* Set system clock select for 48MHz phase locked loop output divided by 2 (24MHz) */
    CLKMAN_SetSystemClock(MXC_E_CLKMAN_SYSTEM_SOURCE_SELECT_PLL_48MHZ_DIV_2);
    CLKMAN_WaitForSystemClockStable();

    /* Set ADC clock source to 8MHz PLL */
    CLKMAN_SetADCClock(MXC_E_CLKMAN_ADC_SOURCE_SELECT_PLL_8MHZ, MXC_E_ADC_CLK_MODE_FULL);

    /* Enable real-time clock during run mode, this is needed to drive the systick with the RTC crystal */
    mxc_pwrseq_reg0_t pwr_reg0 = MXC_PWRSEQ->reg0_f;
    pwr_reg0.pwr_rtcen_run = 1;
    MXC_PWRSEQ->reg0_f = pwr_reg0;

    /* Enable the AFE Power */
    PWR_Enable(MXC_E_PWR_ENABLE_AFE);

    /* Set systick to the RTC input 32.768kHz clock, not system clock; this is needed to keep JTAG alive */
    CLKMAN_SetRTOSMode(TRUE);

    /* Setup IOMux for UART0 */
    IOMAN_UART0(MXC_E_IOMAN_MAPPING_A, 1, 0, 0);

    /* Configure baud rate of UART0 to be 115200 */
    if(UART_Config(OUTPUT_UART_PORT, 115200, 0, 0, 0))
        return -1;

    /* Setup voltage reference*/
    AFE_ADCVRefEnable(MXC_E_AFE_REF_VOLT_SEL_1500);
    AFE_DACVRefEnable(MXC_E_AFE_REF_VOLT_SEL_1500, MXC_E_AFE_DAC_REF_REFADC);

    /* Enable clock control for real-time clock interrupts */
    CLKMAN_SetClkScale(MXC_E_CLKMAN_CLK_RTC_INT_SYNC, MXC_E_CLKMAN_CLK_SCALE_ENABLED);

    /* Set up GPIOs for LEDs */
    GPIO_SetOutMode(GREEN_LED_PORT, GREEN_LED_PIN, MXC_E_GPIO_OUT_MODE_OPEN_DRAIN_W_PULLUP);
    GPIO_SetOutVal(GREEN_LED_PORT, GREEN_LED_PIN, LED_ON);

    /* Reset real-time clock value */
    RTC_SetVal(0);

    /* Set prescale to divide input clock by 2^12 = 4096 (1Hz) */
    RTC_SetPrescale(MXC_E_RTC_PRESCALE_DIV_2_12);

    /* Enable real-time clock */
    RTC_Enable();

    /* Setup real-time clock interrupt to be triggered every second */
    RTC_SetContAlarm(MXC_E_RTC_PRESCALE_DIV_2_12, rtc_one_second_irq_handler);
    
    for(;;) {
        PWR_Sleep();
    }
    return 0;
}
