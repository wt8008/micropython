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
 * Description: Communications Device Class ACM (Serial Port) over USB
 * $Id: cdc_acm.c 16484 2015-02-02 22:18:38Z jeremy.brodt $
 *
 *******************************************************************************
 */

#include <stdint.h>
#include <string.h>
#include <stdarg.h>
#include "usb.h"
#include "usb_event.h"
#include "enumerate.h"
#include "cdc_acm.h"
#include "fifo.h"

/***** Definitions *****/
#define EPBUF_SIZE  64
#define IBUF_SIZE   64
#define OBUF_SIZE   64

// USB CDC ACM class requests
#define ACM_SET_LINE_CODING         0x20
#define ACM_GET_LINE_CODING         0x21
#define ACM_SET_CONTROL_LINE_STATE  0x22
#define ACM_SEND_BREAK              0x23

// Control Line State bits
#define CLS_DTR   (1 << 0)
#define CLS_RTS   (1 << 1)

/***** File Scope Data *****/
static volatile int DTE_present = 0;
static volatile int BREAK_signal = 0;

// Endpoint numbers
static uint8_t out_ep;
static uint8_t in_ep;
static uint8_t notify_ep;

// Line Coding
static usb_req_t creq;
__attribute__((aligned(4)))
static acm_line_t line_coding = {
  0x00002580,   // 9600 bps
  0,
  0,
  8
};

// Write (IN) data
static usb_req_t wreq;
__attribute__((aligned(4)))
static uint8_t wbuf[IBUF_SIZE];
static fifo_t wfifo;

// Read (OUT) data
static usb_req_t rreq;
__attribute__((aligned(4)))
static uint8_t rbuf[OBUF_SIZE];
static fifo_t rfifo;

// Notification
static usb_req_t nreq;
__attribute__((aligned(4)))
static uint8_t notify_data[] = {
  0xa1,           /* bmRequestType = Notification */
  0x20,           /* bNotification = SERIAL_STATE */
  0x00, 0x00,     /* wValue = 0 */
  0x00, 0x00,     /* wIndex = 0 */
  0x02, 0x00,     /* wLength = 2 */
  0x02, 0x00      /* DSR active */
};

static int (*callback[ACM_NUM_CALLBACKS])(void);

/***** Function Prototypes *****/
static int class_req(usb_setup_pkt *sud, void *cbdata);
static void svc_out_from_host(void *cbdata);
static void svc_in_to_host(void *cbdata);
static int set_line_coding(void);
static int get_line_coding(void);

/******************************************************************************/
int acm_init(void)
{
  out_ep = 0;
  in_ep = 0;
  notify_ep = 0;
  memset(callback, 0, sizeof(callback));

  /* Handle class-specific SETUP requests */
  enum_register_callback(ENUM_CLASS_REQ, class_req, NULL);

  return 0;
}

/******************************************************************************/
void acm_register_callback(acm_callback_t cbnum, int (*func)(void))
{
  if (cbnum <= ACM_NUM_CALLBACKS) {
    callback[cbnum] = func;
  }
}

/******************************************************************************/
int acm_configure(const acm_cfg_t *cfg)
{
  int err;

  DTE_present = 0;
  BREAK_signal = 0;

  out_ep = cfg->out_ep;
  if ((err = usb_config_ep(out_ep, MAXUSB_EP_TYPE_OUT, EPBUF_SIZE)) != 0) {
    acm_deconfigure();
    return err;
  }

  in_ep = cfg->in_ep;
  if ((err = usb_config_ep(in_ep, MAXUSB_EP_TYPE_IN, EPBUF_SIZE)) != 0) {
    acm_deconfigure();
    return err;
  }

  notify_ep = cfg->notify_ep;
  if ((err = usb_config_ep(notify_ep, MAXUSB_EP_TYPE_IN, EPBUF_SIZE)) != 0) {
    acm_deconfigure();
    return err;
  }

  fifo_init(&wfifo, wbuf, IBUF_SIZE);
  memset(&wreq, 0, sizeof(usb_req_t));
  wreq.ep = in_ep;
  wreq.data = wbuf;
  wreq.callback = svc_in_to_host;
  wreq.cbdata = &wreq;

  fifo_init(&rfifo, rbuf, OBUF_SIZE);
  memset(&rreq, 0, sizeof(usb_req_t));
  rreq.ep = out_ep;
  rreq.data = rbuf;
  rreq.reqlen = OBUF_SIZE;
  rreq.callback = svc_out_from_host;
  rreq.cbdata = &rreq;

  memset(&nreq, 0, sizeof(usb_req_t));
  nreq.ep = notify_ep;
  nreq.data = (uint8_t*)notify_data;
  nreq.reqlen = sizeof(notify_data);
  nreq.callback = NULL;
  nreq.cbdata = NULL;

  return 0;
}

/******************************************************************************/
int acm_deconfigure(void)
{
  /* deconfigure EPs */
  if (out_ep != 0) {
    usb_reset_ep(out_ep);
    out_ep = 0;
  }

  if (in_ep != 0) {
    usb_reset_ep(in_ep);
    in_ep = 0;
  }

  if (notify_ep != 0) {
    usb_reset_ep(notify_ep);
    notify_ep = 0;
  }

  /* clear driver state */
  fifo_clear(&wfifo);
  fifo_clear(&rfifo);
  DTE_present = 0;
  BREAK_signal = 0;

  return 0;
}

/******************************************************************************/
int acm_present(void)
{
  return DTE_present;
}

/******************************************************************************/
const acm_line_t *acm_line_coding(void)
{
  return &line_coding;
}

/******************************************************************************/
int acm_canread(void)
{
  return fifo_level(&rfifo);
}

/******************************************************************************/
int acm_read(uint8_t *buf, unsigned int len)
{
  unsigned int i;
  uint8_t byte;

  for (i = 0; i < len; i++) {
    while (fifo_get8(&rfifo, &byte) != 0) {
      usb_event_handler();

      /* Check for Break in loop */
      if (BREAK_signal) {
        return -2;
      }

      if (!DTE_present) {
        /* Disconnected during a read, return EOF (0) */
        return 0;
      }
    }

    buf[i] = byte;
  }

  return i;
}

/******************************************************************************/
int acm_write(uint8_t *buf, unsigned int len)
{
  unsigned int i = 0;

  // Write data into the FIFO
  for (i = 0; i < len; i++) {
    if (fifo_put8(&wfifo, buf[i]) != 0) {
      break;
    }
  }

  // If there is not active request, start one
  if (wreq.reqlen == 0) {
    svc_in_to_host(&wreq);
  }

  // Write remaining data to FIFO
  while (i < len) {
    usb_event_handler();
    if (fifo_put8(&wfifo, buf[i]) == 0) {
      i++;
    }
  }

  return i;
}

/******************************************************************************/
static void svc_out_from_host(void *cbdata)
{
  unsigned int i, readlen;
  usb_req_t *req = (usb_req_t*)cbdata;

  // Copy as much data into the local buffer as possible
  for (i = 0; i < req->actlen; i++) {
    if (fifo_put8(&rfifo, req->data[i]) != 0) {
      break;
    }
  }
  readlen = req->actlen;
  req->actlen = 0;

  // An error will occur when the host has been disconnected.
  // Register the next request if still configured.
  if ((req->error_code == 0) && (out_ep > 0)) {
    usb_read_endpoint(req);
  }
  req->error_code = 0;

  // Call the callback if there is new data
  if ((readlen > 0) && (callback[ACM_CB_READ_READY] != NULL)) {
    callback[ACM_CB_READ_READY]();
  }
}

/******************************************************************************/
static void svc_in_to_host(void *cbdata)
{
  int i;
  usb_req_t *req = (usb_req_t*)cbdata;
  uint8_t byte;

  // An error will occur when the host has been disconnected.
  // Register the next request if still configured and there is data to send
  if ((req->error_code == 0) && (in_ep > 0) && !fifo_empty(&wfifo)) {
    for (i = 0; i < IBUF_SIZE; i++) {
      if (fifo_get8(&wfifo, &byte) != 0) {
        break;
      }
      req->data[i] = byte;
    }

    req->reqlen = i;
    req->actlen = 0;

    // Register the next request
    usb_write_endpoint(req);
  } else {
    // Clear the request length to indicate that there is not an active request
    req->reqlen = 0;
    req->error_code = 0;
  }
}

/******************************************************************************/
static int class_req(usb_setup_pkt *sud, void *cbdata)
{
  int result = -1;

  if ( ((sud->bmRequestType & RT_RECIP_MASK) == RT_RECIP_IFACE) && (sud->wIndex == 0) ) {
    switch (sud->bRequest) {
      case ACM_SET_LINE_CODING:
        if ((result = set_line_coding()) == 0) {
          /* There is a data stage. Return immediately and postpone ack/stall. */
          return 0;
        }
        break;
      case ACM_GET_LINE_CODING:
        result = get_line_coding();
        break;
      case ACM_SET_CONTROL_LINE_STATE:
        if (sud->wValue & CLS_DTR) {
          /* DTE is now present, enable initial notification */
          usb_write_endpoint(&nreq);  // prepare the serial state notification
          usb_read_endpoint(&rreq);   // register an initial OUT request
          DTE_present = 1;
          if (callback[ACM_CB_CONNECTED]) {
            callback[ACM_CB_CONNECTED]();
          }
        } else {
          /* DTE disappeared */
          DTE_present = 0;
          if (callback[ACM_CB_DISCONNECTED]) {
            callback[ACM_CB_DISCONNECTED]();
          }
        }
        result = 0;
        break;
      case ACM_SEND_BREAK:
        if (sud->wValue > 0) {
          BREAK_signal = 1;
        } else {
          BREAK_signal = 0;
        }
        result = 0;
        break;
      default:
        result = -1;
        break;
    }
  }

  if (result == -1) {
    usb_stall(0);
  } else {
    usb_ackstat(0);
  }

  return result;
}

/******************************************************************************/
static void set_line_coding_callback(void *cbdata)
{
  int result = 0;
  usb_req_t *req = (usb_req_t*)cbdata;

  // The request has completed, check for errors
  if (req->error_code != 0) {
    // TODO - error handling
    req->error_code = 0;
  }

  if (callback[ACM_CB_SET_LINE_CODING]) {
    result = callback[ACM_CB_SET_LINE_CODING]();
  }

  if (result == -1) {
    usb_stall(0);
  } else {
    usb_ackstat(0);
  }
}

/******************************************************************************/
int set_line_coding(void)
{
  /* Has no meaning for us, but we store it anyway */
  memset(&creq, 0, sizeof(usb_req_t));
  creq.ep = 0;
  creq.data = (uint8_t*)&line_coding;
  creq.reqlen = sizeof(line_coding);
  creq.callback = set_line_coding_callback;
  creq.cbdata = &creq;

  return usb_read_endpoint(&creq);
}

/******************************************************************************/
int get_line_coding(void)
{
  memset(&creq, 0, sizeof(usb_req_t));
  creq.ep = 0;
  creq.data = (uint8_t*)&line_coding;
  creq.reqlen = sizeof(line_coding);
  creq.callback = NULL;
  creq.cbdata = NULL;

  return usb_write_endpoint(&creq);
}
