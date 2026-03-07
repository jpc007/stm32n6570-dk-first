/**
 * @file lv_conf.h
 * Configuration LVGL pour AppliNonSecure - STM32N6570-DK, écran parallèle LTDC 800×480
 */

#ifndef LV_CONF_H
#define LV_CONF_H

#ifndef __ASSEMBLY__
#include <stdint.h>
#endif

/*====================
   COLOR SETTINGS
 *====================*/
#define LV_COLOR_DEPTH 16

/*====================
   DRAW ASM SETTINGS
 *====================*/
#define LV_USE_DRAW_SW_ASM  LV_DRAW_SW_ASM_NONE

/*=========================
   STDLIB WRAPPER SETTINGS
 *=========================*/
#define LV_USE_STDLIB_MALLOC    LV_STDLIB_BUILTIN
#define LV_USE_STDLIB_STRING   LV_STDLIB_BUILTIN
#define LV_USE_STDLIB_SPRINTF  LV_STDLIB_BUILTIN

#define LV_STDINT_INCLUDE      <stdint.h>
#define LV_STDDEF_INCLUDE      <stddef.h>
#define LV_STDBOOL_INCLUDE     <stdbool.h>
#define LV_INTTYPES_INCLUDE    <inttypes.h>
#define LV_LIMITS_INCLUDE      <limits.h>
#define LV_STDARG_INCLUDE      <stdarg.h>

#if LV_USE_STDLIB_MALLOC == LV_STDLIB_BUILTIN
    #define LV_MEM_SIZE (64 * 1024U)
    #define LV_MEM_POOL_EXPAND_SIZE 0
    #define LV_MEM_ADR 0
#endif

/*====================
   HAL SETTINGS
 *====================*/
#define LV_DEF_REFR_PERIOD  33
#define LV_DPI_DEF 130

/*=================
 * OPERATING SYSTEM
 *=================*/
#define LV_USE_OS  LV_OS_NONE

/*==================
 * WIDGETS (essentiels pour démarrer)
 *==================*/
#define LV_USE_LABEL  1
#define LV_USE_BTN   1
#define LV_USE_IMG   1
#define LV_USE_ARC   1
#define LV_USE_BAR   1
#define LV_USE_SLIDER 1
#define LV_USE_CHECKBOX 1
#define LV_USE_DROPDOWN 1
#define LV_USE_LIST  1
#define LV_USE_MENU  1
#define LV_USE_MSGBOX 1
#define LV_USE_TABVIEW 1
#define LV_USE_TEXTAREA 1
#define LV_USE_KEYBOARD 1

/*==================
 * DRIVERS - ST LTDC (écran parallèle STM32N6570-DK)
 *==================*/
/* Le HAL STM32N6 ne fournit pas le driver HAL LTDC classique ;
   on pilote le LTDC directement via CMSIS dans lv_port_disp_ltdc.c */
#define LV_USE_ST_LTDC  0

/* Désactiver démos pour réduire taille */
#define LV_BUILD_EXAMPLES 0
#define LV_BUILD_DEMOS 0

#endif /* LV_CONF_H */
