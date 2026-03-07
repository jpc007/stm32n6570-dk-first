/**
 * @file lvgl_port.h
 * @brief Facade du port LVGL pour STM32N6570-DK (écran parallèle LTDC)
 */

#ifndef LVGL_PORT_H
#define LVGL_PORT_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Initialise display + tick (à appeler après HAL_Init / clocks). */
void lv_port_init(void);

/** À appeler depuis SysTick (période 1 ms) ou timer. */
void lv_port_tick_inc(uint32_t tick_period);

#ifdef __cplusplus
}
#endif

#endif /* LVGL_PORT_H */
