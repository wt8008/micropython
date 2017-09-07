/*******************************************************************************
 * Copyright (C) 2015 Maxim Integrated Products, Inc., All Rights Reserved.
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
 * $Id: usb_app.c 4438 2015-02-20 17:03:52Z jeremy.brodt $
 *
 *******************************************************************************
 */

#include <stddef.h>
#include "mxc_config.h"
#include "board.h"
#include "gpio.h"
#include "power.h"
#include "clkman.h"
#include "nvic_table.h"
#include "usb.h"
#include "usb_event.h"
#include "enumerate.h"
#include "hid_kbd.h"
#include "descriptors.h"

/***** Function Prototypes *****/
static int setconfig_callback(usb_setup_pkt *sud, void *cbdata);
static int event_callback(maxusb_event_t evt, void *data);
static void button_callback(void);

//******************************************************************************
int usb_app_init()
{
  int err;

  /* Setup the USB clocking */
  CLKMAN_USBClockEnable();
  mxc_clkman_clk_ctrl_t clkman_clk_ctrl = MXC_CLKMAN->clk_ctrl_f;
  clkman_clk_ctrl.usb_gate_n = 1;
  MXC_CLKMAN->clk_ctrl_f = clkman_clk_ctrl;

  /* Initialize the usb module */
  if ((err = usb_init(NULL)) != 0) {
    return err;
  }

  /* Initialize the enumeration module */
  if ((err = enum_init()) != 0) {
    return err;
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

  /* Initialize the class driver */
  if ((err = hid_init(&config_descriptor.hid_descriptor, report_descriptor)) != 0) {
    return err;
  }

  /* Register callbacks */
  usb_event_enable(MAXUSB_EVENT_NOVBUS, event_callback, NULL);
  usb_event_enable(MAXUSB_EVENT_VBUS, event_callback, NULL);
  usb_event_enable(MAXUSB_EVENT_BRST, event_callback, NULL);

  NVIC_SetVector(usb_event_handler, USB_IRQn);

  CLKMAN_SetClkScale(MXC_E_CLKMAN_CLK_GPIO, MXC_E_CLKMAN_CLK_SCALE_ENABLED);
  GPIO_SetInMode(SW1_PORT, SW1_PIN, MXC_E_GPIO_IN_MODE_NORMAL);
  GPIO_SetIntMode(SW1_PORT, SW1_PIN, MXC_E_GPIO_INT_MODE_FALLING_EDGE, button_callback);

  /* setup GPIO for the LED */
  GPIO_SetOutMode(YELLOW_LED_PORT, YELLOW_LED_PIN, MXC_E_GPIO_OUT_MODE_NORMAL);
  GPIO_SetOutVal(YELLOW_LED_PORT, YELLOW_LED_PIN, LED_OFF);

  return 0;
}

//******************************************************************************
static int setconfig_callback(usb_setup_pkt *sud, void *cbdata)
{
  /* Confirm the configuration value */
  if (sud->wValue == config_descriptor.config_descriptor.bConfigurationValue) {
    GPIO_SetOutVal(YELLOW_LED_PORT, YELLOW_LED_PIN, LED_ON);
    return hid_configure(config_descriptor.endpoint_descriptor.bEndpointAddress & USB_EP_NUM_MASK);
  } else if (sud->wValue == 0) {
    GPIO_SetOutVal(YELLOW_LED_PORT, YELLOW_LED_PIN, LED_OFF);
    return hid_deconfigure();
  }

  return -1;
}

//******************************************************************************
static void button_callback(void)
{
  static const uint8_t chars[] = "Maxim Integrated\n";
  static int i = 0;

  if (i >= (sizeof(chars) - 1)) {
    i = 0;
  }

  hid_keypress(chars[i++]);
}

//******************************************************************************
static int event_callback(maxusb_event_t evt, void *data)
{
  switch (evt) {
    case MAXUSB_EVENT_NOVBUS:
      GPIO_SetOutVal(YELLOW_LED_PORT, YELLOW_LED_PIN, LED_OFF);
      usb_disconnect();
      enum_clearconfig();
      hid_deconfigure();
      usb_event_enable(MAXUSB_EVENT_VBUS, event_callback, NULL);
      break;
    case MAXUSB_EVENT_VBUS:
      usb_connect();
      usb_event_disable(MAXUSB_EVENT_VBUS);
      break;
    case MAXUSB_EVENT_BRST:
      GPIO_SetOutVal(YELLOW_LED_PORT, YELLOW_LED_PIN, LED_OFF);
      enum_clearconfig();
      hid_deconfigure();
      break;
    default:
      break;
  }

  return 0;
}
