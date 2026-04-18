/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    Secure/Src/secure_nsc.c
  * @author  MCD Application Team
  * @brief   This file contains the non-secure callable APIs (secure world)
  ******************************************************************************
    * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */

/* USER CODE BEGIN Non_Secure_CallLib */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "secure_nsc.h"
#include <string.h>
/** @addtogroup STM32N6xx_HAL_Examples

  * @{
  */

/** @addtogroup Templates
  * @{
  */

/* Global variables ----------------------------------------------------------*/
void *pSecureFaultCallback = NULL;   /* Pointer to secure fault callback in Non-secure */
void *pSecureErrorCallback = NULL;   /* Pointer to secure error callback in Non-secure */

/* Private typedef -----------------------------------------------------------*/
/* Private define ------------------------------------------------------------*/
/* Private macro -------------------------------------------------------------*/
/* Private variables ---------------------------------------------------------*/
/* Private function prototypes -----------------------------------------------*/
/* Private functions ---------------------------------------------------------*/

/**
  * @brief  Secure registration of non-secure callback.
  * @param  CallbackId  callback identifier
  * @param  func        pointer to non-secure function
  * @retval None
  */
  CMSE_NS_ENTRY void SECURE_RegisterCallback(SECURE_CallbackIDTypeDef CallbackId, void *func)
  {
      if(func != NULL)
      {
        switch(CallbackId)
        {
          case SECURE_FAULT_CB_ID:           /* SecureFault Interrupt occurred */
          pSecureFaultCallback = func;
          break;
          case IAC_ERROR_CB_ID:             /* Illegal Access Interrupt occurred */
          pSecureErrorCallback = func;
          break;
          default:
          /* unknown */
          break;
        }
      }
  }

#define LTDC_FB_S_BASE  (0x34200000UL)
#define LTDC_FB_WIDTH   (800U)
#define LTDC_FB_HEIGHT  (480U)
#define LTDC_FB_NPIX    (LTDC_FB_WIDTH * LTDC_FB_HEIGHT)

static void ltdc_fb_clean_range(const void *start, uint32_t byte_len)
{
#if defined(__DCACHE_PRESENT) && (__DCACHE_PRESENT == 1U)
  if (start != NULL && byte_len > 0U) {
    SCB_CleanDCache_by_Addr((void *)start, (int32_t)byte_len);
    __DSB();
  }
#else
  (void)start;
  (void)byte_len;
#endif
}

/**
  * @brief Remplit tout le framebuffer LTDC (alias Secure SRAM3) en RGB565.
  */
CMSE_NS_ENTRY void SECURE_LtdcFbFillRgb565(uint16_t color)
{
  uint16_t *dst = (uint16_t *)LTDC_FB_S_BASE;

  for (uint32_t i = 0; i < LTDC_FB_NPIX; i++) {
    dst[i] = color;
  }
  __DSB();
  ltdc_fb_clean_range((void *)LTDC_FB_S_BASE, (uint32_t)LTDC_FB_NPIX * sizeof(uint16_t));
  /* Forcer un reload immediat du layer : certains chemins laissent le scan
   * sur une ancienne fenetre shadow si seul le contenu SRAM change. */
  LTDC_Layer1_S->RCR = LTDC_LxRCR_IMR;
  __DSB();
  {
    uint32_t spin = 0U;
    while ((LTDC_Layer1_S->RCR & LTDC_LxRCR_IMR) != 0U && spin < 100000U) {
      spin++;
    }
  }
}

/**
  * @brief Copie un rectangle RGB565 depuis la memoire NS vers le FB Secure (0x34200000).
  */
CMSE_NS_ENTRY void SECURE_LtdcFbFlushRgb565(const SECURE_LtdcFbRect_t *rect, const uint16_t *ns_src)
{
  if (rect == NULL || ns_src == NULL) {
    return;
  }

  const uint32_t x1 = rect->x1;
  const uint32_t y1 = rect->y1;
  const uint32_t x2 = rect->x2;
  const uint32_t y2 = rect->y2;

  if ((x1 > x2) || (y1 > y2)) {
    return;
  }
  if ((x2 >= LTDC_FB_WIDTH) || (y2 >= LTDC_FB_HEIGHT)) {
    return;
  }

  const uint32_t w = x2 - x1 + 1U;
  const uint32_t h = y2 - y1 + 1U;

  /* Pas de cmse_check_address_range ici : avec MPU NS + nano-newlib, les
   * verifications peuvent echouer sur des pointeurs pourtant valides (pile NS,
   * buffers LVGL) et le flush devient silencieux = ecran fige sur la demo S. */

  uint16_t *const       dst_base = (uint16_t *)LTDC_FB_S_BASE;
  const uint16_t *      src_row  = ns_src;

  for (uint32_t row = 0; row < h; row++) {
    uint16_t *dst = &dst_base[(y1 + row) * LTDC_FB_WIDTH + x1];
    (void)memcpy(dst, src_row, (size_t)w * sizeof(uint16_t));
    src_row += w;
  }
  __DSB();
  {
    const uint8_t *p0 = (const uint8_t *)&dst_base[y1 * LTDC_FB_WIDTH + x1];
    const uint32_t last = (y1 + h - 1U) * LTDC_FB_WIDTH + (x1 + w - 1U);
    const uint8_t *p1 = (const uint8_t *)&dst_base[last] + sizeof(uint16_t);
    const uint32_t nbytes = (uint32_t)((uintptr_t)p1 - (uintptr_t)p0);
    ltdc_fb_clean_range(p0, nbytes);
  }
  /* Pas de LTDC_LxRCR_IMR ici : LVGL appelle souvent le flush ; le LTDC lit le FB en RAM. */
}

/**
  * @}
  */

/**
  * @}
  */
/* USER CODE END Non_Secure_CallLib */

