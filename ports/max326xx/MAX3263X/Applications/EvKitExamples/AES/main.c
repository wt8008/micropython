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
 * $Date: 2016-03-23 15:28:53 -0500 (Wed, 23 Mar 2016) $
 * $Revision: 22067 $
 *
 ******************************************************************************/

/**
 * @file    main.c
 * @brief   AES demo application
 * @details Runs encryption and decryption on different bit size using NIST Monte-Carlo test vector
 */

#include <stdio.h>
#include <stdint.h>
#include <string.h>

#include "mxc_config.h"
#include "mxc_sys.h"
#include "aes.h"

#include "test_vector.h"

/* Local prototypes */
int aes_check(mxc_aes_mode_t mode, mxc_aes_dir_t dir,
              const uint8_t *key, const uint8_t *in, const uint8_t *expected);
int aes_check_async(mxc_aes_mode_t mode, mxc_aes_dir_t dir,
                    const uint8_t *key, const uint8_t *in, const uint8_t *expected);

/* Global flag, set by ISR and cleared by waiting check function */
volatile unsigned int aes_done = 0;

/* Work area for ISR
 *  - Note that it is not volatile, as the pointer async_work does not change,
 *     only the contents.
 */
uint8_t async_work[MXC_AES_DATA_LEN];

int main(void)
{
    unsigned int fails = 0;
    unsigned int passes = 0;

    /* Init system-dependent AES clocks */
    SYS_AES_Init();

    /* Print banner */
    printf("\nMAX3263x AES NIST Monte-Carlo Test Vector Demo\n");

    /* Synchronous (blocking) API calls */
    printf("\n -- Synchronous API --\n");

    /* Encrypt 128-bit */
    printf("Running 128-bit encrypt .. ");
    if (aes_check(MXC_E_AES_MODE_128, MXC_E_AES_ENCRYPT, nist_enc_128_key, nist_enc_128_pt, nist_enc_128_ct) != 0) {
        printf("ERROR: AES-128 ENCRYPT FAILED\n");
        fails++;
    } else {
        printf("pass\n");
        passes++;
    }

    /* Decrypt 128-bit */
    printf("Running 128-bit decrypt .. ");
    if (aes_check(MXC_E_AES_MODE_128, MXC_E_AES_DECRYPT, nist_dec_128_key, nist_dec_128_ct, nist_dec_128_pt) != 0) {
        printf("ERROR: AES-128 DECRYPT FAILED\n");
        fails++;
    } else {
        printf("pass\n");
        passes++;
    }

    /* Encrypt 192-bit */
    printf("Running 192-bit encrypt .. ");
    if (aes_check(MXC_E_AES_MODE_192, MXC_E_AES_ENCRYPT, nist_enc_192_key, nist_enc_192_pt, nist_enc_192_ct) != 0) {
        printf("ERROR: AES-192 ENCRYPT FAILED\n");
        fails++;
    } else {
        printf("pass\n");
        passes++;
    }

    /* Decrypt 192-bit */
    printf("Running 192-bit decrypt .. ");
    if (aes_check(MXC_E_AES_MODE_192, MXC_E_AES_DECRYPT, nist_dec_192_key, nist_dec_192_ct, nist_dec_192_pt) != 0) {
        printf("ERROR: AES-192 DECRYPT FAILED\n");
        fails++;
    } else {
        printf("pass\n");
        passes++;
    }

    /* Encrypt 256-bit */
    printf("Running 256-bit encrypt .. ");
    if (aes_check(MXC_E_AES_MODE_256, MXC_E_AES_ENCRYPT, nist_enc_256_key, nist_enc_256_pt, nist_enc_256_ct) != 0) {
        printf("ERROR: AES-256 ENCRYPT FAILED\n");
        fails++;
    } else {
        printf("pass\n");
        passes++;
    }

    /* Decrypt 256-bit */
    printf("Running 256-bit decrypt .. ");
    if (aes_check(MXC_E_AES_MODE_256, MXC_E_AES_DECRYPT, nist_dec_256_key, nist_dec_256_ct, nist_dec_256_pt) != 0) {
        printf("ERROR: AES-256 DECRYPT FAILED\n");
        fails++;
    } else {
        printf("pass\n");
        passes++;
    }

    printf("\n -- Asynchronous API --\n");

    /* Enable interrupt for AES, so we'll vector to our handler when operation complete */
    NVIC_EnableIRQ(AES_IRQn);

    /* Encrypt 128-bit */
    printf("Running 128-bit encrypt .. ");
    if (aes_check_async(MXC_E_AES_MODE_128, MXC_E_AES_ENCRYPT_ASYNC, nist_enc_128_key, nist_enc_128_pt, nist_enc_128_ct) != 0) {
        printf("ERROR: AES-128 ENCRYPT FAILED\n");
        fails++;
    } else {
        printf("pass\n");
        passes++;
    }

    /* Decrypt 128-bit */
    printf("Running 128-bit decrypt .. ");
    if (aes_check_async(MXC_E_AES_MODE_128, MXC_E_AES_DECRYPT_ASYNC, nist_dec_128_key, nist_dec_128_ct, nist_dec_128_pt) != 0) {
        printf("ERROR: AES-128 DECRYPT FAILED\n");
        fails++;
    } else {
        printf("pass\n");
        passes++;
    }

    /* Encrypt 192-bit */
    printf("Running 192-bit encrypt .. ");
    if (aes_check_async(MXC_E_AES_MODE_192, MXC_E_AES_ENCRYPT_ASYNC, nist_enc_192_key, nist_enc_192_pt, nist_enc_192_ct) != 0) {
        printf("ERROR: AES-192 ENCRYPT FAILED\n");
        fails++;
    } else {
        printf("pass\n");
        passes++;
    }

    /* Decrypt 192-bit */
    printf("Running 192-bit decrypt .. ");
    if (aes_check_async(MXC_E_AES_MODE_192, MXC_E_AES_DECRYPT_ASYNC, nist_dec_192_key, nist_dec_192_ct, nist_dec_192_pt) != 0) {
        printf("ERROR: AES-192 DECRYPT FAILED\n");
        fails++;
    } else {
        printf("pass\n");
        passes++;
    }

    /* Encrypt 256-bit */
    printf("Running 256-bit encrypt .. ");
    if (aes_check_async(MXC_E_AES_MODE_256, MXC_E_AES_ENCRYPT_ASYNC, nist_enc_256_key, nist_enc_256_pt, nist_enc_256_ct) != 0) {
        printf("ERROR: AES-256 ENCRYPT FAILED\n");
        fails++;
    } else {
        printf("pass\n");
        passes++;
    }

    /* Decrypt 256-bit */
    printf("Running 256-bit decrypt .. ");
    if (aes_check_async(MXC_E_AES_MODE_256, MXC_E_AES_DECRYPT_ASYNC, nist_dec_256_key, nist_dec_256_ct, nist_dec_256_pt) != 0) {
        printf("ERROR: AES-256 DECRYPT FAILED\n");
        fails++;
    } else {
        printf("pass\n");
        passes++;
    }

    printf("\nTests complete: %u tests passed, %u tests failed\n", passes, fails);

    while(1) {}
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
        if (AES_ECBEncrypt(in, ct1, mode) != E_SUCCESS) return -1;
        if (AES_ECBEncrypt(ct1, ct2, mode) != E_SUCCESS) return -1;
    } else {
        if (AES_ECBDecrypt(in, ct1, mode) != E_SUCCESS) return -1;
        if (AES_ECBDecrypt(ct1, ct2, mode) != E_SUCCESS) return -1;
    }

    /* Repeat this process 998 more times, result left in ct2 at end */
    for (i = 0; i < 499; i++) {
        if (dir == MXC_E_AES_ENCRYPT) {
            if (AES_ECBEncrypt(ct2, ct1, mode) != E_SUCCESS) return -1;
            if (AES_ECBEncrypt(ct1, ct2, mode) != E_SUCCESS) return -1;
        } else {
            if (AES_ECBDecrypt(ct2, ct1, mode) != E_SUCCESS) return -1;
            if (AES_ECBDecrypt(ct1, ct2, mode) != E_SUCCESS) return -1;
        }
    }

    /* Check result */
    if (memcmp(ct2, expected, MXC_AES_DATA_LEN) == 0) {
        return 0;
    } else {
        return -1;
    }
}

void AES_IRQHandler(void)
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
        if (AES_ECBEncryptAsync(in, mode) != E_SUCCESS) return -1;
    } else {
        if (AES_ECBDecryptAsync(in, mode) != E_SUCCESS) return -1;
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
            if (AES_ECBEncryptAsync(async_work, mode) != E_SUCCESS) return -1;
        } else {
            if (AES_ECBDecryptAsync(async_work, mode) != E_SUCCESS) return -1;
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
}
