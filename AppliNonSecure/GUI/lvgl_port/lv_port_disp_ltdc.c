/**
 * @file lv_port_disp_ltdc.c
 * @brief LVGL display — LTDC aligne sur BSP ST STM32N6570-DK (RK050HR18).
 *
 * Le HAL LTDC N6 du package local est un stub vide : on programme le LTDC en CMSIS
 * avec les memes formules que stm32n6570_discovery_lcd.c (MX_LTDC_Init /
 * MX_LTDC_ConfigLayer) et les constantes officielles rk050hr18.h.
 *
 * GPIO : derive de LTDC_MspInit() BSP (broches RGB/HSYNC/VSYNC/CLK, PQ/PG13).
 * TrustZone : NE PAS appeler __HAL_RCC_LTDC_FORCE_RESET depuis NS — horloge + reset
 * LTDC sont deja faits dans AppliSecure/FSBL (RCC Secure). Un reset NS mal relache
 * laisse le LTDC mort : acces aux registres = bus bloque, ST-LINK perd la cible.
 *
 * Pixels : copie NSC vers le framebuffer Secure 0x34200000 (le LTDC ne lit pas l'alias NS 0x24200000).
 */

#include "lv_port_disp_ltdc.h"
#include "main.h"
#include "secure_nsc.h" /* SECURE_LtdcFbRect_t */
#include "lvgl.h"
#include "core/lv_refr.h"
#include "rk050hr18.h"
#include "stm32n6xx_hal_gpio.h"
#include "stm32n6xx_hal_uart.h"
#include <string.h>

extern UART_HandleTypeDef huart2;

static void ltdc_boot_trace(const char *msg)
{
    if (msg != NULL) {
        (void)HAL_UART_Transmit(&huart2, (const uint8_t *)msg, (uint16_t)strlen(msg), 300);
    }
}

#define DISP_HOR_RES   ((int32_t)RK050HR18_WIDTH)
#define DISP_VER_RES   ((int32_t)RK050HR18_HEIGHT)

#define LTDC_PF_RGB565  2U
#define BYTES_PER_PX    2U

#define RENDER_LINES   20
static uint8_t render_buf1[DISP_HOR_RES * RENDER_LINES * BYTES_PER_PX]
    __attribute__((aligned(4)));
static uint8_t render_buf2[DISP_HOR_RES * RENDER_LINES * BYTES_PER_PX]
    __attribute__((aligned(4)));

static void ltdc_st_bsp_msp_gpio_init(void);
static void ltdc_hw_init(void);
static void disp_flush_cb(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map);

/**
 * @brief GPIO LCD — broches comme LTDC_MspInit() BSP, sans reset RCC du LTDC (voir .c en-tete).
 */
static void ltdc_st_bsp_msp_gpio_init(void)
{
    GPIO_InitTypeDef gpio_init_structure = {0};

    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_GPIOD_CLK_ENABLE();
    __HAL_RCC_GPIOE_CLK_ENABLE();
    __HAL_RCC_GPIOG_CLK_ENABLE();
    __HAL_RCC_GPIOH_CLK_ENABLE();
    __HAL_RCC_GPIOQ_CLK_ENABLE();

    gpio_init_structure.Mode      = GPIO_MODE_AF_PP;
    gpio_init_structure.Pull      = GPIO_NOPULL;
    gpio_init_structure.Speed     = GPIO_SPEED_FREQ_HIGH;

    gpio_init_structure.Pin       = GPIO_PIN_0 | GPIO_PIN_1 | GPIO_PIN_2 | GPIO_PIN_7 | GPIO_PIN_8 | GPIO_PIN_15;
    gpio_init_structure.Alternate = GPIO_AF14_LCD;
    HAL_GPIO_Init(GPIOA, &gpio_init_structure);

    gpio_init_structure.Pin       = GPIO_PIN_13 | GPIO_PIN_14 | GPIO_PIN_2 | GPIO_PIN_4 | GPIO_PIN_11 | GPIO_PIN_12 | GPIO_PIN_15;
    gpio_init_structure.Alternate = GPIO_AF14_LCD;
    HAL_GPIO_Init(GPIOB, &gpio_init_structure);

    gpio_init_structure.Pin       = GPIO_PIN_8 | GPIO_PIN_9 | GPIO_PIN_15;
    gpio_init_structure.Alternate = GPIO_AF14_LCD;
    HAL_GPIO_Init(GPIOD, &gpio_init_structure);

    gpio_init_structure.Pin       = GPIO_PIN_11;
    gpio_init_structure.Alternate = GPIO_AF14_LCD;
    HAL_GPIO_Init(GPIOE, &gpio_init_structure);

    gpio_init_structure.Pin       = GPIO_PIN_0 | GPIO_PIN_1 | GPIO_PIN_6 | GPIO_PIN_8 | GPIO_PIN_11 | GPIO_PIN_12;
    gpio_init_structure.Alternate = GPIO_AF14_LCD;
    HAL_GPIO_Init(GPIOG, &gpio_init_structure);

    gpio_init_structure.Pin       = GPIO_PIN_3 | GPIO_PIN_4 | GPIO_PIN_6;
    gpio_init_structure.Alternate = GPIO_AF14_LCD;
    HAL_GPIO_Init(GPIOH, &gpio_init_structure);

    gpio_init_structure.Pin       = GPIO_PIN_1;
    gpio_init_structure.Mode      = GPIO_MODE_OUTPUT_PP;
    HAL_GPIO_Init(GPIOE, &gpio_init_structure);

    gpio_init_structure.Pin       = GPIO_PIN_3 | GPIO_PIN_6;
    gpio_init_structure.Mode      = GPIO_MODE_OUTPUT_PP;
    HAL_GPIO_Init(GPIOQ, &gpio_init_structure);

    gpio_init_structure.Pin       = GPIO_PIN_13;
    gpio_init_structure.Mode      = GPIO_MODE_OUTPUT_PP;
    gpio_init_structure.Pull      = GPIO_PULLUP; /* comme BSP_LCD_DisplayOn */
    HAL_GPIO_Init(GPIOG, &gpio_init_structure);

    HAL_GPIO_WritePin(GPIOQ, GPIO_PIN_3, GPIO_PIN_SET);
    HAL_GPIO_WritePin(GPIOG, GPIO_PIN_13, GPIO_PIN_SET);
    HAL_GPIO_WritePin(GPIOQ, GPIO_PIN_6, GPIO_PIN_SET);
}

void lv_port_ltdc_flush_fb_range(const void *addr, uint32_t nbytes)
{
#if defined(__DCACHE_PRESENT) && (__DCACHE_PRESENT == 1U)
    if (addr != NULL && nbytes > 0U) {
        /* Apres ecriture CPU dans le FB : Clean seulement. Clean+Invalidate peut
         * perturber si la zone est partagee ; le LTDC lit la RAM physique. */
        SCB_CleanDCache_by_Addr((volatile void *)addr, (int32_t)nbytes);
        __DSB();
    }
#else
    (void)addr;
    (void)nbytes;
#endif
}

void lv_port_disp_init(void)
{
    /* LTDC est entierement programme par AppliSecure (alias _S), CFBAR_S = 0x34200000.
     * LVGL dessine en SRAM2 ; chaque flush appelle SECURE_LtdcFbFlushRgb565 (NSC). */
    ltdc_boot_trace("  trace: LTDC deja init par Secure, skip HW init\r\n");

    lv_display_t *disp = lv_display_create(DISP_HOR_RES, DISP_VER_RES);
    lv_display_set_flush_cb(disp, disp_flush_cb);
    lv_display_set_buffers(disp, render_buf1, render_buf2,
                           sizeof(render_buf1), LV_DISPLAY_RENDER_MODE_PARTIAL);
    lv_display_set_color_format(disp, LV_COLOR_FORMAT_RGB565);
    lv_display_set_default(disp);
}

static void disp_flush_cb(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map)
{
    int32_t w = lv_area_get_width(area);
    int32_t h = lv_area_get_height(area);

#if defined(__DCACHE_PRESENT) && (__DCACHE_PRESENT == 1U)
    if (w > 0 && h > 0) {
        SCB_CleanDCache_by_Addr((void *)px_map, (int32_t)((uint32_t)w * (uint32_t)h * BYTES_PER_PX));
        __DSB();
    }
#else
    (void)w;
    (void)h;
#endif

    {
      const SECURE_LtdcFbRect_t r = {
          (uint16_t)area->x1,
          (uint16_t)area->y1,
          (uint16_t)area->x2,
          (uint16_t)area->y2,
      };
      SECURE_LtdcFbFlushRgb565(&r, (const uint16_t *)px_map);
    }

    lv_display_flush_ready(disp);
}

/**
 * @brief Memes registres que MX_LTDC_Init + MX_LTDC_ConfigLayer(..., 0, ...) du BSP.
 *        Polarites HAL : H/V/DE = AL (bits 0), PC = IPC (PCPOL).
 */
static void ltdc_hw_init(void)
{
    ltdc_boot_trace("  trace: LTDC MEMENSR\r\n");
    RCC->MEMENSR = RCC_MEMENR_AXISRAM3EN | RCC_MEMENR_AXISRAM4EN;
    __DSB();

    ltdc_boot_trace("  trace: LTDC BSP GPIO\r\n");
    ltdc_st_bsp_msp_gpio_init();
    ltdc_boot_trace("  trace: LTDC after GPIO\r\n");

    LTDC_TypeDef       *ltdc  = LTDC_NS;
    LTDC_Layer_TypeDef *layer = LTDC_Layer1_NS;

    ltdc_boot_trace("  trace: LTDC before GCR\r\n");
    ltdc->GCR &= ~LTDC_GCR_LTDCEN;
    __DSB();
    ltdc->GCR = 0U;
    __DSB();

    LTDC_Layer1_NS->CR = 0U;
    LTDC_Layer1_NS->CFBAR = 0U;
    LTDC_Layer1_NS->BFCR = 0U;
    LTDC_Layer2_NS->CR = 0U;
    LTDC_Layer2_NS->CFBAR = 0U;
    LTDC_Layer2_NS->BFCR = 0U;

    const uint32_t w = (uint32_t)RK050HR18_WIDTH;
    const uint32_t h = (uint32_t)RK050HR18_HEIGHT;
    const uint32_t hsw = (uint32_t)RK050HR18_HSYNC;
    const uint32_t hbp = (uint32_t)RK050HR18_HBP;
    const uint32_t hfp = (uint32_t)RK050HR18_HFP;
    const uint32_t vsw = (uint32_t)RK050HR18_VSYNC;
    const uint32_t vbp = (uint32_t)RK050HR18_VBP;
    const uint32_t vfp = (uint32_t)RK050HR18_VFP;

    uint32_t accum_hbp = hsw + hbp - 1U;
    uint32_t accum_vbp = vsw + vbp - 1U;
    uint32_t accum_aw  = hsw + w + hbp - 1U;
    uint32_t accum_ah  = vsw + h + vbp - 1U;
    uint32_t total_w   = hsw + w + hbp + hfp - 1U;
    uint32_t total_h   = vsw + h + vbp + vfp - 1U;

    ltdc->SSCR = ((hsw - 1U) << LTDC_SSCR_HSW_Pos) | ((vsw - 1U) << LTDC_SSCR_VSH_Pos);
    ltdc->BPCR = (accum_hbp << LTDC_BPCR_AHBP_Pos) | (accum_vbp << LTDC_BPCR_AVBP_Pos);
    ltdc->AWCR = (accum_aw << LTDC_AWCR_AAW_Pos) | (accum_ah << LTDC_AWCR_AAH_Pos);
    ltdc->TWCR = (total_w << LTDC_TWCR_TOTALW_Pos) | (total_h << LTDC_TWCR_TOTALH_Pos);

    /* BSP : HSPOL/VSPOL/DEPOL AL (=0), PCPOL IPC */
    ltdc->GCR = LTDC_GCR_BCKEN | LTDC_GCR_PCPOL;

    ltdc->BCCR = 0x00000000U;

    uint32_t h_start = accum_hbp + 1U;
    uint32_t h_stop  = accum_hbp + w;
    uint32_t v_start = accum_vbp + 1U;
    uint32_t v_stop  = accum_vbp + h;

    layer->WHPCR = (h_start << 0) | (h_stop << 16);
    layer->WVPCR = (v_start << 0) | (v_stop << 16);
    layer->PFCR  = LTDC_PF_RGB565;
    layer->CACR  = 0xFFU;
    layer->DCCR  = 0x00000000U;
    /* PAxCA comme BSP (BF1=6 BF2=7) */
    layer->BFCR  = (0x06U << LTDC_LxBFCR_BF1_Pos) | (0x07U << LTDC_LxBFCR_BF2_Pos);

    /* Secure utilise 0x34200000 ; l'alias NS 0x24200000 n'est pas fiable pour le CPU NS. */
    layer->CFBAR  = 0x34200000UL;
    layer->CFBLR  = ((w * BYTES_PER_PX) << 16) | ((w * BYTES_PER_PX) + 7U);
    layer->CFBLNR = h;

    layer->CR = LTDC_LxCR_LEN;

    ltdc->GCR |= LTDC_GCR_LTDCEN;

    /* CRITIQUE : ne PAS mettre LTDC_LxRCR_GRMSK (bit 2). GRMSK=1 masque LTDC_SRCR :
     * VBR/IMR global ne recharge plus les registres — CFBAR actif reste 0, ecran noir. */
    layer->RCR = 0U;
    __DSB();
    ltdc->SRCR = LTDC_SRCR_IMR;
    __DSB();
    {
        uint32_t t0 = HAL_GetTick();
        /* Fin de reload immediat : le HW clear SRCR.IMR (RRIF peut etre lie au VBR seulement). */
        while ((ltdc->SRCR & LTDC_SRCR_IMR) != 0U && (HAL_GetTick() - t0) < 200U) {
        }
        if ((ltdc->ISR & LTDC_ISR_RRIF) != 0U) {
            ltdc->ICR = LTDC_ICR_CRRIF;
        }
    }

    ltdc_boot_trace("  trace: LTDC regs done\r\n");
}
