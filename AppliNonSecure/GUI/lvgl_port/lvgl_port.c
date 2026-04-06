/**
 * @file lvgl_port.c
 * @brief Facade du port LVGL - STM32N6570-DK
 */

#include "lvgl_port.h"
#include "lv_port_disp_ltdc.h"
#include "lv_port_tick.h"
#include "lvgl.h"
#include "main.h"
#include "stm32n6xx_hal_uart.h"
#include <string.h>

extern UART_HandleTypeDef huart2;

static void lv_boot_trace(const char *msg)
{
    if (msg == NULL) {
        return;
    }
    (void)HAL_UART_Transmit(&huart2, (const uint8_t *)msg, (uint16_t)strlen(msg), 300);
}

void lv_port_init(void)
{
    lv_boot_trace("  trace: before lv_init()\r\n");
    lv_init();
    lv_boot_trace("  trace: after lv_init()\r\n");
    lv_port_disp_init();
    lv_boot_trace("  trace: after lv_port_disp_init()\r\n");
    lv_port_tick_init();
    lv_boot_trace("  trace: after lv_port_tick_init()\r\n");
}
