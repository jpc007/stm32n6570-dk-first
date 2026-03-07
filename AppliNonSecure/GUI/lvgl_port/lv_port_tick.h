/**
 * @file lv_port_tick.h
 * @brief Port tick LVGL pour STM32N6570-DK (SysTick)
 */

#ifndef LV_PORT_TICK_H
#define LV_PORT_TICK_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void lv_port_tick_init(void);
void lv_port_tick_inc(uint32_t tick_period);

#ifdef __cplusplus
}
#endif

#endif /* LV_PORT_TICK_H */
