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
 * $Date: 2016-03-11 12:50:27 -0600 (Fri, 11 Mar 2016) $ 
 * $Revision: 21840 $
 *
 ******************************************************************************/

/**
 * @file    main.c
 * @brief   I2C Loopback Example
 * @details This example uses the I2C Master to read/write to the I2C Slave. For 
 *          this example you must connect P1.6 to P3.4 and P1.7 to P3.5. The Master
 *          will use P1.6 and P1.7. The slave will use P3.4 and P3.5. You must also
 *          connect the pull-up jumpers(JP23 and JP24) to the proper I/O voltage.
 *          Refer to JP27 to determine the I/O voltage.
 * @note    Other devices on the EvKit will be using the same bus. This example cannot be combined with
 *          a PMIC or bluetooth example because the I2C slave uses GPIO pins for those devices.
 */

/***** Includes *****/
#include <stdio.h>
#include <stdint.h>
#include "mxc_config.h"
#include "i2cs.h"
#include "i2cm.h"

/***** Definitions *****/
#define I2C_MASTER          MXC_I2CM0
#define I2C_MASTER_IDX      0

#define I2C_SLAVE           MXC_I2CS
#define I2C_SLAVE_IDX       IOMAN_MAP_B
#define I2C_SLAVE_ADDR      0x58            // address[7:1]

#define I2C_SPEED           I2CS_SPEED_400KHZ

/***** Globals *****/
static uint8_t txdata[33];
static uint8_t rxdata[33];

/***** Functions *****/

/******************************************************************************/
int main(void)
{
    int error, retval = 0;
    uint8_t addr;

    printf("\n***** I2C Loopback Example *****\n");

    // Initialize test data
    for(addr = 0; addr < 33; addr++) {
        txdata[addr] = addr;
        rxdata[addr] = 0;
    }

    // Setup the I2CM
    sys_cfg_i2cm_t i2cm_sys_cfg;
    ioman_cfg_t io_cfg = IOMAN_I2CM(I2C_MASTER_IDX, 1, 0);
    i2cm_sys_cfg.clk_scale = CLKMAN_SCALE_DIV_1;
    i2cm_sys_cfg.io_cfg = io_cfg;
    I2CM_Init(I2C_MASTER, &i2cm_sys_cfg, I2C_SPEED);

    // Setup the I2CS
    sys_cfg_i2cs_t i2cs_sys_cfg;
    ioman_cfg_t i2cs_io_cfg = IOMAN_I2CS(I2C_SLAVE_IDX, 1);
    i2cs_sys_cfg.clk_scale = CLKMAN_SCALE_DIV_1;
    i2cs_sys_cfg.io_cfg = i2cs_io_cfg;
    I2CS_Init(I2C_SLAVE, &i2cs_sys_cfg, I2C_SPEED, I2C_SLAVE_ADDR, I2CS_ADDR_8);

    // Write data to the slave. First byte sets the address pointer inside the slave
    if((error = I2CM_Write(I2C_MASTER, I2C_SLAVE_ADDR, NULL, 0, txdata, 33)) != 33) {
        printf("I2C_Write error %d\n", error);
        retval = -1;
    }

    // Check the data in the I2CS
    for(addr = 0; addr < 32; addr++) {
        if(I2CS_Read(I2C_SLAVE, addr) != txdata[addr+1]) {
            printf("Data mismatch\n");
            retval = -1;
        }
    }

    // Read the data back from the master. Send the address to the slave then restart
    // before reading
    addr = 0;
    if((error = I2CM_Read(I2C_MASTER, I2C_SLAVE_ADDR, &addr, 1, rxdata, 32)) != 32) {
        printf("I2C_Read error %d\n", error);
        retval = -1;
    }

    for(addr = 0; addr < 32; addr++) {
        if(rxdata[addr] != txdata[addr+1]) {
            printf("Data mismatch\n");
            retval = -1;
        }
    }

    if(retval == 0) {
        printf("I2C Memory verified\n");
    } else {
        printf("Error verifying I2C Memory\n");
    }

    while(1) {}
}
