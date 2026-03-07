/**
 * @file lv_port_disp_ltdc.c
 * @brief Port display LVGL pour STM32N6570-DK - ecran parallele LTDC 800x480
 *
 * Le HAL STM32N6xx ne fournit pas de driver HAL LTDC classique
 * (pas de LTDC_HandleTypeDef, HAL_LTDC_Init, etc.).
 * On pilote donc le LTDC directement via les registres CMSIS.
 *
 * Framebuffer en SRAM interne (section .bss).
 * 800x480 RGB565 = 768 000 octets.
 */

#include "lv_port_disp_ltdc.h"
#include "main.h"
#include "lvgl.h"

/* ---------- Resolution ecran STM32N6570-DK ---------- */
#define DISP_HOR_RES   800
#define DISP_VER_RES   480

/* Timing LTDC pour le panneau RK050HR18-CTG (conforme BSP ST) */
#define HSYNC_W        4
#define HBP            4
#define HFP            4
#define VSYNC_W        4
#define VBP            4
#define VFP            4

/* Pixel format dans le registre PFCR : 010 = RGB565 */
#define LTDC_PF_RGB565  2U

/* Taille d'un pixel en octets (RGB565) */
#define BYTES_PER_PX   2U

/* ---------- Framebuffer in dedicated SRAM (SRAM3/SRAM4) ---------- */
static uint16_t framebuffer[DISP_HOR_RES * DISP_VER_RES]
    __attribute__((aligned(64), section(".framebuffer")));

/* ---------- Render buffers pour LVGL (mode partiel) ---------- */
#define RENDER_LINES   20
static uint8_t render_buf1[DISP_HOR_RES * RENDER_LINES * BYTES_PER_PX]
    __attribute__((aligned(4)));
static uint8_t render_buf2[DISP_HOR_RES * RENDER_LINES * BYTES_PER_PX]
    __attribute__((aligned(4)));

/* ---------- Forward declarations ---------- */
static void ltdc_hw_init(void);
static void disp_flush_cb(lv_display_t * disp, const lv_area_t * area,
                           uint8_t * px_map);

/* ============================================================
 *  Public API
 * ============================================================ */
void lv_port_disp_init(void)
{
    ltdc_hw_init();

    lv_display_t * disp = lv_display_create(DISP_HOR_RES, DISP_VER_RES);
    lv_display_set_flush_cb(disp, disp_flush_cb);
    lv_display_set_buffers(disp, render_buf1, render_buf2,
                           sizeof(render_buf1), LV_DISPLAY_RENDER_MODE_PARTIAL);
    lv_display_set_color_format(disp, LV_COLOR_FORMAT_RGB565);
}

/* ============================================================
 *  Flush callback : copie les pixels rendus dans le framebuffer
 * ============================================================ */
static void disp_flush_cb(lv_display_t * disp, const lv_area_t * area,
                           uint8_t * px_map)
{
    uint16_t * src = (uint16_t *)px_map;
    int32_t w = lv_area_get_width(area);

    for(int32_t y = area->y1; y <= area->y2; y++) {
        uint16_t * dst = &framebuffer[y * DISP_HOR_RES + area->x1];
        for(int32_t x = 0; x < w; x++) {
            dst[x] = src[x];
        }
        src += w;
    }

    lv_display_flush_ready(disp);
}

/* ============================================================
 *  LTDC hardware init via registres CMSIS
 * ============================================================ */
static void ltdc_hw_init(void)
{
    /* 0. Activer les horloges SRAM3/SRAM4 (framebuffer) */
    RCC->MEMENR |= RCC_MEMENR_AXISRAM3EN | RCC_MEMENR_AXISRAM4EN;
    __DSB();

    /* 0b. Configurer l'horloge pixel LTDC : IC16 depuis PLL4 (1600 MHz VCO)
     *     diviser par 64 → 25 MHz pixel clock (800x480 @ ~60 Hz)
     *     NOTE : normalement deja configure par le FSBL secure.
     *     Ces ecritures NS sont un fallback ; elles seront ignorees si
     *     les registres RCC IC sont marques secure. */
    MODIFY_REG(RCC->IC16CFGR,
               RCC_IC16CFGR_IC16SEL | RCC_IC16CFGR_IC16INT,
               (3U  << RCC_IC16CFGR_IC16SEL_Pos) |   /* source = PLL4 */
               (63U << RCC_IC16CFGR_IC16INT_Pos));    /* divider-1 = 63 → /64 */
    WRITE_REG(RCC->DIVENSR, RCC_DIVENSR_IC16ENS);     /* activer IC16 */
    __DSB();
    MODIFY_REG(RCC->CCIPR4, RCC_CCIPR4_LTDCSEL,
               RCC_CCIPR4_LTDCSEL_1);                 /* LTDCSEL = 10 = IC16 */

    /* 1. Activer l'horloge LTDC (APB5) */
    RCC->APB5ENR |= RCC_APB5ENR_LTDCEN;
    __DSB();

    LTDC_TypeDef         * ltdc  = LTDC_NS;
    LTDC_Layer_TypeDef   * layer = LTDC_Layer1_NS;

    /* 2. Desactiver le LTDC pendant la configuration */
    ltdc->GCR &= ~LTDC_GCR_LTDCEN;

    /* 3. Timing sync
     *    SSCR : HSW-1 | VSH-1
     *    BPCR : Accum HBP | Accum VBP
     *    AWCR : Accum Active W | Accum Active H
     *    TWCR : Total W | Total H
     */
    uint32_t accum_hbp = HSYNC_W + HBP - 1;
    uint32_t accum_vbp = VSYNC_W + VBP - 1;
    uint32_t accum_aw  = HSYNC_W + HBP + DISP_HOR_RES - 1;
    uint32_t accum_ah  = VSYNC_W + VBP + DISP_VER_RES - 1;
    uint32_t total_w   = HSYNC_W + HBP + DISP_HOR_RES + HFP - 1;
    uint32_t total_h   = VSYNC_W + VBP + DISP_VER_RES + VFP - 1;

    ltdc->SSCR = ((HSYNC_W - 1) << LTDC_SSCR_HSW_Pos) |
                 ((VSYNC_W - 1) << LTDC_SSCR_VSH_Pos);
    ltdc->BPCR = (accum_hbp << LTDC_BPCR_AHBP_Pos) |
                 (accum_vbp << LTDC_BPCR_AVBP_Pos);
    ltdc->AWCR = (accum_aw << LTDC_AWCR_AAW_Pos) |
                 (accum_ah << LTDC_AWCR_AAH_Pos);
    ltdc->TWCR = (total_w << LTDC_TWCR_TOTALW_Pos) |
                 (total_h << LTDC_TWCR_TOTALH_Pos);

    /* 4. Couleur de fond noire */
    ltdc->BCCR = 0x00000000;

    /* 5. Configurer le Layer 1
     *    WHPCR : fenetre horizontale [start .. stop]
     *    WVPCR : fenetre verticale
     *    PFCR  : pixel format RGB565
     *    CACR  : alpha constant = 255 (opaque)
     *    BFCR  : blending factors (pixel alpha x constant alpha)
     *    CFBAR : adresse framebuffer
     *    CFBLR : longueur ligne en octets + pitch
     *    CFBLNR: nombre de lignes
     */
    uint32_t h_start = accum_hbp + 1;
    uint32_t h_stop  = accum_hbp + DISP_HOR_RES;
    uint32_t v_start = accum_vbp + 1;
    uint32_t v_stop  = accum_vbp + DISP_VER_RES;

    layer->WHPCR = (h_start << 0) | (h_stop << 16);
    layer->WVPCR = (v_start << 0) | (v_stop << 16);
    layer->PFCR  = LTDC_PF_RGB565;
    layer->CACR  = 0xFF;
    layer->DCCR  = 0x00000000;  /* Default color = transparent black */
    layer->BFCR  = (0x04 << 0) | (0x05 << 8);  /* BF2=1-CA, BF1=CA */

    layer->CFBAR  = (uint32_t)framebuffer;
    layer->CFBLR  = ((DISP_HOR_RES * BYTES_PER_PX) << 16) |
                    ((DISP_HOR_RES * BYTES_PER_PX) + 7);
    layer->CFBLNR = DISP_VER_RES;

    /* 6. Activer le layer */
    layer->CR |= 0x01;  /* LEN = 1 */

    /* 7. Reload immediat des shadow registers */
    ltdc->SRCR = LTDC_SRCR_IMR;

    /* 8. Activer le LTDC */
    ltdc->GCR |= LTDC_GCR_LTDCEN;
}
