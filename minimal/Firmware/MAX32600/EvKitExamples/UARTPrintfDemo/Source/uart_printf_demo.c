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

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/stat.h>

#include "clkman.h"
#include "power.h"
#include "gpio.h"
#include "icc.h"
#include "uart.h"
#include "ioman.h"
#include "systick.h"

#define UART_PORT       0
#define BAUD_RATE       115200

/* buffer space used by libc stdout */ 
static char uart_stdout_buf0[96];

/* The following libc stub functions are required for a proper link with printf().
 * These can be tailored for a complete stdio implimentation
 */
int _open(const char *name, int flags, int mode)
{
    return -1;
}
int _close(int file)
{
    return -1;
}
int _isatty(int file)
{
    return -1;
}
int _lseek(int file, off_t offset, int whence)
{
    return -1;
}
int _fstat(int file, struct stat *st)
{
    return -1;
}
int _read(int file, uint8_t *ptr, size_t len)
{
    return -1;
}

/* newlib/libc printf() will eventually call _write() to get the data to the stdout */
int _write(int file, char *ptr, int len)
{
    int ret_val = UART_Write(UART_PORT, (uint8_t*)ptr, len);
    
    /* auto insert a carrage return to be nice to terminals 
     * we enabled "buffered IO", therefore this will be called for 
     * every '\n' in printf() 
     */
    if(ptr[len-1] == '\n')
        UART_Write(UART_PORT, (uint8_t*)"\r", 1);

    return ret_val;
}

/* buffer space for UART input */
static uint8_t input_char;
static void uart_rx_handler(int32_t bytes)
{
    /* echo input char(s) back to UART */
    UART_Write(UART_PORT, &input_char, bytes);
    
    return;
}
 
#ifndef TOP_MAIN
int main(void)
{
    /* enable instruction cache */
    ICC_Enable();

    /* use the internal Ring Osc in 24MHz mode */
    CLKMAN_SetSystemClock(MXC_E_CLKMAN_SYSTEM_SOURCE_SELECT_24MHZ_RO);

    /* set systick to the RTC input 32.768KHz clock, not system clock; this is needed to keep JTAG alive */
    CLKMAN_SetRTOSMode(TRUE);

    /* trim the internal clock to get more accurate BAUD rate */
    CLKMAN_TrimRO_Start();
    SysTick_Wait(1635); /* about 50ms, based on a 32KHz systick clock */
    CLKMAN_TrimRO_Stop();

    /* setup GPIO for the LED */
    GPIO_SetOutMode(GREEN_LED_PORT, GREEN_LED_PIN, MXC_E_GPIO_OUT_MODE_OPEN_DRAIN_W_PULLUP);
    GPIO_SetOutVal(GREEN_LED_PORT, GREEN_LED_PIN, LED_ON);

    /* setup UART0 pins, mapping as EvKit to onboard FTDI UART to USB */
    IOMAN_UART0(MXC_E_IOMAN_MAPPING_A, TRUE, FALSE, FALSE);

    /* setup UART */
    UART_Config(UART_PORT, BAUD_RATE, FALSE, FALSE, FALSE);

    /* give libc a buffer, this way libc will not malloc() a stdio buffer */
    setvbuf(stdout, uart_stdout_buf0, _IOLBF, (size_t)sizeof(uart_stdout_buf0));
    setvbuf(stdin, NULL, _IONBF, 0);

    /* set input handler */
    UART_ReadAsync(UART_PORT, &input_char, sizeof(input_char), uart_rx_handler);

    /* print out welcome message */
    printf("\n\nMaxim Integrated MAX32600\n");
    printf("UARTDemo with libc printf\n\n");
    fflush(stdout);

    for(;;) {
        /* default sleep mode is "LP2"; core powered up, ARM in "Wait For Interrupt" mode */
        PWR_Sleep();
    }
    return 0;
}
#endif /* TOP_MAIN */
