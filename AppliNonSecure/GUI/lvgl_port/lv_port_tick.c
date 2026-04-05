/**
 * @file lv_port_tick.c
 * @brief Port tick LVGL pour STM32N6570-DK - appel lv_tick_inc depuis SysTick
 */

#include "lv_port_tick.h"
#include "lvgl.h"

static volatile uint8_t tick_enabled = 0;

void lv_port_tick_init(void)
{
    tick_enabled = 1;
}

void lv_port_tick_inc(uint32_t tick_period)
{
    if (tick_enabled) {
        lv_tick_inc(tick_period);
    }
}
