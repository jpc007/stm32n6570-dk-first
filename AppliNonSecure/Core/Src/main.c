/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
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
/* Includes ------------------------------------------------------------------*/
#include "main.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
/* LVGL desactive pour test baremetal LTDC */
// #include "lvgl_port.h"
// #include "lvgl.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
static void MX_GPIO_Init(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  /* USER CODE BEGIN 2 */
  /* LED PA3 et PE14 : init GPIO output */
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOE_CLK_ENABLE();
  GPIO_InitTypeDef GPIO_InitStruct = {0};
  GPIO_InitStruct.Pin   = GPIO_PIN_3;
  GPIO_InitStruct.Mode  = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull  = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);
  GPIO_InitStruct.Pin   = GPIO_PIN_14;
  HAL_GPIO_Init(GPIOE, &GPIO_InitStruct);

  /* LED ON = preuve qu'on atteint ce point */
  HAL_GPIO_WritePin(GPIOA, GPIO_PIN_3, GPIO_PIN_SET);

  /* --- LCD power ON + backlight ------------------------------------ */
  __HAL_RCC_GPIOQ_CLK_ENABLE();

  GPIO_InitStruct.Pin   = GPIO_PIN_3;
  GPIO_InitStruct.Mode  = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull  = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOQ, &GPIO_InitStruct);
  HAL_GPIO_WritePin(GPIOQ, GPIO_PIN_3, GPIO_PIN_SET);   /* LCD_ON */

  GPIO_InitStruct.Pin   = GPIO_PIN_6;
  HAL_GPIO_Init(GPIOQ, &GPIO_InitStruct);
  HAL_GPIO_WritePin(GPIOQ, GPIO_PIN_6, GPIO_PIN_SET);   /* backlight */

  HAL_Delay(50);  /* laisser le panneau demarrer */

  /* =================================================================
   *  TEST BAREMETAL LTDC ULTRA-MINIMAL
   *  PAS de framebuffer, PAS de SRAM3 — seulement background color.
   *  Si l'ecran affiche du ROUGE = hardware OK (GPIO + pixel clock).
   *  Si ecran noir = probleme de clock ou GPIO.
   * ================================================================= */

  /* ---- PIXEL CLOCK : IC16 = PLL1 (1200 MHz) / 45 = 26.67 MHz ----
   * CRITIQUE : doit etre configure AVANT d'activer le clock LTDC,
   * sinon LTDC utilise PCLK5 (~200 MHz) et sature le bus AHB,
   * tuant le SWD -> "Target is not responding".
   */
  MODIFY_REG(RCC->IC16CFGR,
             RCC_IC16CFGR_IC16SEL | RCC_IC16CFGR_IC16INT,
             (0U << RCC_IC16CFGR_IC16SEL_Pos) |   /* source = PLL1 */
             (44U << RCC_IC16CFGR_IC16INT_Pos));   /* divider 45 (N-1) */
  SET_BIT(RCC->DIVENSR, RCC_DIVENSR_IC16ENS);     /* enable IC16 */
  while (!(RCC->DIVENR & RCC_DIVENR_IC16EN)) {}   /* wait enabled */
  MODIFY_REG(RCC->CCIPR4, RCC_CCIPR4_LTDCSEL,
             RCC_CCIPR4_LTDCSEL_1);                /* LTDC clock = IC16 */

  /* LTDC APB5 clock */
  RCC->APB5ENR |= RCC_APB5ENR_LTDCEN;
  __DSB();

  /* ---- Configurer LTDC — background color only, NO layer ---- */
  #define DISP_W   800
  #define DISP_H   480
  #define HSYNC_W  4
  #define HBP      4
  #define HFP      4
  #define VSYNC_W  4
  #define VBP      4
  #define VFP      4

  LTDC_TypeDef *ltdc = LTDC_NS;

  ltdc->GCR &= ~LTDC_GCR_LTDCEN;  /* disable pendant config */

  uint32_t accum_hbp = HSYNC_W + HBP - 1;
  uint32_t accum_vbp = VSYNC_W + VBP - 1;
  uint32_t accum_aw  = HSYNC_W + HBP + DISP_W - 1;
  uint32_t accum_ah  = VSYNC_W + VBP + DISP_H - 1;
  uint32_t total_w   = HSYNC_W + HBP + DISP_W + HFP - 1;
  uint32_t total_h   = VSYNC_W + VBP + DISP_H + VFP - 1;

  ltdc->SSCR = ((HSYNC_W - 1) << LTDC_SSCR_HSW_Pos) |
               ((VSYNC_W - 1) << LTDC_SSCR_VSH_Pos);
  ltdc->BPCR = (accum_hbp << LTDC_BPCR_AHBP_Pos) |
               (accum_vbp << LTDC_BPCR_AVBP_Pos);
  ltdc->AWCR = (accum_aw << LTDC_AWCR_AAW_Pos) |
               (accum_ah << LTDC_AWCR_AAH_Pos);
  ltdc->TWCR = (total_w << LTDC_TWCR_TOTALW_Pos) |
               (total_h << LTDC_TWCR_TOTALH_Pos);

  /* Fond ROUGE VIF — pas de layer active, le LTDC affiche juste BCCR */
  ltdc->BCCR = 0x00FF0000;  /* R=FF, G=00, B=00 (format RGB888) */

  /* Desactiver explicitement les deux layers */
  LTDC_Layer1_NS->CR = 0;
  LTDC_Layer2_NS->CR = 0;

  /* Reload immediat */
  ltdc->SRCR = LTDC_SRCR_IMR;

  /* Activer LTDC */
  ltdc->GCR |= LTDC_GCR_LTDCEN;

  /* LED PE14 ON = LTDC active */
  HAL_GPIO_WritePin(GPIOE, GPIO_PIN_14, GPIO_PIN_SET);
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  uint32_t led_tick = 0;
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
    /* Clignotement LED (~500ms) pour prouver que le CPU tourne */
    if (HAL_GetTick() - led_tick >= 500)
    {
      led_tick = HAL_GetTick();
      HAL_GPIO_TogglePin(GPIOA, GPIO_PIN_3);
      HAL_GPIO_TogglePin(GPIOE, GPIO_PIN_14);
    }
  }
  /* USER CODE END 3 */
}

/**
  * @brief Peripherals Common Clock Configuration
  * @retval None
  */
void PeriphCommonClock_Config(void)
{
  RCC_PeriphCLKInitTypeDef PeriphClkInitStruct = {0};

  /** Initializes the peripherals clock
  */
  PeriphClkInitStruct.PeriphClockSelection = RCC_PERIPHCLK_CKPER;
  PeriphClkInitStruct.CkperClockSelection = RCC_CLKPCLKSOURCE_HSI;
  if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInitStruct) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};
  /* USER CODE BEGIN MX_GPIO_Init_1 */

  /* USER CODE END MX_GPIO_Init_1 */

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOH_CLK_ENABLE();
  __HAL_RCC_GPIOD_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();
  __HAL_RCC_GPIOE_CLK_ENABLE();
  __HAL_RCC_GPIOG_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();

  /*Configure GPIO pins : LCD_B4_Pin LCD_B5_Pin LCD_R4_Pin */
  GPIO_InitStruct.Pin = LCD_B4_Pin|LCD_B5_Pin|LCD_R4_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
  GPIO_InitStruct.Alternate = GPIO_AF14_LCD;
  HAL_GPIO_Init(GPIOH, &GPIO_InitStruct);

  /*Configure GPIO pins : LCD_R2_Pin LCD_R7_Pin LCD_R1_Pin */
  GPIO_InitStruct.Pin = LCD_R2_Pin|LCD_R7_Pin|LCD_R1_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
  GPIO_InitStruct.Alternate = GPIO_AF14_LCD;
  HAL_GPIO_Init(GPIOD, &GPIO_InitStruct);

  /*Configure GPIO pins : LCD_HSYNC_Pin LCD_B2_Pin LCD_G4_Pin LCD_G6_Pin
                           LCD_G5_Pin LCD_R3_Pin LCD_CLK_Pin */
  GPIO_InitStruct.Pin = LCD_HSYNC_Pin|LCD_B2_Pin|LCD_G4_Pin|LCD_G6_Pin
                          |LCD_G5_Pin|LCD_R3_Pin|LCD_CLK_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
  GPIO_InitStruct.Alternate = GPIO_AF14_LCD;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /*Configure GPIO pin : LCD_VSYNC_Pin */
  GPIO_InitStruct.Pin = LCD_VSYNC_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
  GPIO_InitStruct.Alternate = GPIO_AF14_LCD;
  HAL_GPIO_Init(LCD_VSYNC_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pins : LCD_B3_Pin LCD_B0_Pin LCD_G1_Pin LCD_G0_Pin
                           LCd_G7_Pin LCD_R6_Pin LCD_R0_Pin */
  GPIO_InitStruct.Pin = LCD_B3_Pin|LCD_B0_Pin|LCD_G1_Pin|LCD_G0_Pin
                          |LCd_G7_Pin|LCD_R6_Pin|LCD_R0_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
  GPIO_InitStruct.Alternate = GPIO_AF14_LCD;
  HAL_GPIO_Init(GPIOG, &GPIO_InitStruct);

  /* LCD_DE (PG13) : sortie GPIO mise a HIGH (pas AF14 !) conformement au BSP ST */
  GPIO_InitStruct.Pin   = LCD_DE_Pin;
  GPIO_InitStruct.Mode  = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull  = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
  HAL_GPIO_Init(LCD_DE_GPIO_Port, &GPIO_InitStruct);
  HAL_GPIO_WritePin(LCD_DE_GPIO_Port, LCD_DE_Pin, GPIO_PIN_SET);

  /*Configure GPIO pins : LCD_G2_Pin LCD_R5_Pin LCD_B1_Pin LCD_B7_Pin
                           LCD_B6_Pin LCD_G3_Pin */
  GPIO_InitStruct.Pin = LCD_G2_Pin|LCD_R5_Pin|LCD_B1_Pin|LCD_B7_Pin
                          |LCD_B6_Pin|LCD_G3_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
  GPIO_InitStruct.Alternate = GPIO_AF14_LCD;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /* USER CODE BEGIN MX_GPIO_Init_2 */
  /* LCD_NRST (PE1) : sortir le panneau LCD du reset */
  GPIO_InitStruct.Pin   = LCD_NRST_Pin;
  GPIO_InitStruct.Mode  = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull  = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(LCD_NRST_GPIO_Port, &GPIO_InitStruct);
  HAL_GPIO_WritePin(LCD_NRST_GPIO_Port, LCD_NRST_Pin, GPIO_PIN_SET);
  /* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */

/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  __BKPT(0);
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}
#ifdef USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
