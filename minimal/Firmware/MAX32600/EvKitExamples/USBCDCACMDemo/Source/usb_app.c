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
 * $Id: usb_app.c 4433 2015-02-20 16:48:50Z jeremy.brodt $
 *
 *******************************************************************************
 */

#include <stddef.h>
#include "mxc_config.h"
#include "board.h"
#include "uart.h"
#include "power.h"
#include "clkman.h"
#include "ioman.h"
#include "gpio.h"
#include "nvic_table.h"
#include "usb.h"
#include "usb_event.h"
#include "enumerate.h"
#include "cdc_acm.h"
#include "descriptors.h"

/***** Definitions *****/
#define UART_PORT   0


/***** File Scope Variables *****/

/* This EP assignment must match the Configuration Descriptor */
static const acm_cfg_t acm_cfg = {
  1,  /* EP OUT */
  2,  /* EP IN */
  3   /* EP Notify */
};

static uint8_t uart_rx_data[MXC_CFG_UART_FIFO_DEPTH + 1];
static uint8_t uart_tx_data[MXC_CFG_UART_FIFO_DEPTH];


/***** Function Prototypes *****/
static int setconfig_callback(usb_setup_pkt *sud, void *cbdata);
static int event_callback(maxusb_event_t evt, void *data);
static int configure_uart(void);
static int read_from_usb(void);
static void read_from_uart(int32_t);


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

  /* Handle configuration */
  enum_register_callback(ENUM_SETCONFIG, setconfig_callback, NULL);

  /* Initialize the class driver */
  if ((err = acm_init()) != 0) {
    return err;
  }

  /* Register callbacks */
  usb_event_enable(MAXUSB_EVENT_NOVBUS, event_callback, NULL);
  usb_event_enable(MAXUSB_EVENT_VBUS, event_callback, NULL);
  usb_event_enable(MAXUSB_EVENT_BRST, event_callback, NULL);
  acm_register_callback(ACM_CB_SET_LINE_CODING, configure_uart);
  acm_register_callback(ACM_CB_READ_READY, read_from_usb);

  NVIC_SetVector(usb_event_handler, USB_IRQn);

  /* setup UART */
  IOMAN_UART0(MXC_E_IOMAN_MAPPING_A, TRUE, FALSE, FALSE);
  if ((err = configure_uart()) != 0) {
    return err;
  }

  /* setup GPIO for the LED */
  GPIO_SetOutMode(YELLOW_LED_PORT, YELLOW_LED_PIN, MXC_E_GPIO_OUT_MODE_NORMAL);
  GPIO_SetOutVal(YELLOW_LED_PORT, YELLOW_LED_PIN, LED_OFF);

  return 0;
}

//******************************************************************************
static int configure_uart(void)
{
  int err;
  const acm_line_t *params;
  uint8_t parity_enable, parity_mode;

  /* get the current line coding parameters */
  params = acm_line_coding();

  /* some settings are fixed for this implementation */
  if ((params->stopbits != ACM_STOP_1) || (params->databits != 8)) {
    return -1;
  }

  if (params->parity == ACM_PARITY_NONE) {
    parity_enable = 0;
    parity_mode = 0;
  } else if (params->parity == ACM_PARITY_ODD) {
    parity_enable = 1;
    parity_mode = 0;
  } else if (params->parity == ACM_PARITY_EVEN) {
    parity_enable = 1;
    parity_mode = 1;
  } else {
    return -1;
  }

  if ((err = UART_Config(UART_PORT, params->speed, parity_enable, parity_mode, 0)) != 0) {
    return err;
  }

  /* submit the initial read request */
  UART_ReadAsync(UART_PORT, uart_rx_data, 1, read_from_uart);

  return 0;
}

//******************************************************************************
static int setconfig_callback(usb_setup_pkt *sud, void *cbdata)
{
  /* Confirm the configuration value */
  if (sud->wValue == config_descriptor.config_descriptor.bConfigurationValue) {
    GPIO_SetOutVal(YELLOW_LED_PORT, YELLOW_LED_PIN, LED_ON);
    return acm_configure(&acm_cfg); /* Configure the device class */
  } else if (sud->wValue == 0) {
    GPIO_SetOutVal(YELLOW_LED_PORT, YELLOW_LED_PIN, LED_OFF);
    return acm_deconfigure();
  }

  return -1;
}

//******************************************************************************
static int event_callback(maxusb_event_t evt, void *data)
{
  switch (evt) {
    case MAXUSB_EVENT_NOVBUS:
      GPIO_SetOutVal(YELLOW_LED_PORT, YELLOW_LED_PIN, LED_OFF);
      usb_disconnect();
      enum_clearconfig();
      acm_deconfigure();
      usb_event_enable(MAXUSB_EVENT_VBUS, event_callback, NULL);
      break;
    case MAXUSB_EVENT_VBUS:
      usb_connect();
      usb_event_disable(MAXUSB_EVENT_VBUS);
      break;
    case MAXUSB_EVENT_BRST:
      GPIO_SetOutVal(YELLOW_LED_PORT, YELLOW_LED_PIN, LED_OFF);
      enum_clearconfig();
      acm_deconfigure();
      break;
    default:
      break;
  }

  return 0;
}

//******************************************************************************
static int read_from_usb(void)
{
  int chars;

  while ((chars = acm_canread()) > 0) {

    if (chars > sizeof(uart_tx_data)) {
      chars = sizeof(uart_tx_data);
    }

    if (acm_read(uart_tx_data, chars) != chars) {
      break;
    } else {
      UART_Write(UART_PORT, uart_tx_data, chars);
    }
  }

  return 0;
}

//******************************************************************************
static void read_from_uart(int32_t rx_bytes)
{
  int bytes;

  // read additional characters that may have been received
  if ((bytes = UART_Poll(UART_PORT)) > 0) {
    rx_bytes += UART_Read(UART_PORT, &uart_rx_data[rx_bytes], bytes);
  }

  if (acm_present()) {
    acm_write(uart_rx_data, rx_bytes);
  }
}
