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

#include "mxc_config.h"
#include "board.h"

#include "adc.h"
#include "afe.h"

#include "ioman.h"
#include "clkman.h"
#include "power.h"
#include "gpio.h"
#include "uart.h"
#include "systick.h"
#include "icc.h"

#include <stdio.h>
#include <string.h>

/* remove 'ASCII_OUTPUT' to get binary output on the UART */
#define ASCII_OUTPUT 


/* globals for the capture data */
#define CAPT_SAMPLES 1024 /* size in samples (16bit) of each buffer */
static adc_transport_t adc_capture_handle;
static uint16_t capt_buf0[CAPT_SAMPLES];
static uint16_t capt_buf1[CAPT_SAMPLES];

/* ADC sample rate is 10KHz = 8MHz/(2^FILTER_CNT * (PGA_TRK_CNT+ADC_ACQ_CNT+7)) */

/* globals for ADC setup, voltage sample rate and power; see documentation for value definitions and sample rate equations */
#define REF_VOLTAGE MXC_E_AFE_REF_VOLT_SEL_1500 /* ADC reference voltage */
#define PGA_ACQ_CNT 0 
#define ADC_ACQ_CNT 0
#define PGA_TRK_CNT 9
#define ADC_SLP_CNT 664

#define ADC_MODE     MXC_E_ADC_MODE_SMPLCNT_LOW_POWER


#define FILTER_MODE  MXC_E_ADC_AVG_MODE_FILTER_OUTPUT  /* or ADC_FILTER_BYPASS */
#define FILTER_CNT   3                  /* number of averaging samples = 2^FILTER_CNT (FILTER_CNT range from 1-7) */
#define BIPOLAR_MODE MXC_E_ADC_BI_POL_UNIPOLAR       /* use single or Bi-Polar inputs                   */
#define BI_RANGE     MXC_E_ADC_RANGE_FULL     /* range value only applicable when BIPOLAR==TRUE  */

#define PGA_BYPASS TRUE              /* TRUE => bypass the PGA; FALSE => use the PGA      */
#define PGA_GAIN   MXC_E_ADC_PGA_GAIN_1    /* gain value only applicable when PGA_BYPASS==FALSE */

#define START_MODE MXC_E_ADC_STRT_MODE_SOFTWARE /* use either software or pulse-train to trigger the ADC samples */

#define MUX_INPUT MXC_E_ADC_PGA_MUX_CH_SEL_AIN2               /* pin(s) to sample on */
#define MUX_MODE  MXC_E_ADC_PGA_MUX_DIFF_DISABLE   /* use one pin and ground; or differential with two pins AIN+/AIN- */

#define UART_BAUD_RATE 115200


#if (ADC_MODE<2) /* simple check for fixed samples or continuous mode (not compatible with scan modes) */
#define CAPTURE_STOP TRUE  /* fill bufferes and stop */
#else
#define CAPTURE_STOP FALSE /* ping-pong between buffres until told to stop */
#endif

/* state variables; volatile for non-register saving, different ISR level accesses */
static volatile uint8_t running = 0;
static uint8_t done_buf_num = 0;

/* mutex must be volatile because its set/cleared in two different ISRs */
static volatile uint8_t uart_mutex = 0;

/* function called in ISR context */
static void uart_done(int32_t status)
{
    uart_mutex = 0;
}

#ifdef ASCII_OUTPUT

/* create an intermediate buffer to convert the ADC data to ASCII 'csv' for output,
 * 5 ascii digits + comma per sample */
static char ascii_buf0[CAPT_SAMPLES*6];
static char ascii_buf1[CAPT_SAMPLES*6];
static int bin2ascii(uint16_t *binp, char *ap, int count)
{
    int i;
    int siz = 0;
    for(i=0; i<count; i++) {
        siz += sprintf(&ap[siz],"%d",binp[i]);
        ap[siz++] = ',';
    }
    ap[siz++] = 0;
    return siz;
}

/* function called at ISR context when any buffer is completely filled */
static void capt_results(int32_t status, void *arg)
{
    int print_bytes;
    
    if(done_buf_num == 0) { 
        print_bytes = bin2ascii(capt_buf0, ascii_buf0, CAPT_SAMPLES);
        done_buf_num = 1;
        
        /* Make sure the uart is not busy sending out the previous buffer.
         * PMU priority MUST be higher number (lower priority) than UART along
         * with ability for nested ISRs for this to work if the mutex is still set
         */
        while(uart_mutex)
            __WFI(); 
        uart_mutex = 1;
        
        UART_WriteAsync(0, (uint8_t*)ascii_buf0, print_bytes, uart_done); 
    } else {
        print_bytes = bin2ascii(capt_buf1, ascii_buf1, CAPT_SAMPLES);
        done_buf_num = 0;

        while(uart_mutex)
            __WFI(); 
        uart_mutex = 1;
        
        UART_WriteAsync(0, (uint8_t*)ascii_buf1, print_bytes, uart_done); 
    }    

    if(status == 1) {  /* last capture */
        done_buf_num = 0;
        running = 0;
        
        /* Turn off YELLOW LED */
        GPIO_SetOutVal(YELLOW_LED_PORT, YELLOW_LED_PIN, LED_OFF);
    }
}

#else /* use binary data out UART */

static void capt_results(int32_t status, void *arg)
{ 
    /* Make sure the uart is not busy sending out the previous buffer.
     * PMU priority MUST be higher number (lower priority) than UART along
     * with ability for nested ISRs for this to work if the mutex is still set
     */
    while(uart_mutex)
        __WFI(); 
    uart_mutex = 1;
    
    if(done_buf_num == 0) { 
        done_buf_num = 1;
        UART_WriteAsync(0, (uint8_t*)capt_buf0, CAPT_SAMPLES*2, uart_done); 
    } else {
        done_buf_num = 0;
        UART_WriteAsync(0, (uint8_t*)capt_buf1, CAPT_SAMPLES*2, uart_done); 
    }    

    if(status == 1) {  /* last capture */
        done_buf_num = 0;
        running = 0;
        
        /* Turn off YELLOW LED */
        GPIO_SetOutVal(YELLOW_LED_PORT, YELLOW_LED_PIN, LED_OFF);
    }
}
#endif /* output switch */

void adc_simple_init(void)
{
    /* setup ADC capture handle */
    ADC_CaptureConfig(&adc_capture_handle, capt_buf0, CAPT_SAMPLES,
                      capt_buf1, CAPT_SAMPLES,
                      capt_results, capt_buf0, CAPTURE_STOP);

    /* set all parameters as defined above */
    ADC_SetRate(PGA_ACQ_CNT, ADC_ACQ_CNT, PGA_TRK_CNT, ADC_SLP_CNT);
    ADC_SetMode(ADC_MODE, FILTER_MODE, FILTER_CNT, BIPOLAR_MODE, BI_RANGE);
    ADC_SetPGAMode(PGA_BYPASS, PGA_GAIN);
    ADC_SetStartMode(START_MODE);
    ADC_SetMuxSel(MUX_INPUT, MUX_MODE);

    /* power up and initialize the ADC module */
    ADC_Enable();
}

/* ISR function called by button GPIO interrupt*/
void trigger_adc_simple_capture(void)
{
    if(!running) {
        /* Turn on YELLOW LED */
        GPIO_SetOutVal(YELLOW_LED_PORT, YELLOW_LED_PIN, LED_ON);
        ADC_CaptureStart(&adc_capture_handle);
        running = 1;
    } else {
        ADC_CaptureStop(&adc_capture_handle);
    }
}

#ifndef TOP_MAIN
int main(void)
{
    /* enable instruction cache */
    ICC_Enable();

    /* set systick to the RTC input 32.768kHz clock, not system clock; this is needed to keep JTAG alive */
    CLKMAN_SetRTOSMode(TRUE);
    
    /* use the internal Ring Osc in 24MHz mode */
    CLKMAN_SetSystemClock(MXC_E_CLKMAN_SYSTEM_SOURCE_SELECT_24MHZ_RO);
    CLKMAN_WaitForSystemClockStable();

    /* use the PLL to generate ADC clock */
    CLKMAN_PLLConfig(MXC_E_CLKMAN_PLL_INPUT_SELECT_24MHZ_RO, MXC_E_CLKMAN_PLL_DIVISOR_SELECT_24MHZ, MXC_E_CLKMAN_STABILITY_COUNT_2_13_CLKS, FALSE, TRUE);
    CLKMAN_PLLEnable();

    /* trim the internal clock to get more accurate BAUD rate */
    CLKMAN_TrimRO_Start();
    SysTick_Wait(1635); /* let it run about 50ms */
    CLKMAN_TrimRO_Stop();

    /* enable the AFE Power */
    PWR_Enable(MXC_E_PWR_ENABLE_AFE);

    /* ADC Clock must be 8MHz or lower */
    CLKMAN_SetADCClock(MXC_E_CLKMAN_ADC_SOURCE_SELECT_PLL_8MHZ, MXC_E_ADC_CLK_MODE_FULL);

    /* setup UART0 pins, mapping as EvKit to onboard FTDI UART to USB */
    IOMAN_UART0(MXC_E_IOMAN_MAPPING_A, TRUE, FALSE, FALSE);

    /* setup UART */
    UART_Config(0, UART_BAUD_RATE, FALSE, FALSE, FALSE);
    
    /* initialize the SW1_TEST button on the MAX32600EVKIT for manual trigger of application */
    CLKMAN_SetClkScale(MXC_E_CLKMAN_CLK_GPIO, MXC_E_CLKMAN_STABILITY_COUNT_2_23_CLKS); /* slowest GPIO clock; its a human button */
    GPIO_SetInMode(SW1_PORT, SW1_PIN, MXC_E_GPIO_IN_MODE_INVERTED);
    GPIO_SetIntMode(SW1_PORT, SW1_PIN, MXC_E_GPIO_INT_MODE_FALLING_EDGE, trigger_adc_simple_capture); /* trigger on button release */

    /* setup GPIO for green LED status on EvKit */
    GPIO_SetOutMode(GREEN_LED_PORT, GREEN_LED_PIN, MXC_E_GPIO_OUT_MODE_OPEN_DRAIN_W_PULLUP);
    GPIO_SetOutVal(GREEN_LED_PORT, GREEN_LED_PIN, LED_ON);

    /* yellow LED will indicate active capture */
    GPIO_SetOutMode(YELLOW_LED_PORT, YELLOW_LED_PIN, MXC_E_GPIO_OUT_MODE_NORMAL);
    GPIO_SetOutVal(YELLOW_LED_PORT, YELLOW_LED_PIN, LED_OFF);

    /* setup of Voltage Reference*/
    AFE_ADCVRefEnable(REF_VOLTAGE);

    /* set interrupt priority for PMU(ADC) lower (higher number) than UART (UART is default==0) */
    NVIC_SetPriority(PMU_IRQn, 1);
    
    /* local adc init function */
    adc_simple_init();
    
    for(;;) {
        /* default sleep mode is "LP2"; core powered up, ARM in "Wait For Interrupt" mode */
        PWR_Sleep();
    }
    return 0;
}
#endif
