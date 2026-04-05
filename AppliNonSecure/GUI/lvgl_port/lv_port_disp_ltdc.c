/**
 * @file lv_port_disp_ltdc.c
 * @brief Port display LVGL pour STM32N6570-DK - ecran parallele LTDC 800x480
 *
 * Le HAL STM32N6xx ne fournit pas de driver HAL LTDC classique
 * (pas de LTDC_HandleTypeDef, HAL_LTDC_Init, etc.).
 * On pilote donc le LTDC directement via les registres CMSIS.
 *
 * Framebuffer en SRAM3 (section .framebuffer / FBRAM : voir .ld — SRAM1 trop petite en NS).
 * 800x480 RGB565 = 768 000 octets.
 */

#include "lv_port_disp_ltdc.h"
#include "framebuffer_ns.h"
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

/* ---------- Framebuffer : section .framebuffer (SRAM3, voir .ld) ---------- */
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
/** Init LTDC seulement (sans LVGL) — tests visuels / diagnostic affichage. */
void lv_port_ltdc_hw_init_only(void)
{
    ltdc_hw_init();
}

void lv_port_ltdc_flush_fb_range(const void *addr, uint32_t nbytes)
{
#if defined(__DCACHE_PRESENT) && (__DCACHE_PRESENT == 1U)
    if(addr != NULL && nbytes > 0U) {
        /* Clean+Invalidate : le LTDC lit la RAM hors cache CPU. */
        SCB_CleanInvalidateDCache_by_Addr((volatile void *)addr, (int32_t)nbytes);
        __DSB();
    }
#else
    (void)addr;
    (void)nbytes;
#endif
}

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

    /* LTDC lit la RAM : vidage cache D obligatoire (CM55 + SRAM cacheable). */
    lv_port_ltdc_flush_fb_range(framebuffer, ltdc_fb_size_bytes());

    lv_display_flush_ready(disp);
}

/* ============================================================
 *  LTDC hardware init via registres CMSIS
 * ============================================================ */
static void ltdc_hw_init(void)
{
    /* 0. Activer les horloges SRAM3/SRAM4 (framebuffer)
     *    MEMENSR est le SET register (write-1-to-set pour MEMENR) */
    RCC->MEMENSR = RCC_MEMENR_AXISRAM3EN | RCC_MEMENR_AXISRAM4EN;
    __DSB();

    /* NOTE: IC16 pixel clock (PLL1/45 = 26.67 MHz), CCIPR4 LTDCSEL, and
     * APB5 LTDC clock are configured by AppliSecure (Secure context required
     * for these RCC registers — NS writes are silently ignored). */

    LTDC_TypeDef         * ltdc  = LTDC_NS;
    /* BSP N6570-DK : MX_LTDC_ConfigLayer(..., 0, ...) = premier layer HAL = souvent L2 materiel. */
    LTDC_Layer_TypeDef   * layer = LTDC_Layer2_NS;

    /* 2. Desactiver le LTDC ; GCR a zero (evite residus FSBL). */
    ltdc->GCR &= ~LTDC_GCR_LTDCEN;
    __DSB();
    ltdc->GCR = 0U;
    __DSB();

    /* L1 coupe — on affiche sur L2 uniquement (evite conflit / mauvais master AXI). */
    {
        LTDC_Layer_TypeDef *l1 = LTDC_Layer1_NS;
        l1->CR    = 0U;
        l1->CFBAR = 0U;
        l1->BFCR  = 0U;
    }
    {
        LTDC_Layer_TypeDef *l2 = LTDC_Layer2_NS;
        l2->CR    = 0U;
        l2->CFBAR = 0U;
        l2->BFCR  = 0U;
    }

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

    /* 4. PCPOL + BCKEN : le fond BCCR participe au melange avec le layer (N6 / RK050). */
    ltdc->GCR = LTDC_GCR_PCPOL | LTDC_GCR_BCKEN;

    /* Couleur de fond (RGB888 dans BCCR) */
    ltdc->BCCR = 0x00000000;

    /* 5. Layer 2 : memes timings que BSP stm32n6570_discovery_lcd (RK050HR18).
     *    BFCR PAxCA comme MX_LTDC_ConfigLayer (HAL) — avec RGB565 l'alpha pixel est implicite. */
    uint32_t h_start = accum_hbp + 1;
    uint32_t h_stop  = accum_hbp + DISP_HOR_RES;
    uint32_t v_start = accum_vbp + 1;
    uint32_t v_stop  = accum_vbp + DISP_VER_RES;

    layer->WHPCR = (h_start << 0) | (h_stop << 16);
    layer->WVPCR = (v_start << 0) | (v_stop << 16);
    layer->PFCR  = LTDC_PF_RGB565;
    layer->CACR  = 0xFF;
    layer->DCCR  = 0x00000000U;
    /* BF1=110 BF2=111 : PAxCA (BSP officiel N6570-DK). */
    layer->BFCR  = (0x06U << LTDC_LxBFCR_BF1_Pos) | (0x07U << LTDC_LxBFCR_BF2_Pos);

    /* CFBAR = &_fb_start : symbole linker (meme zone que le tableau .framebuffer). */
    layer->CFBAR  = (uint32_t)(uintptr_t)&_fb_start;
    layer->CFBLR  = ((DISP_HOR_RES * BYTES_PER_PX) << 16) |
                    ((DISP_HOR_RES * BYTES_PER_PX) + 7);
    layer->CFBLNR = DISP_VER_RES;

    /* 6. Activer le layer uniquement (LEN) */
    layer->CR = LTDC_LxCR_LEN;

    /* 7. Activer le LTDC puis reload ombre (sequence type HAL Init + ConfigLayer) */
    ltdc->GCR |= LTDC_GCR_LTDCEN;

    /* 8. Reload per-layer immediat (approche HAL ST officielle pour N6)
     *    Le HAL N6 n'utilise JAMAIS le reload global (SRCR.IMR).
     *    Il garde GRMSK positionne et declenche un reload per-layer via
     *    RCR.IMR (bit 0).  RCR = IMR(bit0) | GRMSK(bit2) = 0x05 */
    layer->RCR = (1U << 0) | (1U << 2);  /* IMR | GRMSK : per-layer reload */
}
