#include <unistd.h>
#include "py/mpconfig.h"

#include "mxc_config.h"
#include "uart.h"
#include "board.h"

/*
 * Core UART functions to implement for a port
 */

#define MXC_UARTn   MXC_UART_GET_UART(CONSOLE_UART)
#define UART_FIFO   MXC_UART_GET_FIFO(CONSOLE_UART)

static void UART_PutChar(const uint8_t data)
{
    // Wait for TXFIFO to not be full 
    while ((MXC_UARTn->tx_fifo_ctrl & MXC_F_UART_TX_FIFO_CTRL_FIFO_ENTRY) == MXC_F_UART_TX_FIFO_CTRL_FIFO_ENTRY);
        MXC_UARTn->intfl = MXC_F_UART_INTFL_TX_DONE; // clear DONE flag for UART_PrepForSleep
        UART_FIFO->tx = data;
}

static uint8_t UART_GetChar(void)
{
    while (!(MXC_UARTn->rx_fifo_ctrl & MXC_F_UART_RX_FIFO_CTRL_FIFO_ENTRY));
        return UART_FIFO->rx;
}

#if MICROPY_MIN_USE_STM32_MCU
typedef struct {
    volatile uint32_t SR;
    volatile uint32_t DR;
} periph_uart_t;
#define USART1 ((periph_uart_t*)0x40011000)
#endif

// Receive single character
int mp_hal_stdin_rx_chr(void) {
    unsigned char c = 0;
#if MICROPY_MIN_USE_STDOUT
    int r = read(0, &c, 1);
    (void)r;
#elif MICROPY_MIN_USE_STM32_MCU
    // wait for RXNE
    while ((USART1->SR & (1 << 5)) == 0) {
    }
    c = USART1->DR;
#elif MICROPY_MAX326XX_MCU
    c = UART_GetChar();
#endif
    return c;
}

// Send string of given length
void mp_hal_stdout_tx_strn(const char *str, mp_uint_t len) {
#if MICROPY_MIN_USE_STDOUT
    int r = write(1, str, len);
    (void)r;
#elif MICROPY_MIN_USE_STM32_MCU
    while (len--) {
        // wait for TXE
        while ((USART1->SR & (1 << 7)) == 0) {
        }
        USART1->DR = *str++;
    }
#elif MICROPY_MAX326XX_MCU
    while (len--) {
        UART_PutChar(*str++);
    }
#endif
}
