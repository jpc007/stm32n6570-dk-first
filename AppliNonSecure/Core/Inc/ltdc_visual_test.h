/**
 * @file ltdc_visual_test.h
 * @brief Sequences de test visuel LTDC (sans LVGL) — STM32N6570-DK
 *
 * Mettre APP_LTDC_VISUAL_TEST a 0 dans main.h pour revenir a l'appli LVGL.
 */
#ifndef LTDC_VISUAL_TEST_H
#define LTDC_VISUAL_TEST_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Phase unique : couleurs pleines, bandes, fond BCCR sans layer, puis fin.
 * @param log  fonction type DBG_Print (UART), peut etre NULL
 * @param hold_ms duree d'affichage de chaque etape (ex. 3000)
 */
void ltdc_visual_test_run_sequence(void (*log)(const char *s), uint32_t hold_ms);

/**
 * Boucle simple : fait tourner R/V/B/blanc sur le FB (appeler depuis while(1)).
 * @param log peut etre NULL
 * @param step_ms periode entre deux changements
 */
void ltdc_visual_test_loop_cycle(void (*log)(const char *s), uint32_t step_ms);

#ifdef __cplusplus
}
#endif

#endif /* LTDC_VISUAL_TEST_H */
