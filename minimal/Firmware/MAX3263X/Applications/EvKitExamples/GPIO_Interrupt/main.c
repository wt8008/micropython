/*******************************************************************************
 * Copyright (C) 2016 Maxim Integrated Products, Inc., All Rights Reserved.
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
 * $Date: 2016-08-31 16:37:32 -0500 (Wed, 31 Aug 2016) $
 * $Revision: 24219 $
 *
 ******************************************************************************/

/**
 * @file    main.c
 * @brief   GPIO callback demo
 * @details When GPIO Port 7 Pin 1 transitions from high to low, the LED transitions between Red and Green
 */

/***** Includes *****/
#include <stdio.h>
#include <stdint.h>
#include "mxc_config.h"
#include "gpio.h"
#include "led.h"
#include "nhd12832.h"

/***** Definitions *****/

#define FMTSTR_LEN 40

/***** Globals *****/

const gpio_cfg_t pin = { PORT_7, PIN_1, GPIO_FUNC_GPIO, GPIO_PAD_INPUT_PULLUP };

/***** Functions *****/

/* This function is called when the interrupt fires on the GPIO pin
 *
 * Remember to de-bounce the input to this pin, or the LEDs will toggle
 *  too fast to observe!
 *
 */
void pin_cb(void *cbdata)
{
  static unsigned int state = 0;

  if (state) {
    LED_On(0);
    LED_Off(1);
  } else {
    LED_Off(0);
    LED_On(1);
  }
  state ^= 0x01;
}


// *****************************************************************************
int main(void)
{
    char fmtstr[FMTSTR_LEN];
  
    /* OLED message */
    NHD12832_Init();
    NHD12832_ShowString((uint8_t*)"GPIO Callback Demo", 0, 4);
    snprintf(fmtstr, FMTSTR_LEN, "H->L edge on P%u.%u", pin.port, __builtin_ctz(pin.mask));
    NHD12832_ShowString((uint8_t*)fmtstr, 1, 4);
    NHD12832_ShowString((uint8_t*)" toggles LED0 & LED1", 2, 4);

    /* Initial state of LEDs */
    LED_On(0);
    LED_Off(1);

    /* Configure port pin and callback */
    GPIO_Config(&pin);
    GPIO_RegisterCallback(&pin, pin_cb, NULL);
    GPIO_IntConfig(&pin, GPIO_INT_FALLING_EDGE);
    GPIO_IntEnable(&pin);
    NVIC_EnableIRQ(MXC_GPIO_GET_IRQ(pin.port));
    
    /* Wait forever here, toggling LEDs on interrupts */
    while (1);
}
