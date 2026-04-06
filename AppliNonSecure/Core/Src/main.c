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
#include "lvgl_port.h"
#include "framebuffer_ns.h"
#include "lvgl.h"
#include <string.h>
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
/* =========================================================================
 * Debug board : 5 LEDs + USART2 (PD5 TX AF7, PF6 RX AF7)
 * ========================================================================= */
#define DBG_LED_GREEN_PORT   GPIOD
#define DBG_LED_GREEN_PIN    GPIO_PIN_0
#define DBG_LED_YELLOW_PORT  GPIOE
#define DBG_LED_YELLOW_PIN   GPIO_PIN_9
#define DBG_LED_RED_PORT     GPIOH
#define DBG_LED_RED_PIN      GPIO_PIN_5
#define DBG_LED_BLUE_PORT    GPIOE
#define DBG_LED_BLUE_PIN     GPIO_PIN_10
#define DBG_LED_WHITE_PORT   GPIOE
#define DBG_LED_WHITE_PIN    GPIO_PIN_13

#define DBG_LED_ON(p,b)     HAL_GPIO_WritePin(p, b, GPIO_PIN_SET)
#define DBG_LED_OFF(p,b)    HAL_GPIO_WritePin(p, b, GPIO_PIN_RESET)
#define DBG_LED_TOGGLE(p,b) HAL_GPIO_TogglePin(p, b)

#define DBG_LED_COUNT  5

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

UART_HandleTypeDef huart2;

/* USER CODE BEGIN PV */

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
static void MX_GPIO_Init(void);
static void MX_USART2_UART_Init(void);
/* USER CODE BEGIN PFP */
static void DBG_Board_Init(void);
static void DBG_Print(const char *s);
static void DBG_PrintDec(uint32_t v);
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

/* --- LED table for chenillard ------------------------------------------ */
static GPIO_TypeDef * const led_port[DBG_LED_COUNT] = {
  DBG_LED_GREEN_PORT,
  DBG_LED_YELLOW_PORT,
  DBG_LED_RED_PORT,
  DBG_LED_BLUE_PORT,
  DBG_LED_WHITE_PORT,
};
static const uint16_t led_pin[DBG_LED_COUNT] = {
  DBG_LED_GREEN_PIN,
  DBG_LED_YELLOW_PIN,
  DBG_LED_RED_PIN,
  DBG_LED_BLUE_PIN,
  DBG_LED_WHITE_PIN,
};

/* --- Debug board : GPIO (5 LEDs) init ---------------------------------- */
static void DBG_Board_Init(void)
{
  GPIO_InitTypeDef gi = {0};

  /* Clock enable for all LED ports */
  __HAL_RCC_GPIOD_CLK_ENABLE();
  __HAL_RCC_GPIOE_CLK_ENABLE();
  __HAL_RCC_GPIOH_CLK_ENABLE();

  /* Configure 5 LED pins as push-pull output */
  gi.Mode  = GPIO_MODE_OUTPUT_PP;
  gi.Pull  = GPIO_NOPULL;
  gi.Speed = GPIO_SPEED_FREQ_LOW;
  for (int i = 0; i < DBG_LED_COUNT; i++)
  {
    gi.Pin = led_pin[i];
    HAL_GPIO_Init(led_port[i], &gi);
    HAL_GPIO_WritePin(led_port[i], led_pin[i], GPIO_PIN_RESET);
  }
  /* USART2 init est fait par MX_USART2_UART_Init() + HAL_UART_MspInit() */
}

/* --- Minimal serial print ---------------------------------------------- */
static void DBG_Print(const char *s)
{
  HAL_UART_Transmit(&huart2, (const uint8_t *)s, (uint16_t)strlen(s), 100);
}

/* --- Print decimal number ---------------------------------------------- */
static void DBG_PrintDec(uint32_t v)
{
  char buf[12];
  int pos = 0;
  if (v == 0) { DBG_Print("0"); return; }
  while (v) { buf[pos++] = '0' + (v % 10); v /= 10; }
  /* reverse */
  for (int i = 0; i < pos / 2; i++) {
    char tmp = buf[i]; buf[i] = buf[pos-1-i]; buf[pos-1-i] = tmp;
  }
  buf[pos] = '\0';
  HAL_UART_Transmit(&huart2, (const uint8_t *)buf, (uint16_t)pos, 100);
}

static void DBG_PrintHex32(uint32_t v)
{
  static const char hex[] = "0123456789ABCDEF";
  char buf[11];
  buf[0] = '0';
  buf[1] = 'x';
  for (int i = 0; i < 8; i++) {
    buf[2 + i] = hex[(v >> (28 - i * 4)) & 0xFU];
  }
  buf[10] = '\0';
  DBG_Print(buf);
}

/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */
  /* ==== EARLIEST POSSIBLE TRACE — USART2 still configured from FSBL ==== */
  /* Direct register write, no HAL, no GPIO, no clock enable needed. */
  {
    const char *msg = "\r\n[NS] main() reached!\r\n";
    while (*msg) {
      while (!(USART2->ISR & USART_ISR_TXE_TXFNF)) {}
      USART2->TDR = (uint8_t)*msg++;
    }
  }

  /* ==== EARLY DIAG : LED BLANC = "NS main() atteint" ==== */
  RCC->AHB4ENR |= RCC_AHB4ENR_GPIOEEN;  /* horloge GPIOE */
  __DSB();
  GPIOE_NS->MODER = (GPIOE_NS->MODER & ~(3UL << (13*2))) | (1UL << (13*2)); /* PE13 output */
  GPIOE_NS->BSRR = (1UL << 13);  /* PE13 HIGH = BLANC ON */
  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/
  HAL_Init();

  /* USER CODE BEGIN Init */
  /* Boot LEDs (VERT/JAUNE/ROUGE) gerees par FSBL + AppliSecure */
  /* USER CODE END Init */

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_USART2_UART_Init();
  /* USER CODE BEGIN 2 */
  /* --- Debug board init (5 LEDs + USART2) --- */
  DBG_Board_Init();
  DBG_Print("\r\n=== [3/3 AppliNonSecure] Chenillard ===\r\n");

  /* LCD + LTDC sont deja initialises par AppliSecure (ecran blanc).
   * NS ne fait que lancer LVGL et le display driver. */
  DBG_Print("[2] LCD deja init par Secure\r\n");

  DBG_Print("[3] LVGL init...\r\n");
  lv_port_init();
  DBG_Print("[4] LVGL OK\r\n");

  /* Diagnostic LTDC AVANT tout rendu LVGL — confirme que le Secure a pris. */
  DBG_Print("LTDC diag: CFBAR=");
  DBG_PrintHex32(LTDC_Layer1_NS->CFBAR);
  DBG_Print(" GCR=");
  DBG_PrintHex32(LTDC_NS->GCR);
  DBG_Print(" CR=");
  DBG_PrintHex32(LTDC_Layer1_NS->CR);
  DBG_Print(" WHPCR=");
  DBG_PrintHex32(LTDC_Layer1_NS->WHPCR);
  DBG_Print("\r\n");

  /* Lire un pixel du FB pour confirmer que la SRAM3 est accessible et contient du blanc.
   * Note : RCC_NS->MEMENR peut retourner 0 si PUBCFGR non configure, meme
   * si SRAM3 est reellement actif. On tente la lecture directe. */
  {
    volatile uint16_t *fb = (volatile uint16_t *)0x24200000UL;
    DBG_Print("FB[0]=");
    DBG_PrintHex32((uint32_t)fb[0]);
    DBG_Print(" FB[1000]=");
    DBG_PrintHex32((uint32_t)fb[1000]);
    DBG_Print(" (attendu FFFF)\r\n");
  }

  /* PAS de lv_refr_now() — le FB est deja blanc depuis Secure.
   * Le lv_timer_handler() dans la boucle fera le premier rendu LVGL.
   * Si l'ecran est TOUJOURS noir ici, le probleme est LTDC/panel, pas LVGL. */
  DBG_Print("[5] Boucle main...\r\n");

  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  uint32_t chenillard_tick = 0;
  uint32_t hello_tick      = 0;
  uint32_t hello_count     = 0;
  int      chenillard_idx  = 0;

  /* Allumer la premiere LED du chenillard */
  DBG_LED_ON(led_port[0], led_pin[0]);

  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
    lv_timer_handler();

    uint32_t now = HAL_GetTick();

    /* --- Chenillard : une LED a la fois, avance toutes les 150 ms --- */
    if (now - chenillard_tick >= 150U)
    {
      chenillard_tick = now;
      DBG_LED_OFF(led_port[chenillard_idx], led_pin[chenillard_idx]);
      chenillard_idx = (chenillard_idx + 1) % DBG_LED_COUNT;
      DBG_LED_ON(led_port[chenillard_idx], led_pin[chenillard_idx]);
    }

    /* --- Hello World toutes les 1000 ms sur USART2 --- */
    if (now - hello_tick >= 1000U)
    {
      hello_tick = now;
      hello_count++;
      DBG_Print("Hello World #");
      DBG_PrintDec(hello_count);
      DBG_Print("\r\n");
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
  * @brief USART2 Initialization Function
  * @param None
  * @retval None
  */
static void MX_USART2_UART_Init(void)
{

  /* USER CODE BEGIN USART2_Init 0 */

  /* USER CODE END USART2_Init 0 */

  /* USER CODE BEGIN USART2_Init 1 */

  /* USER CODE END USART2_Init 1 */
  huart2.Instance = USART2;
  huart2.Init.BaudRate = 115200;
  huart2.Init.WordLength = UART_WORDLENGTH_8B;
  huart2.Init.StopBits = UART_STOPBITS_1;
  huart2.Init.Parity = UART_PARITY_NONE;
  huart2.Init.Mode = UART_MODE_TX_RX;
  huart2.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart2.Init.OverSampling = UART_OVERSAMPLING_16;
  huart2.Init.OneBitSampling = UART_ONE_BIT_SAMPLE_DISABLE;
  huart2.Init.ClockPrescaler = UART_PRESCALER_DIV1;
  huart2.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;
  if (HAL_UART_Init(&huart2) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_UARTEx_SetTxFifoThreshold(&huart2, UART_TXFIFO_THRESHOLD_1_8) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_UARTEx_SetRxFifoThreshold(&huart2, UART_RXFIFO_THRESHOLD_1_8) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_UARTEx_DisableFifoMode(&huart2) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USART2_Init 2 */

  /* USER CODE END USART2_Init 2 */

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
  __HAL_RCC_GPIOF_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();

  /*Configure GPIO pins : LCD_B4_Pin LCD_B5_Pin LCD_R4_Pin */
  GPIO_InitStruct.Pin = LCD_B4_Pin|LCD_B5_Pin|LCD_R4_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  GPIO_InitStruct.Alternate = GPIO_AF14_LCD;
  HAL_GPIO_Init(GPIOH, &GPIO_InitStruct);

  /*Configure GPIO pins : LCD_R2_Pin LCD_R7_Pin LCD_R1_Pin */
  GPIO_InitStruct.Pin = LCD_R2_Pin|LCD_R7_Pin|LCD_R1_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  GPIO_InitStruct.Alternate = GPIO_AF14_LCD;
  HAL_GPIO_Init(GPIOD, &GPIO_InitStruct);

  /*Configure GPIO pins : LCD_HSYNC_Pin LCD_B2_Pin LCD_G4_Pin LCD_G6_Pin
                           LCD_G5_Pin LCD_R3_Pin */
  GPIO_InitStruct.Pin = LCD_HSYNC_Pin|LCD_B2_Pin|LCD_G4_Pin|LCD_G6_Pin
                          |LCD_G5_Pin|LCD_R3_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  GPIO_InitStruct.Alternate = GPIO_AF14_LCD;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /*Configure GPIO pin : LCD_VSYNC_Pin */
  GPIO_InitStruct.Pin = LCD_VSYNC_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  GPIO_InitStruct.Alternate = GPIO_AF14_LCD;
  HAL_GPIO_Init(LCD_VSYNC_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pins : LCD_B3_Pin LCD_B0_Pin LCD_G1_Pin LCD_G0_Pin
                           LCd_G7_Pin LCD_DE_Pin LCD_R6_Pin */
  GPIO_InitStruct.Pin = LCD_B3_Pin|LCD_B0_Pin|LCD_G1_Pin|LCD_G0_Pin
                          |LCd_G7_Pin|LCD_DE_Pin|LCD_R6_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  GPIO_InitStruct.Alternate = GPIO_AF14_LCD;
  HAL_GPIO_Init(GPIOG, &GPIO_InitStruct);

  /*Configure GPIO pins : LCD_G2_Pin LCD_R5_Pin LCD_B1_Pin LCD_B7_Pin
                           LCD_B6_Pin LCD_G3_Pin */
  GPIO_InitStruct.Pin = LCD_G2_Pin|LCD_R5_Pin|LCD_B1_Pin|LCD_B7_Pin
                          |LCD_B6_Pin|LCD_G3_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  GPIO_InitStruct.Alternate = GPIO_AF14_LCD;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /* USER CODE BEGIN MX_GPIO_Init_2 */
  /* Corriger la vitesse GPIO pour les pins LTDC (CubeMX met LOW,
     mais le pixel clock a 26.67 MHz necessite HIGH) */
  GPIO_InitStruct.Mode      = GPIO_MODE_AF_PP;
  GPIO_InitStruct.Pull      = GPIO_NOPULL;
  GPIO_InitStruct.Speed     = GPIO_SPEED_FREQ_HIGH;
  GPIO_InitStruct.Alternate = GPIO_AF14_LCD;
  /* GPIOH : LCD_B4, LCD_B5, LCD_R4 */
  GPIO_InitStruct.Pin = LCD_B4_Pin|LCD_B5_Pin|LCD_R4_Pin;
  HAL_GPIO_Init(GPIOH, &GPIO_InitStruct);
  /* GPIOD : LCD_R2, LCD_R7, LCD_R1 */
  GPIO_InitStruct.Pin = LCD_R2_Pin|LCD_R7_Pin|LCD_R1_Pin;
  HAL_GPIO_Init(GPIOD, &GPIO_InitStruct);
  /* GPIOB : LCD_HSYNC, LCD_B2, LCD_G4, LCD_G6, LCD_G5, LCD_R3 */
  GPIO_InitStruct.Pin = LCD_HSYNC_Pin|LCD_B2_Pin|LCD_G4_Pin|LCD_G6_Pin
                        |LCD_G5_Pin|LCD_R3_Pin;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);
  /* GPIOE : LCD_VSYNC */
  GPIO_InitStruct.Pin = LCD_VSYNC_Pin;
  HAL_GPIO_Init(LCD_VSYNC_GPIO_Port, &GPIO_InitStruct);
  /* GPIOG : LCD_B3, LCD_B0, LCD_G1, LCD_G0, LCd_G7, LCD_R6 (PAS LCD_DE) */
  GPIO_InitStruct.Pin = LCD_B3_Pin|LCD_B0_Pin|LCD_G1_Pin|LCD_G0_Pin
                        |LCd_G7_Pin|LCD_R6_Pin;
  HAL_GPIO_Init(GPIOG, &GPIO_InitStruct);
  /* GPIOA : LCD_G2, LCD_R5, LCD_B1, LCD_B7, LCD_B6, LCD_G3 */
  GPIO_InitStruct.Pin = LCD_G2_Pin|LCD_R5_Pin|LCD_B1_Pin|LCD_B7_Pin
                        |LCD_B6_Pin|LCD_G3_Pin;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

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
