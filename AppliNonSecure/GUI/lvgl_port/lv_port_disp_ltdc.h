/**
 * @file lv_port_disp_ltdc.h
 * @brief Port display LVGL pour STM32N6570-DK - écran parallèle LTDC (800×480)
 */

#ifndef LV_PORT_DISP_LTDC_H
#define LV_PORT_DISP_LTDC_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void lv_port_disp_init(void);

/**
 * Nettoie le D-cache sur une zone memoire (obligatoire avant que le LTDC lise le FB).
 * @param addr   debut (alignement gere par CMSIS)
 * @param nbytes taille en octets
 */
void lv_port_ltdc_flush_fb_range(const void *addr, uint32_t nbytes);

#ifdef __cplusplus
}
#endif

#endif /* LV_PORT_DISP_LTDC_H */
