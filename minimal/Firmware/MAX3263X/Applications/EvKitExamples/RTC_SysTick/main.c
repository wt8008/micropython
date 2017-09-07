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
 * $Date: 2016-06-21 10:41:06 -0500 (Tue, 21 Jun 2016) $
 * $Revision: 23420 $
 *
 ******************************************************************************/

/**
 * @file    main.c
 * @brief   32kHz RTC timer example.
 * @details The RTC with combined with the SysTick timer makes a 32kHz resolution timer
 *          that can keep an accurate count while in LP1.         
 */

/***** Includes *****/
#include <stdio.h>
#include "mxc_config.h"
#include "nvic_table.h"
#include "board.h"
#include "tmr_utils.h"
#include "rtc.h"
#include "lp.h"

/***** Definitions *****/
#define SYSTICK_EXTERNAL_CLOCK      0

/***** Globals *****/
volatile int go_to_lp1 = 0;
volatile int rtc_flag = 0;
uint32_t count_32kHz;
uint32_t systick_prev, systick_current;

/***** Functions *****/

// *****************************************************************************
void PB0_Sleep(void *pb)
{
  int count;

  // De-bounce input by repeated reads of the button
  count = 0;
  while (PB_Get(0) == 1) {
    if (++count > 10000) {
      go_to_lp1 = 1;
      break;
    }
  }

  // Button De-bounce
  TMR_Delay(MXC_TMR0, MSEC(200));
}

// *****************************************************************************
void UpdateCount(void)
{
    systick_current = SysTick->VAL;
    count_32kHz += systick_prev - systick_current;
    systick_prev = systick_current;
}

// *****************************************************************************
void SysTick_Handler(void)
{
    // Add the ticks prior to the rollover
    count_32kHz += systick_prev;
    systick_prev = SysTick_LOAD_RELOAD_Msk;

    UpdateCount();
}

// *****************************************************************************
void RTC_IRQHandler(void)
{
    // Enable SysTick
    SysTick->CTRL |= SysTick_CTRL_ENABLE_Msk;

    // Compensate the count_32kHz with the RTC count
    count_32kHz = count_32kHz + RTC_GetCount()*8;

    rtc_flag = 1;
    RTC_DisableINT(MXC_F_RTC_INTEN_COMP0);
    RTC_Stop();
}

// *****************************************************************************
int main(void)
{
    int error;

    printf("\n***** RTC_SysTick example *****\n");

    // Initialize count
    count_32kHz = 0;

    // Setup PB interrupt
    PB_RegisterCallback(SW1, PB0_Sleep);

    // Setup SysTick to use the 32kHz crystal
    systick_prev = SysTick_LOAD_RELOAD_Msk;
    error = SYS_SysTick_Config(SysTick_LOAD_RELOAD_Msk, SYSTICK_EXTERNAL_CLOCK);
    if(error != E_NO_ERROR) {
        printf("Error setting up SysTick\n");
        while(1) {}
    }

    // Setup the RTC
    rtc_cfg_t RTCconfig;
    RTCconfig.compareCount[0] = 0;
    RTCconfig.compareCount[1] = 0;
    RTCconfig.prescaler = RTC_PRESCALE_DIV_2_0;     //4kHz clock
    RTCconfig.prescalerMask = RTC_PRESCALE_DIV_2_0;
    RTCconfig.snoozeCount = 0;
    RTCconfig.snoozeMode = RTC_SNOOZE_DISABLE;
    RTC_Init(&RTCconfig);
    NVIC_SetVector(RTC0_IRQn, RTC_IRQHandler);

    printf("SysTick started and RTC initialized\n");
    printf("Press SW1 to enter/exit LP1\n");

    while(1) {

        // Enable button press to go to LP1 & set same pin for wake-up
        PB_IntClear(SW1);
        PB_IntEnable(SW1);
        LP_ClearWakeUpConfig();
        LP_ConfigGPIOWakeUpDetect(&pb_pin[0], 0, LP_WEAK_PULL_UP);
        go_to_lp1 = 0;
        LED_On(3);

        // Update count_32kHz and display the current value
        while(!go_to_lp1) {
            UpdateCount();
            printf("count_32kHz = 0x%x\n", count_32kHz);
            TMR_Delay(MXC_TMR0, MSEC(500));
        }

        LED_Off(3);

        // Prepare for LP1
        PB_IntDisable(SW1);
        LP_ClearWakeUpFlags();
        RTC_SetCount(0);
        RTC_Start();        

        // Disable the SysTick
        SysTick->CTRL &= ~(SysTick_CTRL_ENABLE_Msk);

        LP_EnterLP1();

        // Resume and sync the RTC with the counter timer. Both timers are effectively
        // using the 32k crystal, but we need to align with the RTC count. Add 2 to the
        // current count to give us at least 1 full period to synchronize.
        rtc_flag = 0;
        RTC_SetCompare(0, RTC_GetCount() + 2);
        RTC_EnableINT(MXC_F_RTC_INTEN_COMP0);
        while(rtc_flag == 0) {}
    }

    return 0;
}
