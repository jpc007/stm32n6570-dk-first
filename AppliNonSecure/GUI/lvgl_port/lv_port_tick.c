/**
 * @file lv_port_tick.c
 * @brief Port tick LVGL pour STM32N6570-DK - appel lv_tick_inc depuis SysTick
 */

#include "lv_port_tick.h"
#include "lvgl.h"

void lv_port_tick_init(void)
{
    /* Le tick est géré par l’interruption SysTick ; appeler lv_port_tick_inc(1) depuis le handler. */
}

void lv_port_tick_inc(uint32_t tick_period)
{
    lv_tick_inc(tick_period);
}
