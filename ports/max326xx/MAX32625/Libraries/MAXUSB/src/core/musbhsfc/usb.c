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
 * $Date: 2017-04-03 12:40:53 -0500 (Mon, 03 Apr 2017) $
 * $Revision: 27352 $
 *
 ******************************************************************************/

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "mxc_config.h"
#include "mxc_assert.h"
#include "usbhs_regs.h"
#include "usb.h"

typedef enum {
    SETUP_IDLE,
    SETUP_NODATA,
    SETUP_DATA_OUT,
    SETUP_DATA_IN
} setup_phase_t;

/* storage for active endpoint data request objects */
static usb_req_t *usb_request[MXC_USBHS_NUM_EP];
/* endpoint sizes */
static uint8_t ep_size[MXC_USBHS_NUM_EP];
/* MUSBHSFC does not track SETUP in hardware, so instantiate state variable */
static setup_phase_t setup_phase = SETUP_IDLE;
/* Driver options passed in during usb_init() */
static maxusb_cfg_options_t driver_opts;

static volatile uint8_t *get_fifo_ptr(unsigned int ep)
{
    volatile uint32_t *ptr;

    ptr = &MXC_USBHS->fifo0;
    ptr += ep; /* Pointer math: multiplies ep by sizeof(uint32_t) */

    return (volatile uint8_t *)ptr;
}

int usb_init(maxusb_cfg_options_t *options)
{
    int i;

    /* Endpoint 0 is CONTROL, size fixed in hardware. Does not need configuration. */
    ep_size[0] = 64;

    /* Reset all other endpoints */
    for (i = 1; i < MXC_USBHS_NUM_EP; i++) {
	usb_reset_ep(i);
    }

    setup_phase = SETUP_IDLE;

    /* Save the init options */
    memcpy(&driver_opts, options, sizeof(maxusb_cfg_options_t));
    
    /* Start out disconnected */
    MXC_USBHS->power = 0;

    /* Disable all interrupts */
    MXC_USBHS->intrinen = 0;
    MXC_USBHS->introuten = 0;
    MXC_USBHS->intrusben = 0;
    
    return 0;
}

int usb_shutdown(void)
{
    /* Disconnect and disable HS, too. */
    MXC_USBHS->power = 0;
    
    /* Disable all interrupts */
    MXC_USBHS->intrinen = 0;
    MXC_USBHS->introuten = 0;
    MXC_USBHS->intrusben = 0;
    
    return 0;
}

int usb_connect(void)
{
    /* Should high-speed negotiation be attempted? */
    if (driver_opts.enable_hs) {
	MXC_USBHS->power |= MXC_F_USBHS_POWER_HS_ENABLE;
    }
  
    /* Connect to bus, if present */
    MXC_USBHS->power |= MXC_F_USBHS_POWER_SOFTCONN;

    setup_phase = SETUP_IDLE;

    /* Enable SOF for missed-interrupt check */
    //MXC_USBHS->intrusben = MXC_F_USBHS_INTRUSB_SOF_INT;
    
    return 0;
}

int usb_disconnect(void)
{
    /* Disconnect from bus */
    MXC_USBHS->power &= ~MXC_F_USBHS_POWER_SOFTCONN;

    setup_phase = SETUP_IDLE;

    return 0;
}

int usb_config_ep(unsigned int ep, maxusb_ep_type_t type, unsigned int size)
{
    if (!ep || (ep >= MXC_USBHS_NUM_EP) || (size > MXC_USBHS_MAX_PACKET)) {
	/* Can't configure this endpoint, invalid endpoint, or size too big */
	return -1;
    }

    /* Default to disabled */
    usb_reset_ep(ep);

    /* Interrupts must be disabled while banked registers are accessed */
    MAXUSB_ENTER_CRITICAL();
    
    /* Select register index for endpoint */
    MXC_USBHS->index = ep;
    
    switch (type) {
	case MAXUSB_EP_TYPE_DISABLED:
	    break;
	case MAXUSB_EP_TYPE_OUT:
	    MXC_USBHS->outcsrl = MXC_F_USBHS_OUTCSRL_CLRDATATOG;
	    MXC_USBHS->outcsru = MXC_F_USBHS_OUTCSRU_DPKTBUFDIS;
	    MXC_USBHS->outmaxp = size;
	    ep_size[ep] = size;
	    MXC_USBHS->introuten |= (1 << ep);
	    break;
	case MAXUSB_EP_TYPE_IN:
	    MXC_USBHS->incsrl = MXC_F_USBHS_INCSRL_CLRDATATOG;
	    MXC_USBHS->incsru = MXC_F_USBHS_INCSRU_DPKTBUFDIS | MXC_F_USBHS_INCSRU_MODE;
	    MXC_USBHS->inmaxp = size;
	    ep_size[ep] = size;
	    MXC_USBHS->intrinen |= (1 << ep);
	    break;
	default:
	    MAXUSB_EXIT_CRITICAL();
	    return -1;
	    break;
    }

    MAXUSB_EXIT_CRITICAL();
    return 0;
}

int usb_is_configured(unsigned int ep)
{
    return !!(ep_size[ep]);
}

int usb_stall(unsigned int ep)
{
    usb_req_t *req;

    if (!usb_is_configured(ep)) {
	/* Can't stall an unconfigured endpoint */
	return -1;
    }

    /* Interrupts must be disabled while banked registers are accessed */
    MAXUSB_ENTER_CRITICAL();
    
    MXC_USBHS->index = ep;

    if (ep == 0) {
	MXC_USBHS->csr0 |= MXC_F_USBHS_CSR0_SERV_OUTPKTRDY | MXC_F_USBHS_CSR0_SEND_STALL;
	setup_phase = SETUP_IDLE;
    } else {
	if (MXC_USBHS->incsru & MXC_F_USBHS_INCSRU_MODE) {
	    /* IN endpoint */
	    MXC_USBHS->incsrl |= MXC_F_USBHS_INCSRL_SENDSTALL;
	} else {
	    /* Otherwise, must be OUT endpoint */
	    MXC_USBHS->outcsrl |= MXC_F_USBHS_OUTCSRL_SENDSTALL;
	}
    }

    /* clear pending requests */
    req = usb_request[ep];
    usb_request[ep] = NULL;

    if (req) {
	/* complete pending requests with error */
	req->error_code = -1;
	if (req->callback) {
	    req->callback(req->cbdata);
	}
    }

    MAXUSB_EXIT_CRITICAL();
    return 0;
}

int usb_unstall(unsigned int ep)
{
    if (!usb_is_configured(ep)) {
	/* Can't unstall an unconfigured endpoint */
	return -1;
    }

    /* Interrupts must be disabled while banked registers are accessed */
    MAXUSB_ENTER_CRITICAL();
    
    MXC_USBHS->index = ep;
    
    if (ep != 0) {
	if (MXC_USBHS->incsru & MXC_F_USBHS_INCSRU_MODE) {
	    /* IN endpoint */
	    MXC_USBHS->incsrl = MXC_F_USBHS_INCSRL_CLRDATATOG;
	} else {
	    /* Otherwise, must be OUT endpoint */
	    MXC_USBHS->outcsrl = MXC_F_USBHS_OUTCSRL_CLRDATATOG;
	}   
    }

    MAXUSB_EXIT_CRITICAL();
    return 0;
}

int usb_is_stalled(unsigned int ep)
{
    unsigned int stalled;
    
    MXC_USBHS->index = ep;
    
    if (ep) {
	if (MXC_USBHS->incsru & MXC_F_USBHS_INCSRU_MODE) {
	    /* IN endpoint */
	    stalled = !!(MXC_USBHS->incsrl & MXC_F_USBHS_INCSRL_SENDSTALL);
	} else {
	    /* Otherwise, must be OUT endpoint */
	    stalled = !!(MXC_USBHS->outcsrl & MXC_F_USBHS_OUTCSRL_SENDSTALL);
	}
    } else {
	/* Control (EP 0) */
	stalled = !!(MXC_USBHS->csr0 & MXC_F_USBHS_CSR0_SEND_STALL);
    }

    return stalled;
}

int usb_reset_ep(unsigned int ep)
{
    usb_req_t *req;

    if (ep >= MXC_USBHS_NUM_EP) {
	return -1;
    }

    /* Interrupts must be disabled while banked registers are accessed */
    MAXUSB_ENTER_CRITICAL();
    
    /* clear pending requests */
    req = usb_request[ep];
    usb_request[ep] = NULL;

    if (ep) {
	ep_size[ep] = 0;
	
	/* Select register index for endpoint */
	MXC_USBHS->index = ep;
        
	/* Default to disabled */
	MXC_USBHS->intrinen &= ~(1 << ep);
	MXC_USBHS->introuten &= ~(1 << ep);

	MXC_USBHS->incsrl = MXC_F_USBHS_INCSRL_SENDSTALL;

	MXC_USBHS->incsru = MXC_F_USBHS_INCSRU_DPKTBUFDIS;
	MXC_USBHS->inmaxp = 0;
        
	MXC_USBHS->outcsrl = MXC_F_USBHS_OUTCSRL_SENDSTALL;

	MXC_USBHS->outcsru = MXC_F_USBHS_OUTCSRU_DPKTBUFDIS;
	MXC_USBHS->outmaxp = 0;

	MAXUSB_EXIT_CRITICAL();

	/* We specifically do not complete SETUP callbacks, as this causes undesired SETUP status-stage STALLs */
	if (req) {
	    /* complete pending requests with error */
	    req->error_code = -1;
	    if (req->callback) {
		req->callback(req->cbdata);
	    }
	}
    } else {
	MAXUSB_EXIT_CRITICAL();
    }

    return 0;
}

int usb_ackstat(unsigned int ep)
{
    uint32_t saved_index;
     
    if (ep) {
	/* Only valid for endpoint 0 */
	return -1;
    }

    /* Interrupts must be disabled while banked registers are accessed */
    MAXUSB_ENTER_CRITICAL();

    saved_index = MXC_USBHS->index;
    MXC_USBHS->index = 0;

    /* On this hardware, only setup transactions with no data stage need to be explicitly ACKed */
    if (setup_phase == SETUP_NODATA) {
	MXC_USBHS->csr0 |= MXC_F_USBHS_CSR0_SERV_OUTPKTRDY | MXC_F_USBHS_CSR0_DATA_END;
    }
    
    setup_phase = SETUP_IDLE;
    
    MXC_USBHS->index = saved_index;
    
    MAXUSB_EXIT_CRITICAL();
    
    return 0;
}

void usb_dma_isr(void)
{
  /* Not implemented */
}

/* sent packet done handler*/
static void event_in_data(uint32_t irqs)
{
    uint32_t epnum, buffer_bit, data_left;
    usb_req_t *req;
    uint8_t *data;
    unsigned int len;
    volatile uint8_t *fifoptr;

    /* Loop for each data endpoint */
    for (epnum = 0; epnum < MXC_USBHS_NUM_EP; epnum++) {

	buffer_bit = (1 << epnum);
	if ((irqs & buffer_bit) == 0) { /* Not set, next Endpoint */
	    continue;
	}
	
	/* If an interrupt was received, but no request on this EP, ignore. */
	if (!usb_request[epnum]) {
	    continue;
	}

	/* This function is called within interrupt context, so no need for a critical section */
	
	MXC_USBHS->index = epnum;

	req = usb_request[epnum];
	data_left = req->reqlen - req->actlen;

	/* Check for more data left to transmit */
	if (data_left) {
        
	    if (data_left >= ep_size[epnum]) {
		len = ep_size[epnum];
	    } else {
		len = data_left;
	    }
	    /* Last place we left off */
	    data = req->data + req->actlen;

	    /* Update it for the next transmission */
	    req->actlen += len;
	    fifoptr = get_fifo_ptr(epnum);

	    /* Fill FIFO with data */
	    while (len--) {
		*fifoptr = *data++;
	    }

	    if (!epnum) {
		if (usb_request[epnum]->actlen == usb_request[epnum]->reqlen) {
		    /* Implicit status-stage ACK, move state machine back to IDLE */
		    setup_phase = SETUP_IDLE;
		    MXC_USBHS->csr0 |= MXC_F_USBHS_CSR0_INPKTRDY | MXC_F_USBHS_CSR0_DATA_END;

		    /* TODO: Improve this handling -- must be done here due to how EP0-IN operates */
		    /* free request */
		    usb_request[epnum] = NULL;
		    
		    /* set done return value */
		    if (req->callback) {
		      req->callback(req->cbdata);
		    }
		} else {
		    MXC_USBHS->csr0 |= MXC_F_USBHS_CSR0_INPKTRDY;
		}
	    } else {
		/* Arm for transmit to host */
		MXC_USBHS->incsrl = MXC_F_USBHS_INCSRL_INPKTRDY;
	    }
        
	} else {
	    /* all done sending data */

	    /* free request */
	    usb_request[epnum] = NULL;
	    
	    /* set done return value */
	    if (req->callback) {
		req->callback(req->cbdata);
	    }
	}
    }
}

/* received packet */
static void event_out_data(uint32_t irqs)
{
    uint32_t epnum, buffer_bit, reqsize, rxsize;
    volatile uint8_t *fifoptr;
    uint8_t *dataptr;
    usb_req_t *req;
    
    /* Loop for each data endpoint */
    for (epnum = 0; epnum < MXC_USBHS_NUM_EP; epnum++) {

	buffer_bit = (1 << epnum);
	if ((irqs & buffer_bit) == 0) {
	    continue;
	}

	/* If an interrupt was received, but no request on this EP, ignore. */
	if (!usb_request[epnum]) {
	    continue; 
	}

	req = usb_request[epnum];

	/* This function is called within interrupt context, so no need for a critical section */
	
	/* Select this endpoint for banked registers */
	MXC_USBHS->index = epnum;
	fifoptr = get_fifo_ptr(epnum);

	if (!epnum) {
	    if (!(MXC_USBHS->csr0 & MXC_F_USBHS_CSR0_OUTPKTRDY)) {
		continue;
	    }
	    if (MXC_USBHS->count0 == 0) {
		/* ZLP */
		usb_request[epnum] = NULL;
		/* Let the callback do the status stage */
		/* call it done */
		if (req->callback) {
		    req->callback(req->cbdata);
		}
		continue;
	    } else {
		/* Write as much as we can to the request buffer */
		reqsize = MXC_USBHS->count0;
		if (reqsize > (req->reqlen - req->actlen)) {
		    reqsize = (req->reqlen - req->actlen);
		}
	    }
	} else {
	    if (!(MXC_USBHS->outcsrl & MXC_F_USBHS_OUTCSRL_OUTPKTRDY)) {
		continue;
	    }
	    if (MXC_USBHS->outcount == 0) {
		/* ZLP */
		/* Clear out request */
		usb_request[epnum] = NULL;

                /* NOTE: Endpoint FIFO control not returned to hardware.  Next usb_read_endpoint()
                 * function call will set up the request and return FIFO control to hardware.
                 */
		
		/* call it done */
		if (req->callback) {
		    req->callback(req->cbdata);
		}
		continue;
	    } else {
		/* Write as much as we can to the request buffer */
		reqsize = MXC_USBHS->outcount;
		if (reqsize > (req->reqlen - req->actlen)) {
		    reqsize = (req->reqlen - req->actlen);
		}
	    }
	}
    
	/* Copy data from FIFO into data buffer */
	rxsize = reqsize;
	dataptr = &req->data[req->actlen];
	while (rxsize--) {
	    *dataptr++ = *fifoptr;
	}
	req->actlen += reqsize;

	if (!epnum) {
	    if (req->actlen == req->reqlen) {
		/* No more data */
		MXC_USBHS->csr0 |= MXC_F_USBHS_CSR0_SERV_OUTPKTRDY | MXC_F_USBHS_CSR0_DATA_END;
		/* Done */
		usb_request[epnum] = NULL;
        
		if (req->callback) {
		    req->callback(req->cbdata);
		}
	    } else {
		/* More data */
		MXC_USBHS->csr0 |= MXC_F_USBHS_CSR0_SERV_OUTPKTRDY;
	    }
	} else {
        
	  if ((req->type == MAXUSB_TYPE_PKT) || (req->actlen == req->reqlen)) {
		/* Done */
		usb_request[epnum] = NULL;

                /* NOTE: Endpoint FIFO control not returned to hardware.  Next usb_read_endpoint()
                 * function call will set up the request and return FIFO control to hardware.
                 */

		if (req->callback) {
		    req->callback(req->cbdata);
		}
	    } else {
	      /* Signal to H/W that FIFO has been read */
	      MXC_USBHS->outcsrl &= ~MXC_F_USBHS_OUTCSRL_OUTPKTRDY;
	    }
	}
    }
}

void usb_irq_handler(maxusb_usbio_events_t *evt)
{
    uint32_t saved_index;
    uint32_t in_flags, out_flags, usb_flags, usb_mxm_flags;
    int i, aborted = 0;

    /* Save current index register */
    saved_index = MXC_USBHS->index;

    /* Note: Hardware clears these after read, so we must process them all or they are lost */
    usb_flags = MXC_USBHS->intrusb & MXC_USBHS->intrusben;
    in_flags = MXC_USBHS->intrin & MXC_USBHS->intrinen;
    out_flags = MXC_USBHS->introut & MXC_USBHS->introuten;

    /* These USB interrupt flags are W1C. */
    usb_mxm_flags = MXC_USBHS->mxm_int & MXC_USBHS->mxm_int_en;
    MXC_USBHS->mxm_int = usb_mxm_flags;

    /* Map hardware-specific signals to generic stack events */
    evt->dpact  = !!(usb_flags & MXC_F_USBHS_INTRUSB_SOF_INT);
    evt->rwudn  = 0;
    evt->bact   = !!(usb_flags & MXC_F_USBHS_INTRUSB_SOF_INT);
    evt->brst   = !!(usb_flags & MXC_F_USBHS_INTRUSB_RESET_INT);
    evt->susp   = !!(usb_flags & MXC_F_USBHS_INTRUSB_SUSPEND_INT);
    evt->novbus = !!(usb_mxm_flags & MXC_F_USBHS_MXM_INT_NOVBUS);
    evt->vbus   = !!(usb_mxm_flags & MXC_F_USBHS_MXM_INT_VBUS);
    evt->brstdn = !!(usb_flags & MXC_F_USBHS_INTRUSB_RESET_INT); /* Hardware does not signal this, so simulate it */
    evt->sudav = 0; /* Overwritten, if necessary, below */

    /* Handle control state machine */
    if (in_flags & MXC_F_USBHS_INTRIN_EP0_IN_INT) {
		/* Select endpoint 0 */
		MXC_USBHS->index = 0;
		/* Process all error conditions */
		if (MXC_USBHS->csr0 & MXC_F_USBHS_CSR0_SENT_STALL) {
			/* Clear stall indication, go back to IDLE */
			MXC_USBHS->csr0 &= ~(MXC_F_USBHS_CSR0_SENT_STALL);
			/* Remove this from the IN flags so that it is not erroneously processed as data */
			in_flags &= ~MXC_F_USBHS_INTRIN_EP0_IN_INT;
			setup_phase = SETUP_IDLE;
			aborted = 1;
		}
		if (MXC_USBHS->csr0 & MXC_F_USBHS_CSR0_SETUP_END) {
			/* TODO: Abort pending requests, clear early end-of-control-transaction bit, go back to IDLE */
			MXC_USBHS->csr0 |= (MXC_F_USBHS_CSR0_SERV_SETUP_END);
			setup_phase = SETUP_IDLE;

			/* Remove this from the IN flags so that it is not erroneously processed as data */
			in_flags &= ~MXC_F_USBHS_INTRIN_EP0_IN_INT;
			usb_reset_ep(0);
			aborted = 1;
		}
		/* Now, check for a SETUP packet */
		if (!aborted) {
			if ((setup_phase == SETUP_IDLE) && (MXC_USBHS->csr0 & MXC_F_USBHS_CSR0_OUTPKTRDY)) {
			/* Flag that we got a SETUP packet */
			evt->sudav = 1;
			/* Remove this from the IN flags so that it is not erroneously processed as data */
			in_flags &= ~MXC_F_USBHS_INTRIN_EP0_IN_INT;
			} else {
			/* Otherwise, we are in endpoint 0 data IN/OUT */
			/* Fix interrupt flags so that OUTs are processed properly */
			if (setup_phase == SETUP_DATA_OUT) {
				out_flags |= MXC_F_USBHS_INTRIN_EP0_IN_INT;
			}
			/* SETUP_NODATA is silently ignored by event_in_data() right now.. could fix this later */
			}
		}
    }
    /* do cleanup in cases of bus reset */
    if (evt->brst) {
		setup_phase = SETUP_IDLE;
		/* kill any pending requests */
		for (i = 0; i < MXC_USBHS_NUM_EP; i++) {
			usb_reset_ep(i);
		}
		/* no need to process events after reset */
		return;
    }

    if (in_flags) {
    	event_in_data(in_flags);
    }

    if (out_flags) {
    	event_out_data(out_flags);
    }

    /* Restore register index before exiting ISR */
    MXC_USBHS->index = saved_index;

}

int usb_irq_enable(maxusb_event_t event)
{
    if (event >= MAXUSB_NUM_EVENTS) {
	return -1;
    }

    switch (event) {
	case MAXUSB_EVENT_BACT:
	    /* Bus Active */
	    MXC_USBHS->intrusben |= MXC_F_USBHS_INTRUSBEN_SOF_INT_EN;
	    break;
        
	case MAXUSB_EVENT_BRST:
	    /* Bus Reset */
	    MXC_USBHS->intrusben |= MXC_F_USBHS_INTRUSBEN_RESET_INT_EN;
	    break;
        
	case MAXUSB_EVENT_SUSP:
	    /* Suspend */
	    MXC_USBHS->intrusben |= MXC_F_USBHS_INTRUSBEN_SUSPEND_INT_EN;
	    break;
        
	case MAXUSB_EVENT_SUDAV:
	    /* Setup Data Available */
	    MXC_USBHS->intrinen |= MXC_F_USBHS_INTRINEN_EP0_INT_EN;
	    break;

	case MAXUSB_EVENT_VBUS:
	    /* VBUS Detect */
	    MXC_USBHS->mxm_int_en |= MXC_F_USBHS_MXM_INT_EN_VBUS;
	    break;
	    
	case MAXUSB_EVENT_NOVBUS:
	    /* NOVBUS Detect */
	    MXC_USBHS->mxm_int_en |= MXC_F_USBHS_MXM_INT_EN_NOVBUS;
	    break;
	    
	default:
	    /* Other events not supported by this hardware */
	    break;
    }

    return 0;
}

int usb_irq_disable(maxusb_event_t event)
{
    if (event >= MAXUSB_NUM_EVENTS) {
	return -1;
    }

    switch (event) {
	case MAXUSB_EVENT_BACT:
	    /* Bus Active */
	    MXC_USBHS->intrusben &= ~MXC_F_USBHS_INTRUSBEN_SOF_INT_EN;
	    break;
        
	case MAXUSB_EVENT_BRST:
	    /* Bus Reset */
	    MXC_USBHS->intrusben &= ~MXC_F_USBHS_INTRUSBEN_RESET_INT_EN;
	    break;
        
	case MAXUSB_EVENT_SUSP:
	    /* Suspend */
	    MXC_USBHS->intrusben &= ~MXC_F_USBHS_INTRUSBEN_SUSPEND_INT_EN;
	    break;
        
	case MAXUSB_EVENT_SUDAV:
	    /* Setup Data Available */
	    MXC_USBHS->intrinen &= ~MXC_F_USBHS_INTRINEN_EP0_INT_EN;
	    break;

	case MAXUSB_EVENT_VBUS:
	    /* VBUS Detect */
	    MXC_USBHS->mxm_int_en &= ~MXC_F_USBHS_MXM_INT_EN_VBUS;
	    break;
	    
	case MAXUSB_EVENT_NOVBUS:
	    /* NOVBUS Detect */
	    MXC_USBHS->mxm_int_en &= ~MXC_F_USBHS_MXM_INT_EN_NOVBUS;
	    break;
	    
	default:
	    /* Other events not supported by this hardware */
	    break;
    }

    return 0;
}

int usb_irq_clear(maxusb_event_t event)
{
    /* No-op on this hardware, as reading the interrupt flag register clears it */
    
    return 0;
}

int usb_get_setup(usb_setup_pkt *sud)
{
    volatile uint8_t *fifoptr = (uint8_t *)&MXC_USBHS->fifo0;
    
    /* Interrupts must be disabled while banked registers are accessed */
    MAXUSB_ENTER_CRITICAL();

    /* Select endpoint 0 */
    MXC_USBHS->index = 0;
    
    if ((sud == NULL) || !(MXC_USBHS->csr0 & MXC_F_USBHS_CSR0_OUTPKTRDY)) {
	return -1;
    }

    /* Pull SETUP packet out of FIFO */
    sud->bmRequestType = *fifoptr;
    sud->bRequest = *fifoptr;
    sud->wValue = *fifoptr;
    sud->wValue += (*fifoptr) << 8;
    sud->wIndex = *fifoptr;
    sud->wIndex += (*fifoptr) << 8;
    sud->wLength = *fifoptr;
    sud->wLength += (*fifoptr) << 8;

    /* Check for follow-on data and advance state machine */
    if (sud->wLength > 0) {
	MXC_USBHS->csr0 |= MXC_F_USBHS_CSR0_SERV_OUTPKTRDY;
	/* Determine if IN or OUT data follows */
	if (sud->bmRequestType & RT_DEV_TO_HOST) {
	    setup_phase = SETUP_DATA_IN;
	} else {
	    setup_phase = SETUP_DATA_OUT;
	}
    } else {
	/* SERV_OUTPKTRDY is set using usb_ackstat() */
	setup_phase = SETUP_NODATA;
    }
    
    MAXUSB_EXIT_CRITICAL();
    return 0;
}

int usb_set_func_addr(unsigned int addr)
{
    if (addr > 0x7f) {
	return -1;
    }

    MXC_USBHS->faddr = addr;
    
    return 0;
}

usb_req_t *usb_get_request(unsigned int ep)
{
    return usb_request[ep];
}

int usb_write_endpoint(usb_req_t *req)
{
    unsigned int ep  = req->ep;
    uint8_t *data    = req->data;
    unsigned int len = req->reqlen;
    unsigned int armed;
    volatile uint8_t *fifoptr;

    if (ep >= MXC_USBHS_NUM_EP) {
	return -1;
    }

    /* EP must be enabled (configured) */
    if (!usb_is_configured(ep)) {
	return -1;
    }

    /* Interrupts must be disabled while banked registers are accessed */
    MAXUSB_ENTER_CRITICAL();

    MXC_USBHS->index = ep;
    fifoptr = get_fifo_ptr(ep);
    
    /* if pending request; error */
    if (usb_request[ep] || (MXC_USBHS->incsrl & MXC_F_USBHS_INCSRL_INPKTRDY)) {
	MAXUSB_EXIT_CRITICAL();
	return -1;
    }

    /* assign req object */
    usb_request[ep] = req;

    /* clear errors */
    req->error_code = 0;
    
    /* Determine if DMA can be used for this transmit */
    armed = 0;

    if (!armed) {
	/* EP0 or no free DMA channel found, fall back to PIO */
	
	/* Determine how many bytes to be sent */
	if (len > ep_size[ep]) {
	    len = ep_size[ep];
	}
	usb_request[ep]->actlen = len;
	
	/* Fill FIFO with data */
	while (len--) {
	    *fifoptr = *data++;
	}
	
	if (!ep) {
	    if (usb_request[ep]->actlen == usb_request[ep]->reqlen) {
		MXC_USBHS->csr0 |= MXC_F_USBHS_CSR0_INPKTRDY | MXC_F_USBHS_CSR0_DATA_END;
	    } else {
		MXC_USBHS->csr0 |= MXC_F_USBHS_CSR0_INPKTRDY;
	    }
	} else {
	    /* Arm for transmit to host */
	    MXC_USBHS->incsrl = MXC_F_USBHS_INCSRL_INPKTRDY;
	}
    }
    
    MAXUSB_EXIT_CRITICAL();
    return 0;
}

int usb_read_endpoint(usb_req_t *req)
{
    unsigned int ep  = req->ep;
    uint32_t reqsize, rxsize;
    unsigned int armed = 0;
    volatile uint8_t *fifoptr;
    uint8_t *dataptr;
    
    if (ep >= MXC_USBHS_NUM_EP) {
	return -1;
    }

    /* Interrupts must be disabled while banked registers are accessed */
    MAXUSB_ENTER_CRITICAL();

    /* EP must be enabled (configured) and not stalled */
    if (!usb_is_configured(ep)) {
	return -1;
    }

    if (usb_is_stalled(ep)) {
	MAXUSB_EXIT_CRITICAL();
	return -1;
    }

    /* if pending request; error */
    if (usb_request[ep]) {
	MAXUSB_EXIT_CRITICAL();
	return -1;
    }

    /* clear errors */
    req->error_code = 0;
    
    /* reset length */
    req->actlen = 0;

    /* assign the req object */
    usb_request[ep] = req;

    /* Select endpoint */
    MXC_USBHS->index = ep;
    
    /* Since the OUT interrupt for EP 0 doesn't really exist, only do this logic for other endpoints */
    if (ep) {

        /* NOTE: In many cases, the endpoint FIFO will be empty but firmware did not return control
         * of FIFO to the hardware because no USB request existed.  This call to usb_read_endpoint()
         * submits a new request so now is the time to give the hardware control of the endpoint FIFO.
         */

        /* If FIFO is owned by software and zero length, transfer control to hardware. */
	if ((MXC_USBHS->outcsrl & MXC_F_USBHS_OUTCSRL_OUTPKTRDY) && (MXC_USBHS->outcount == 0)) {
          /* Signal to H/W that FIFO has been read */
	    MXC_USBHS->outcsrl &= ~MXC_F_USBHS_OUTCSRL_OUTPKTRDY;
        }

	armed = 0;
	
	if (!armed) {
	  /* EP0 or no free DMA channel found, fall back to PIO */
	  
	  /* See if data already in FIFO for this EP */
	  if (MXC_USBHS->outcsrl & MXC_F_USBHS_OUTCSRL_OUTPKTRDY) {
	    reqsize = MXC_USBHS->outcount;
	    if (reqsize > (req->reqlen - req->actlen)) {
	      reqsize = (req->reqlen - req->actlen);
	    }
	    
	    /* Copy data from FIFO into data buffer */
	    rxsize = reqsize;
	    fifoptr = get_fifo_ptr(ep);
	    dataptr = &req->data[req->actlen];
	    while (rxsize--) {
	      *dataptr++ = *fifoptr;
	    }
	    req->actlen += reqsize;
	    
	    /* Signal to H/W that FIFO has been read */
	    MXC_USBHS->outcsrl &= ~MXC_F_USBHS_OUTCSRL_OUTPKTRDY;
	  }
	}
    }

    MAXUSB_EXIT_CRITICAL();
    return 0;
}

void usb_remote_wakeup(void)
{
    MXC_USBHS->power |= MXC_F_USBHS_POWER_RESUME;
    driver_opts.delay_us(10000);
    MXC_USBHS->power &= ~MXC_F_USBHS_POWER_RESUME;
}
