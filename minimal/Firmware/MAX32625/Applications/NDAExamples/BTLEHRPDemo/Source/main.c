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
 * $Date: 2016-06-14 15:55:45 -0500 (Tue, 14 Jun 2016) $
 * $Revision: 23329 $
 *
 ******************************************************************************/
/**
 * @file    main.c
 * @brief   BTLE Heart Rate Sensor Demo.
 * @details This example emulates a heart rate sensor. The EM9301 is used to 
 *          facilitate the Bluetooth Low Energy communication. 
 * @note    Due to a PCB error, you must jumper P4.0 to P4.1 for the example 
 *          to work properly.
 */

#include <stdio.h>
#include "mxc_config.h"
#include "board.h"
#include "tmr.h"
#include "lp.h"
#include "pb.h"
#include "tmr_utils.h"

#include "wsf_types.h"
#include "wsf_os.h"
#include "wsf_buf.h"
#include "wsf_sec.h"
#include "hci_handler.h"
#include "dm_handler.h"
#include "l2c_handler.h"
#include "att_handler.h"
#include "smp_handler.h"
#include "app_api.h"
#include "l2c_api.h"
#include "smp_api.h"
#include "fit_api.h"
#include "hci_drv.h"
#include "app_ui.h"

#define BLE_MS_PER_TIMER_TICK      100  /* milliseconds per WSF timer tick */

#define WSF_BUF_POOLS              4    /* Number of WSF buffer pools */

/*! Free memory for pool buffers. */
static uint8_t mainBufMem[768];

/*! Default pool descriptor. */
static wsfBufPoolDesc_t mainPoolDesc[WSF_BUF_POOLS] = {
    {  16,  8 },
    {  32,  4 },
    {  64,  2 },
    { 128,  2 }
};

/* Initialize WSF(Wicentric Software Foundation) and BLE stack */
static void bleStackInit(uint8_t msPerTick)
{
    wsfHandlerId_t handlerId;

    /* init OS subsystems */
    WsfTimerInit(msPerTick);

    WsfBufInit(sizeof(mainBufMem), mainBufMem, WSF_BUF_POOLS, &mainPoolDesc[0]);
    WsfSecInit(); // security service

    /* initialize HCI */
    handlerId = WsfOsSetNextHandler(HciHandler);
    HciHandlerInit(handlerId);

    /* initialize DM */
    handlerId = WsfOsSetNextHandler(DmHandler);
    DmAdvInit();
    DmConnInit();
    DmConnSlaveInit();
    DmSecInit();
    DmHandlerInit(handlerId);

    /* initialize L2C */
    handlerId = WsfOsSetNextHandler(L2cSlaveHandler);
    L2cSlaveHandlerInit(handlerId);
    L2cInit();
    L2cSlaveInit();

    /* Initialize ATT */
    handlerId = WsfOsSetNextHandler(AttHandler);
    AttHandlerInit(handlerId);
    AttsInit();
    AttsIndInit();
    AttcInit();

    handlerId = WsfOsSetNextHandler(SmpHandler);
    SmpHandlerInit(handlerId);
    SmprInit();

    handlerId = WsfOsSetNextHandler(AppHandler);
    AppHandlerInit(handlerId);

    handlerId = WsfOsSetNextHandler(FitHandler);
    FitHandlerInit(handlerId);

    hciDrvInit();
}

void TMR0_IRQHandler(void)
{
    static uint32_t counter = 0;

    TMR32_ClearFlag(MXC_TMR0);

    /* Update our BLE stack timers */
    WsfTimerUpdate(1);

    /* Blink LED every one second */
    if (counter++ == (500 / BLE_MS_PER_TIMER_TICK)) {
        LED_Toggle(2);
        counter = 0;
    }
}

void sw1_callback()
{
    if(TMR_TO_Check(MXC_TMR3) == E_TIME_OUT) {
        AppUiBtnTest(APP_UI_BTN_1_SHORT);
        printf("fitHeartRate = %d\n", fitHeartRate);
        TMR_TO_Start(MXC_TMR3, MSEC(200));    
    }
}

void sw2_callback()
{
    if(TMR_TO_Check(MXC_TMR3) == E_TIME_OUT) {
        AppUiBtnTest(APP_UI_BTN_2_SHORT);
        printf("fitHeartRate = %d\n", fitHeartRate);
        TMR_TO_Start(MXC_TMR3, MSEC(200));    
    }
}
    

int main(void)
{
    CLKMAN_TrimRO();

    /* display welcome message */
    printf("\n\nMaxim Integrated MAX32625 EvKit\n");
    printf("Bluetooth LE Heart Rate Sensor Demo\n");

    /* initialize Bluetooth Low Energy stack */
    bleStackInit(BLE_MS_PER_TIMER_TICK);

    /* Configure advertising and services */
    FitStart();

    /* Enable periodic timer */
    TMR_Init(MXC_TMR0, TMR_PRESCALE_DIV_2_12, NULL);
    tmr32_cfg_t tmr_cfg;
    tmr_cfg.mode = TMR32_MODE_CONTINUOUS;
    tmr_cfg.polarity = TMR_POLARITY_UNUSED;
    TMR32_TimeToTicks(MXC_TMR0, BLE_MS_PER_TIMER_TICK, TMR_UNIT_MILLISEC, &tmr_cfg.compareCount);
    TMR32_Config(MXC_TMR0, &tmr_cfg);
    TMR32_EnableINT(MXC_TMR0);
    NVIC_SetPriority(TMR0_0_IRQn, 6);
    NVIC_EnableIRQ(TMR0_0_IRQn);
    TMR32_Start(MXC_TMR0);

    /* Enable SW callbacks */
    TMR_TO_Start(MXC_TMR3, MSEC(10));
    PB_RegisterCallback(SW1, sw1_callback);
    PB_RegisterCallback(SW2, sw2_callback);

    while (1) {
        wsfOsDispatcher();

        LP_EnterLP2();
    }

    return 0;
}
