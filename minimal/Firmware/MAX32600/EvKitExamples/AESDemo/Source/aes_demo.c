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
#include "ioman.h"
#include "uart.h"
#include "aes.h"

#include "aes_demo_test_vector.h"

/* Local prototypes */
void aes_demo_init(void);
int aes_check(mxc_aes_mode_t mode, mxc_aes_dir_t dir, 
	      const uint8_t *key, const uint8_t *in, const uint8_t *expected);
int aes_check_async(mxc_aes_mode_t mode, mxc_aes_dir_t dir, 
		    const uint8_t *key, const uint8_t *in, const uint8_t *expected);

/* Global flag, set by ISR (asynchronous API callback) and cleared by waiting check function */
volatile unsigned int aes_done = 0;
/* Work area for ISR  
 *  - Note that it is not volatile, as the pointer async_work does not change, 
 *     only the contents.
 */
uint8_t async_work[MXC_AES_DATA_LEN];

/*** START: MOVE TO BSP ***/

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
    int ret_val; 

    ret_val = UART_Write(UART_PORT, (uint8_t*)ptr, len);
    
    /* auto insert a carrage return to be nice to terminals 
     * we enabled "buffered IO", therefore this will be called for 
     * every '\n' in printf() 
     */
    if(ptr[len-1] == '\n')
        UART_Write(UART_PORT, (uint8_t*)"\r", 1);

    return ret_val;
}

/*** END: MOVE TO BSP ***/

#ifndef TOP_MAIN
int main(void)
{
    unsigned int fails = 0;
    unsigned int passes = 0;

    /* enable instruction cache */
    ICC_Enable();

    /* use the internal Ring Osc in 24MHz mode */
    CLKMAN_SetSystemClock(MXC_E_CLKMAN_SYSTEM_SOURCE_SELECT_24MHZ_RO);
    CLKMAN_WaitForSystemClockStable();
    
    /* set systick to the RTC input 32.768kHz clock, not system clock; this is needed to keep JTAG alive */
    CLKMAN_SetRTOSMode(TRUE);

    /* setup UART0 pins, mapping as EvKit to onboard FTDI UART to USB */
    IOMAN_UART0(MXC_E_IOMAN_MAPPING_A, TRUE, FALSE, FALSE);

    /* setup UART */
    UART_Config(UART_PORT, BAUD_RATE, FALSE, FALSE, FALSE);

    /* give libc a buffer, this way libc will not malloc() a stdio buffer */
    setvbuf(stdout, uart_stdout_buf0, _IOLBF, (size_t)sizeof(uart_stdout_buf0));
    setvbuf(stdin, NULL, _IONBF, 0);

    /* Print banner */
    printf("\nMAX32600 AES NIST Monte-Carlo Test Vector Demo\n");

    /* Set up clocks for AES block */
    aes_demo_init();

    /* Synchronous (blocking) API calls */
    printf("\n -- Synchronous API --\n");

    /* Encrypt 128-bit */
    printf("Running 128-bit encrypt .. "); fflush(stdout);
    if (aes_check(MXC_E_AES_MODE_128, MXC_E_AES_ENCRYPT, nist_enc_128_key, nist_enc_128_pt, nist_enc_128_ct) != 0) {
      printf("ERROR: AES-128 ENCRYPT FAILED\n");
      fails++;
    } else {
      printf("pass\n");
      passes++;
    }

    /* Decrypt 128-bit */
    printf("Running 128-bit decrypt .. "); fflush(stdout);
    if (aes_check(MXC_E_AES_MODE_128, MXC_E_AES_DECRYPT, nist_dec_128_key, nist_dec_128_ct, nist_dec_128_pt) != 0) {
      printf("ERROR: AES-128 DECRYPT FAILED\n");
      fails++;
    } else {
      printf("pass\n");
      passes++;
    }

    /* Encrypt 192-bit */
    printf("Running 192-bit encrypt .. "); fflush(stdout);
    if (aes_check(MXC_E_AES_MODE_192, MXC_E_AES_ENCRYPT, nist_enc_192_key, nist_enc_192_pt, nist_enc_192_ct) != 0) {
      printf("ERROR: AES-192 ENCRYPT FAILED\n");
      fails++;
    } else {
      printf("pass\n");
      passes++;
    }

    /* Decrypt 192-bit */
    printf("Running 192-bit decrypt .. "); fflush(stdout);
    if (aes_check(MXC_E_AES_MODE_192, MXC_E_AES_DECRYPT, nist_dec_192_key, nist_dec_192_ct, nist_dec_192_pt) != 0) {
      printf("ERROR: AES-192 DECRYPT FAILED\n");
      fails++;
    } else {
      printf("pass\n");
      passes++;
    }    

   /* Encrypt 256-bit */
    printf("Running 256-bit encrypt .. "); fflush(stdout);
    if (aes_check(MXC_E_AES_MODE_256, MXC_E_AES_ENCRYPT, nist_enc_256_key, nist_enc_256_pt, nist_enc_256_ct) != 0) {
      printf("ERROR: AES-256 ENCRYPT FAILED\n");
      fails++;
    } else {
      printf("pass\n");
      passes++;
    }

    /* Decrypt 256-bit */
    printf("Running 256-bit decrypt .. "); fflush(stdout);
    if (aes_check(MXC_E_AES_MODE_256, MXC_E_AES_DECRYPT, nist_dec_256_key, nist_dec_256_ct, nist_dec_256_pt) != 0) {
      printf("ERROR: AES-256 DECRYPT FAILED\n");
      fails++;
    } else {
      printf("pass\n");
      passes++;
    }  

    printf("\n -- Asynchronous API --\n");

    /* Encrypt 128-bit */
    printf("Running 128-bit encrypt .. "); fflush(stdout);
    if (aes_check_async(MXC_E_AES_MODE_128, MXC_E_AES_ENCRYPT_ASYNC, nist_enc_128_key, nist_enc_128_pt, nist_enc_128_ct) != 0) {
      printf("ERROR: AES-128 ENCRYPT FAILED\n");
      fails++;
    } else {
      printf("pass\n");
      passes++;
    }

    /* Decrypt 128-bit */
    printf("Running 128-bit decrypt .. "); fflush(stdout);
    if (aes_check_async(MXC_E_AES_MODE_128, MXC_E_AES_DECRYPT_ASYNC, nist_dec_128_key, nist_dec_128_ct, nist_dec_128_pt) != 0) {
      printf("ERROR: AES-128 DECRYPT FAILED\n");
      fails++;
    } else {
      printf("pass\n");
      passes++;
    }

    /* Encrypt 192-bit */
    printf("Running 192-bit encrypt .. "); fflush(stdout);
    if (aes_check_async(MXC_E_AES_MODE_192, MXC_E_AES_ENCRYPT_ASYNC, nist_enc_192_key, nist_enc_192_pt, nist_enc_192_ct) != 0) {
      printf("ERROR: AES-192 ENCRYPT FAILED\n");
      fails++;
    } else {
      printf("pass\n");
      passes++;
    }

    /* Decrypt 192-bit */
    printf("Running 192-bit decrypt .. "); fflush(stdout);
    if (aes_check_async(MXC_E_AES_MODE_192, MXC_E_AES_DECRYPT_ASYNC, nist_dec_192_key, nist_dec_192_ct, nist_dec_192_pt) != 0) {
      printf("ERROR: AES-192 DECRYPT FAILED\n");
      fails++;
    } else {
      printf("pass\n");
      passes++;
    }    

   /* Encrypt 256-bit */
    printf("Running 256-bit encrypt .. "); fflush(stdout);
    if (aes_check_async(MXC_E_AES_MODE_256, MXC_E_AES_ENCRYPT_ASYNC, nist_enc_256_key, nist_enc_256_pt, nist_enc_256_ct) != 0) {
      printf("ERROR: AES-256 ENCRYPT FAILED\n");
      fails++;
    } else {
      printf("pass\n");
      passes++;
    }

    /* Decrypt 256-bit */
    printf("Running 256-bit decrypt .. "); fflush(stdout);
    if (aes_check_async(MXC_E_AES_MODE_256, MXC_E_AES_DECRYPT_ASYNC, nist_dec_256_key, nist_dec_256_ct, nist_dec_256_pt) != 0) {
      printf("ERROR: AES-256 DECRYPT FAILED\n");
      fails++;
    } else {
      printf("pass\n");
      passes++;
    }

    printf("\nTests complete: %u tests passed, %u tests failed\n", passes, fails);

    /* Loop endlessly at end of demo */
    while (1) {
      __NOP();
    }
}
#endif /* TOP_MAIN */

void aes_demo_init(void)
{
    /* Enable crypto ring oscillator, which is used by all TPU components (AES, uMAA, etc.) */
    CLKMAN_CryptoClockConfig(MXC_E_CLKMAN_STABILITY_COUNT_2_8_CLKS);

    /* Un-gate crypto clock to clock manager, and change prescaler to /1 */
    CLKMAN_CryptoClockEnable();
    CLKMAN_SetCryptClkScale(MXC_E_CLKMAN_CRYPT_CLK_AES, MXC_E_CLKMAN_CLK_SCALE_ENABLED);
}

int aes_check(mxc_aes_mode_t mode, mxc_aes_dir_t dir, 
	      const uint8_t *key, const uint8_t *in, const uint8_t *expected)
{
  uint8_t ct1[MXC_AES_DATA_LEN], ct2[MXC_AES_DATA_LEN];
  int i;
  
  /* Insert key into AES engine */
  AES_SetKey(key, mode);
  
  /* Run an AES encrypt operation over NIST Monte-carlo test vector, 1000 encrypts */
  
  /* Unroll one loop to get everything into our temporary working arrays */
  if (dir == MXC_E_AES_ENCRYPT) {
    if (AES_ECBEncrypt(in, ct1, mode) != MXC_E_AES_OK) return -1;
    if (AES_ECBEncrypt(ct1, ct2, mode) != MXC_E_AES_OK) return -1;
  } else {
    if (AES_ECBDecrypt(in, ct1, mode) != MXC_E_AES_OK) return -1;
    if (AES_ECBDecrypt(ct1, ct2, mode) != MXC_E_AES_OK) return -1;
  }
  
  /* Repeat this process 998 more times, result left in ct2 at end */
  for (i = 0; i < 499; i++) {
    if (dir == MXC_E_AES_ENCRYPT) {
      if (AES_ECBEncrypt(ct2, ct1, mode) != MXC_E_AES_OK) return -1;
      if (AES_ECBEncrypt(ct1, ct2, mode) != MXC_E_AES_OK) return -1;
    } else {
      if (AES_ECBDecrypt(ct2, ct1, mode) != MXC_E_AES_OK) return -1;
      if (AES_ECBDecrypt(ct1, ct2, mode) != MXC_E_AES_OK) return -1;
    }
  }
  
  /* Check result */
  if (memcmp(ct2, expected, MXC_AES_DATA_LEN) == 0) {
    return 0;
  } else {
    return -1;
  }
}

void aes_isr(void)
{
  /* Copy output of engine into work area */
  AES_GetOutput(async_work);
  
  /* Signal bottom half that data is ready */
  aes_done = 1;

  return;
}

int aes_check_async(mxc_aes_mode_t mode, mxc_aes_dir_t dir, 
		    const uint8_t *key, const uint8_t *in, const uint8_t *expected)
{
  int i;

  /* Insert key into AES engine */
  AES_SetKey(key, mode);

  /* Run an AES encrypt operation over NIST Monte-carlo test vector, 1000 encrypts */
  aes_done = 0;
  
  /* Unroll one loop to get everything into our temporary working arrays */
  if (dir == MXC_E_AES_ENCRYPT_ASYNC) {
    if (AES_ECBEncryptAsync(in, mode, aes_isr) != MXC_E_AES_OK) return -1;
  } else {
    if (AES_ECBDecryptAsync(in, mode, aes_isr) != MXC_E_AES_OK) return -1;
  }

  /* Wait for completion */
  while (!aes_done) {
    /* Could also be LP2, if desired */
    __NOP();
  }
  aes_done = 0;

  /* Repeat this process 999 more times */
  for (i = 0; i < 999; i++) {
    if (dir == MXC_E_AES_ENCRYPT_ASYNC) {
      if (AES_ECBEncryptAsync(async_work, mode, aes_isr) != MXC_E_AES_OK) return -1;
    } else {
      if (AES_ECBDecryptAsync(async_work, mode, aes_isr) != MXC_E_AES_OK) return -1;
    }
    /* Wait for completion */
    while (!aes_done) {
      /* Could also be LP2, if desired */
      __NOP();
    }  
    aes_done = 0;
  }

  /* Check result */
  if (memcmp(async_work, expected, MXC_AES_DATA_LEN) == 0) {
    return 0;
  } else {
    return -1;
  }

  return -1;
}
