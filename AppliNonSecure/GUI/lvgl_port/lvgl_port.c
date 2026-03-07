/**
 * @file lvgl_port.c
 * @brief Facade du port LVGL - STM32N6570-DK
 */

#include "lvgl_port.h"
#include "lv_port_disp_ltdc.h"
#include "lv_port_tick.h"
#include "lvgl.h"

void lv_port_init(void)
{
    lv_init();
    lv_port_disp_init();
    lv_port_tick_init();
}
