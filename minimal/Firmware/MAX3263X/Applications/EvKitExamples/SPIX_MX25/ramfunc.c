
#include "mxc_config.h"
#include "clkman.h"
#include "ioman.h"
#include "spim.h"
#include "mx25.h"
#include "led.h"
#include "spix.h"

/******************************************************************************/
__attribute__ ((section(".xip_section"))) void xip_function(void)
{
    volatile int i;
    int j;

    for(j = 0; j < 10; j++) {
        MXC_GPIO->out_val[led_pin[0].port] &= ~led_pin[0].mask;
        for(i = 0; i < 0xFFFFF; i++);
        MXC_GPIO->out_val[led_pin[0].port] |= led_pin[0].mask;
        for(i = 0; i < 0xFFFFF; i++);
    }
}
