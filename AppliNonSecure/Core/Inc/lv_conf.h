/**
 * @file lv_conf.h
 * Configuration LVGL 9 — STM32N6570-DK, LTDC 800×480
 *
 * Utiliser les noms LVGL 9 (LV_USE_BUTTON, etc.). Les anciennes macros
 * (LV_USE_BTN, LV_USE_IMG) sont ignorees : sans ca, lv_conf_internal.h
 * activait tous les widgets par defaut → stack / init tres lourds.
 */

#ifndef LV_CONF_H
#define LV_CONF_H

#ifndef __ASSEMBLY__
#include <stdint.h>
#endif

#define LV_COLOR_DEPTH 16

#define LV_USE_DRAW_SW_ASM  LV_DRAW_SW_ASM_NONE

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
    #define LV_MEM_SIZE (96 * 1024U)
    #define LV_MEM_POOL_EXPAND_SIZE 0
    #define LV_MEM_ADR 0
#endif

#define LV_DEF_REFR_PERIOD  33
#define LV_DPI_DEF 130

#define LV_USE_OS  LV_OS_NONE

/* ---------- Widgets : minimum pour bouton + label (Hello LVGL) ---------- */
#define LV_WIDGETS_HAS_DEFAULT_VALUE  1

#define LV_USE_ANIMIMG    0
#define LV_USE_ARC        0
#define LV_USE_BAR        0
#define LV_USE_BUTTON     1
#define LV_USE_BUTTONMATRIX 0
#define LV_USE_CALENDAR   0
#define LV_USE_CANVAS     0
#define LV_USE_CHART      0
#define LV_USE_CHECKBOX   0
#define LV_USE_DROPDOWN   0
#define LV_USE_IMAGE      1
#define LV_USE_IMAGEBUTTON 0
#define LV_USE_KEYBOARD   0
#define LV_USE_LABEL      1
#if LV_USE_LABEL
    #define LV_LABEL_TEXT_SELECTION 0
    #define LV_LABEL_LONG_TXT_HINT 1
    #define LV_LABEL_WAIT_CHAR_COUNT 3
#endif
#define LV_USE_LED        0
#define LV_USE_LINE       0
#define LV_USE_LIST       0
#define LV_USE_MENU       0
#define LV_USE_MSGBOX     0
#define LV_USE_ROLLER     0
#define LV_USE_SCALE      0
#define LV_USE_SLIDER     0
#define LV_USE_SPAN       0
#define LV_USE_SPINBOX    0
#define LV_USE_SPINNER    0
#define LV_USE_SWITCH     0
#define LV_USE_TEXTAREA   0
#define LV_USE_TABLE      0
#define LV_USE_TABVIEW    0
#define LV_USE_TILEVIEW   0
#define LV_USE_WIN        0

#define LV_USE_ST_LTDC  0

#define LV_BUILD_EXAMPLES 0
#define LV_BUILD_DEMOS 0

#endif /* LV_CONF_H */
