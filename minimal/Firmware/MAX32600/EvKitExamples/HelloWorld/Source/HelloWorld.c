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
 ********************************************************************************/

/* $Revision$ $Date$ */

/* config.h is the required application configuration; RAM layout, stack, chip type etc. */
#include "mxc_config.h" 
#include "board.h"

#include "clkman.h"
#include "ioman.h"
#include "power.h"
#include "gpio.h"
#include "icc.h"
#include "lcd.h"

#include "systick.h"

/* SysTick will be tied to 32KHz RTC clock */
#define SYSTICK_PERIOD  32 /* make about 0.98ms timeout (32.768KHz/32 = 1024Hz) */

/* The LCD defines for the PD878 LCD Screen */
#define PD878_SEGMENTS  32
#define PD878_GND_ENABLE 1
#define PD878_FRM_VALUE 16
#define PD878_DUTY_CYCLE LCD_DUTY_25
#define LCD_MAX_LENGTH 8

static const char message[] = "         Hello World! MAX32600 Ev Kit ...       ";
static uint8_t led_mode = 0;

/* This array is an ASCII to LCD char mapping, starting at ascii 0x20, such that
 * offset  0 => ascii 0x20 or ' ' (space),·
 * offset 48 => ascii 0x30 or '1' ...·
 * values of 0xffff are invalid or undisplayable on this LCD.
 */
static const uint16_t lcd_value[] =
{
    0x0000, 0xFFFF, 0x00C0, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0x0008, 0x0050, 0xA000,
    0xF0F0, 0x50A0, 0x2000, 0x4020, 0x0100, 0x2040, // *+,-./
    0x2E47, 0x0006, 0x4C23, 0x4E21, 0x50A4, 0x8A21, 0x4A27, 0x0E00, 0x4E27, 0x4e25, // 0-9
    0xFFFF, 0xFFFF, 0x0050, 0xFFFF, 0xA000, 0xFFFF, 0xFFFF,
    0x4E26, 0x4857, 0x0807, 0x1E81, 0x4827, 0x4826, 0x0A37, 0x4626, 0x1881, 0x0603, // A-J
    0x10D0, 0x0007, 0x8646, 0x8616, 0x0E07, 0x4C26, 0x0E17, 0x4C36, 0x4A25, 0x1880, // K-T
    0x0607, 0x8610, 0x2616, 0xA050, 0x9040, 0x2841,           // U-Z
    0x0C01, 0x8010, 0x0807, 0xFFFF, 0x8080, 0x0100,
    0x4E26, 0x4857, 0x0807, 0x1E81, 0x4827, 0x4826, 0x0A37, 0x4626, 0x1881, 0x0603, // a-j
    0x10D0, 0x0007, 0x8646, 0x8616, 0x0E07, 0x4C26, 0x0E17, 0x4C36, 0x4A25, 0x1880, // k-t
    0x0607, 0x8610, 0x2616, 0xA050, 0x9040, 0x2841,           // u-z
    0xFFFF, 0x0090, 0xFFFF, 0xFFFF, 0xFFFF
};  

/**
 * This function displays a single character on the LCD at the position
 * given.
 */
static int32_t lcd_display_char(uint8_t position, uint8_t ch)
{
    uint8_t index = ch - 0x20;
    uint8_t high_byte, low_byte;

    if (((ch >= 0x20) && (ch <= 0x7F)) && (position < 8 ))
    {
        if (index > (sizeof(lcd_value) >> 1)) /* Out of range */
        {
            return 1;
        }
        else if (lcd_value[index] == 0xFFFF ) /* Unsuported character */
        {
            return 1;
        }

        high_byte = (lcd_value[index] >> 8) & 0xFF;
        low_byte = lcd_value[index] & 0xFF;

        LCD_Write(position, low_byte);
        LCD_Write((15-position), high_byte);

        return 0;
    }
    else
    {
        return 1;
    }
}

/**
 * Initializes the three LCD LED's on the MAX32600 EV Kit
 */
void blinky_init(void)
{
    /* setup GPIO for the LED */
    GPIO_SetOutMode(GREEN_LED_PORT, GREEN_LED_PIN, MXC_E_GPIO_OUT_MODE_OPEN_DRAIN_W_PULLUP);
    GPIO_SetOutMode(YELLOW_LED_PORT, YELLOW_LED_PIN, MXC_E_GPIO_OUT_MODE_OPEN_DRAIN_W_PULLUP);
    GPIO_SetOutMode(RED_LED_PORT, RED_LED_PIN, MXC_E_GPIO_OUT_MODE_OPEN_DRAIN_W_PULLUP);
    GPIO_SetOutVal(GREEN_LED_PORT, GREEN_LED_PIN, LED_ON);
    GPIO_SetOutVal(YELLOW_LED_PORT, YELLOW_LED_PIN, LED_OFF);
    GPIO_SetOutVal(RED_LED_PORT, RED_LED_PIN, LED_OFF);
}

/**
 * Setups the LCD Drivers to work with the PD878 LCD Screen
 */
void LCD_InitPd878(void)
{
    IOMAN_LCD(1,0xffffffff, 0);
    LCD_Init(PD878_SEGMENTS, PD878_GND_ENABLE, PD878_FRM_VALUE, PD878_DUTY_CYCLE, 
            lcd_display_char, LCD_MAX_LENGTH);
    LCD_Enable();
    LCD_Clear();

}

/* Interrupt handler which changes the state based upon Test push button */
static void change_led_mode(void)
{
    if(GPIO_GetInVal(SW1_PORT, SW1_PIN))
    {
        led_mode = ~led_mode;
    }
}

/*
 * Main function of occurring things, modifies state of LCD and LED's
 * Called by the systick interrupt.
 */
void change_state(void)
{
    static uint8_t led_state = 0;
    static char* position = (char*)message;

    /* Scroll through the message and restart once we hit the end */
    ++position;
    if(*position == '\0')
    {
        position = (char*)message;
    }

    LCD_Display((uint8_t *) position);
    LCD_Update();

    /* If in first mode alternate lights up and down */
    if(!led_mode){
        GPIO_SetOutVal(GREEN_LED_PORT, GREEN_LED_PIN, LED_OFF);
        GPIO_SetOutVal(YELLOW_LED_PORT, YELLOW_LED_PIN, LED_OFF);
        GPIO_SetOutVal(RED_LED_PORT, RED_LED_PIN, LED_OFF);

        switch(led_state)
        {
            case 0:
                GPIO_SetOutVal(GREEN_LED_PORT, GREEN_LED_PIN, LED_ON);
                led_state = 1;
                break;
            case 1:
                GPIO_SetOutVal(RED_LED_PORT, RED_LED_PIN, LED_ON);
                led_state = 2;
                break;
            case 2:
                GPIO_SetOutVal(YELLOW_LED_PORT, YELLOW_LED_PIN, LED_ON);
                led_state = 3;
                break;
            case 3:
                GPIO_SetOutVal(RED_LED_PORT, RED_LED_PIN, LED_ON);
                led_state = 0;
                break;
        }
    }
    else
    {
        /* If in second mode toggle two lights together */
        if(led_state)
        {
            GPIO_SetOutVal(GREEN_LED_PORT, GREEN_LED_PIN, LED_OFF);
            GPIO_SetOutVal(YELLOW_LED_PORT, YELLOW_LED_PIN, LED_OFF);
            GPIO_SetOutVal(RED_LED_PORT, RED_LED_PIN, LED_ON);
            led_state = 0;
        }
        else
        {
            GPIO_SetOutVal(GREEN_LED_PORT, GREEN_LED_PIN, LED_ON);
            GPIO_SetOutVal(YELLOW_LED_PORT, YELLOW_LED_PIN, LED_ON);
            GPIO_SetOutVal(RED_LED_PORT, RED_LED_PIN, LED_OFF);
            led_state = 1;
        }
    }
}

/* called in interrupt context */
static void systick_handler(uint32_t ticks)
{
    /* toggle state every 256 ticks (1024Hz/256 => 0.25 sec) for a 4Hz blink */
    if((ticks & 255) == 0)
    {
        change_state();
    }
}

#ifndef TOP_MAIN
int main(void)
{
    /* enable instruction cache */
    ICC_Enable();

    /* use the internal Ring Osc in 24MHz mode */
    CLKMAN_SetSystemClock(MXC_E_CLKMAN_SYSTEM_SOURCE_SELECT_24MHZ_RO);

    /* Setup the charge pump so the display acts normally */
    CLKMAN_SetClkScale(MXC_E_CLKMAN_CLK_LCD_CHPUMP, MXC_E_CLKMAN_CLK_SCALE_ENABLED);

    /* Setup the GPIO clock so the push button interrupt works */
    CLKMAN_SetClkScale(MXC_E_CLKMAN_CLK_GPIO, MXC_E_CLKMAN_CLK_SCALE_ENABLED);

    /* set systick to the RTC input 32.768KHz clock, not system clock; this is needed to keep JTAG alive */
    CLKMAN_SetRTOSMode(TRUE);

    /* enable real-time clock during run mode, this is needed to drive the systick with the RTC crystal */
    PWR_EnableDevRun(MXC_E_PWR_DEVICE_RTC);

    /* setup LED */
    blinky_init();

    /* Change LED state based upon button press through interrupt */
    GPIO_SetInMode(SW1_PORT, SW1_PIN, MXC_E_GPIO_IN_MODE_INVERTED);
    GPIO_SetIntMode(SW1_PORT, SW1_PIN, MXC_E_GPIO_INT_MODE_ANY_EDGE, change_led_mode);

    /* Setup the PD878 LCD Display */
    LCD_InitPd878();

    /* tie interrupt handler to systick */
    SysTick_Config(SYSTICK_PERIOD, systick_handler);

    for(;;) {
        /* default sleep mode is "LP2"; core powered up, ARM in "Wait For Interrupt" mode */
        PWR_Sleep();
    }
    return 0;
}
#endif /* TOP_MAIN */
