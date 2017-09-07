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
 * $Id: main.c 21896 2016-03-15 18:53:44Z kevin.gillespie $
 *
 *******************************************************************************
 */

/**
 * @file main.c
 * @brief Demonstrates how to configure a the USB device controller as a HID keyboard class device
 * @details The EvKit should enumerate as a HID Keyboard device.
 *          1. LED0 will illuminate once enumeration and configuration is complete.
 *          2. Open a text editor on the PC host and place cursor in edit box.
 *          3. Pressing pushbutton SW1 will cause a message to be typed in on a virtual keyboard one character at a time.
 *
 */

#include <stdio.h>
#include <stddef.h>
#include "mxc_config.h"
#include "mxc_sys.h"
#include "pwrman_regs.h"
#include "board.h"
#include "lp.h"
#include "led.h"
#include "pb.h"
#include "tmr.h"
#include "usb.h"
#include "usb_event.h"
#include "enumerate.h"
#include "hid_kbd.h"
#include "descriptors.h"

/***** Definitions *****/
#define EVENT_ENUM_COMP     MAXUSB_NUM_EVENTS
#define EVENT_REMOTE_WAKE   (EVENT_ENUM_COMP + 1)

#define WORKAROUND_TIMER    0           // Timer used for USB wakeup workaround
#define TMRn_IRQHandler     TMR0_IRQHandler
#define MXC_TMRn            MXC_TMR_GET_TMR(WORKAROUND_TIMER)
#define TMRn_IRQn           MXC_TMR_GET_IRQ_32(WORKAROUND_TIMER)

/***** Global Data *****/
volatile int configured;
volatile int suspended;
volatile unsigned int event_flags;
int remote_wake_en;

/***** Function Prototypes *****/
static int setconfig_callback(usb_setup_pkt *sud, void *cbdata);
static int setfeature_callback(usb_setup_pkt *sud, void *cbdata);
static int clrfeature_callback(usb_setup_pkt *sud, void *cbdata);
static int event_callback(maxusb_event_t evt, void *data);
static void usb_app_sleep(void);
static void usb_app_wakeup(void);
static void button_callback(void *pb);

/******************************************************************************/
int main(void)
{
    printf("\n\n***** MAX3263x USB HID Keyboard Example *****\n");
    printf("Waiting for VBUS...\n");

    /* Initialize state */
    configured = 0;
    suspended = 0;
    event_flags = 0;
    remote_wake_en = 0;

    /* Enable the USB clock and power */
    SYS_USB_Enable(1);

    /* Initialize the usb module */
    if (usb_init(NULL) != 0) {
        printf("usb_init() failed\n");
        while (1);
    }

    /* Initialize the enumeration module */
    if (enum_init() != 0) {
        printf("enum_init() failed\n");
        while (1);
    }

    /* Register enumeration data */
    enum_register_descriptor(ENUM_DESC_DEVICE, (uint8_t*)&device_descriptor, 0);
    enum_register_descriptor(ENUM_DESC_CONFIG, (uint8_t*)&config_descriptor, 0);
    enum_register_descriptor(ENUM_DESC_STRING, lang_id_desc, 0);
    enum_register_descriptor(ENUM_DESC_STRING, mfg_id_desc, 1);
    enum_register_descriptor(ENUM_DESC_STRING, prod_id_desc, 2);
    enum_register_descriptor(ENUM_DESC_STRING, serial_id_desc, 3);

    /* Handle configuration */
    enum_register_callback(ENUM_SETCONFIG, setconfig_callback, NULL);

    /* Handle feature set/clear */
    enum_register_callback(ENUM_SETFEATURE, setfeature_callback, NULL);
    enum_register_callback(ENUM_CLRFEATURE, clrfeature_callback, NULL);

    /* Initialize the class driver */
    if (hidkbd_init(&config_descriptor.hid_descriptor, report_descriptor) != 0) {
        printf("hidkbd_init() failed\n");
        while (1);
    }

    /* Register callbacks */
    usb_event_enable(MAXUSB_EVENT_NOVBUS, event_callback, NULL);
    usb_event_enable(MAXUSB_EVENT_VBUS, event_callback, NULL);

    /* Register callback for keyboard events */
    if (PB_RegisterCallback(0, button_callback) != E_NO_ERROR) {
        printf("PB_RegisterCallback() failed\n");
        while (1);
    }

    /* Start with USB in low power mode */
    usb_app_sleep();
    NVIC_EnableIRQ(USB_IRQn);

    /* Wait for events */
    while (1) {

        if (suspended || !configured) {
            LED_Off(0);
        } else {
            LED_On(0);
        }

        if (event_flags) {
            /* Display events */
            if (MXC_GETBIT(&event_flags, MAXUSB_EVENT_NOVBUS)) {
                MXC_CLRBIT(&event_flags, MAXUSB_EVENT_NOVBUS);
                printf("VBUS Disconnect\n");
            } else if (MXC_GETBIT(&event_flags, MAXUSB_EVENT_VBUS)) {
                MXC_CLRBIT(&event_flags, MAXUSB_EVENT_VBUS);
                printf("VBUS Connect\n");
            } else if (MXC_GETBIT(&event_flags, MAXUSB_EVENT_BRST)) {
                MXC_CLRBIT(&event_flags, MAXUSB_EVENT_BRST);
                printf("Bus Reset\n");
            } else if (MXC_GETBIT(&event_flags, MAXUSB_EVENT_SUSP)) {
                MXC_CLRBIT(&event_flags, MAXUSB_EVENT_SUSP);
                printf("Suspended\n");
            } else if (MXC_GETBIT(&event_flags, MAXUSB_EVENT_DPACT)) {
                MXC_CLRBIT(&event_flags, MAXUSB_EVENT_DPACT);
                printf("Resume\n");
            } else if (MXC_GETBIT(&event_flags, EVENT_ENUM_COMP)) {
                MXC_CLRBIT(&event_flags, EVENT_ENUM_COMP);
                printf("Enumeration complete. Press SW1 to send character.\n");
            } else if (MXC_GETBIT(&event_flags, EVENT_REMOTE_WAKE)) {
                MXC_CLRBIT(&event_flags, EVENT_REMOTE_WAKE);
                printf("Remote Wakeup\n");
            }
        } else {
            LP_EnterLP2();
        }
    }
}

/******************************************************************************/
static int setconfig_callback(usb_setup_pkt *sud, void *cbdata)
{
    /* Confirm the configuration value */
    if (sud->wValue == config_descriptor.config_descriptor.bConfigurationValue) {
        configured = 1;
        MXC_SETBIT(&event_flags, EVENT_ENUM_COMP);
        return hidkbd_configure(config_descriptor.endpoint_descriptor.bEndpointAddress & USB_EP_NUM_MASK);
    } else if (sud->wValue == 0) {
        configured = 0;
        return hidkbd_deconfigure();
    }

    return -1;
}

/******************************************************************************/
static int setfeature_callback(usb_setup_pkt *sud, void *cbdata)
{
    if(sud->wValue == FEAT_REMOTE_WAKE) {
        remote_wake_en = 1;
    } else {
        // Unknown callback
        return -1;
    }

    return 0;
}

/******************************************************************************/
static int clrfeature_callback(usb_setup_pkt *sud, void *cbdata)
{
    if(sud->wValue == FEAT_REMOTE_WAKE) {
        remote_wake_en = 0;
    } else {
        // Unknown callback
        return -1;
    }
    
    return 0;
}

/******************************************************************************/
static void usb_app_sleep(void)
{
    usb_sleep();
    MXC_PWRMAN->pwr_rst_ctrl &= ~MXC_F_PWRMAN_PWR_RST_CTRL_USB_POWERED;
    if (MXC_USB->dev_cn & MXC_F_USB_DEV_CN_CONNECT) {
        usb_event_clear(MAXUSB_EVENT_DPACT);
        usb_event_enable(MAXUSB_EVENT_DPACT, event_callback, NULL);
    } else {
        usb_event_disable(MAXUSB_EVENT_DPACT);
    }
    suspended = 1;
}

/******************************************************************************/
static void usb_app_wakeup(void)
{
    usb_event_disable(MAXUSB_EVENT_DPACT);
    MXC_PWRMAN->pwr_rst_ctrl |= MXC_F_PWRMAN_PWR_RST_CTRL_USB_POWERED;
    usb_wakeup();
    suspended = 0;
}

/******************************************************************************/
static int event_callback(maxusb_event_t evt, void *data)
{
    /* Set event flag */
    MXC_SETBIT(&event_flags, evt);

    switch (evt) {
        case MAXUSB_EVENT_NOVBUS:
            usb_event_disable(MAXUSB_EVENT_BRST);
            usb_event_disable(MAXUSB_EVENT_SUSP);
            usb_event_disable(MAXUSB_EVENT_DPACT);
            usb_disconnect();
            configured = 0;
            enum_clearconfig();
            hidkbd_deconfigure();
            usb_app_sleep();
            break;
        case MAXUSB_EVENT_VBUS:
            usb_event_clear(MAXUSB_EVENT_BRST);
            usb_event_enable(MAXUSB_EVENT_BRST, event_callback, NULL);
            usb_event_clear(MAXUSB_EVENT_SUSP);
            usb_event_enable(MAXUSB_EVENT_SUSP, event_callback, NULL);
            usb_connect();
            usb_app_sleep();
            break;
        case MAXUSB_EVENT_BRST:
            TMR32_Stop(MXC_TMRn);   /* No need for workaround if bus reset */
            usb_app_wakeup();
            enum_clearconfig();
            hidkbd_deconfigure();
            configured = 0;
            suspended = 0;
            break;
        case MAXUSB_EVENT_SUSP:
            usb_app_sleep();
            break;
        case MAXUSB_EVENT_DPACT:
            usb_app_wakeup();
            /* Workaround to reset internal suspend timer flag. Wait to determine if this is resume signaling or a bus reset. */
            TMR_Init(MXC_TMRn, TMR_PRESCALE_DIV_2_5, NULL);
            tmr32_cfg_t tmr_cfg;
            tmr_cfg.mode = TMR32_MODE_ONE_SHOT;
            tmr_cfg.polarity = TMR_POLARITY_UNUSED;
            TMR32_TimeToTicks(MXC_TMRn, 30, TMR_UNIT_MICROSEC, &tmr_cfg.compareCount);
            TMR32_Config(MXC_TMRn, &tmr_cfg);
            TMR32_EnableINT(MXC_TMRn);
            NVIC_EnableIRQ(TMRn_IRQn);
            TMR32_Start(MXC_TMRn);
            MXC_CLRBIT(&event_flags, MAXUSB_EVENT_DPACT);   /* delay until we know this is resume signaling */
            break;
        default:
            break;
    }

    return 0;
}

/******************************************************************************/
void button_callback(void *pb)
{
    static const uint8_t chars[] = "Maxim Integrated\n";
    static int i = 0;
    int count = 0;
    int button_pressed = 0;

    //determine if interrupt triggered by bounce or a true button press
    while (PB_Get(0) && !button_pressed)
    {
        count++;

        if(count > 1000)
            button_pressed = 1;
    }

    if(button_pressed) {
        LED_Toggle(1);
        if (configured) {
            if (suspended && remote_wake_en) {
                /* The bus is suspended. Wake up the host */
                suspended = 0;
                usb_app_wakeup();
                usb_remote_wakeup();
                MXC_SETBIT(&event_flags, EVENT_REMOTE_WAKE);
            } else {
                if (i >= (sizeof(chars) - 1)) {
                    i = 0;
                }

                hidkbd_keypress(chars[i++]);
            }
        }
    }
}

/******************************************************************************/
void USB_IRQHandler(void)
{
    usb_event_handler();
}

/******************************************************************************/
void TMRn_IRQHandler(void)
{
    TMR32_Stop(MXC_TMRn);
    TMR32_ClearFlag(MXC_TMRn);
    NVIC_DisableIRQ(TMRn_IRQn);

    /* No Bus Reset occurred. We woke due to resume signaling from the host.
     * Workaround to reset internal suspend timer flag. The host will hold
     * the resume signaling for 20ms. This remote wakeup signaling will
     * complete before the host sets the bus to idle.
     */
    usb_remote_wakeup();
    MXC_SETBIT(&event_flags, MAXUSB_EVENT_DPACT);
}
