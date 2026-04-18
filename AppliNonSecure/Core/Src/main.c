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
#include "ns_diag_config.h"
#if !NS_DIAG_NO_LVGL
#include "lvgl_port.h"
#include "lvgl.h"
#endif
#include "secure_nsc.h"
#include <string.h>
/* SCB_InvalidateDCache_by_Addr : via CMSIS dans stm32n6xx.h (main.h) */
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

/* Cause reset vue depuis NS (meme info que FSBL, utile si trace tronquee). */
static void DBG_PrintResetCauseNs(void)
{
  const uint32_t r = RCC_NS->RSR;
  /* FSBL efface souvent RMVF apres son trace : ici souvent 0, c'est normal. */
  DBG_Print("[NS] RCC->RSR=");
  DBG_PrintHex32(r);
  DBG_Print(" [");
  if ((r & RCC_RSR_LCKRSTF) != 0U) {
    DBG_Print("LCKUP ");
  }
  if ((r & RCC_RSR_BORRSTF) != 0U) {
    DBG_Print("BOR ");
  }
  if ((r & RCC_RSR_PINRSTF) != 0U) {
    DBG_Print("NRST ");
  }
  if ((r & RCC_RSR_PORRSTF) != 0U) {
    DBG_Print("POR ");
  }
  if ((r & RCC_RSR_SFTRSTF) != 0U) {
    DBG_Print("SW ");
  }
  if ((r & RCC_RSR_IWDGRSTF) != 0U) {
    DBG_Print("IWDG ");
  }
  if ((r & RCC_RSR_WWDGRSTF) != 0U) {
    DBG_Print("WWDG ");
  }
  if ((r & RCC_RSR_LPWRRSTF) != 0U) {
    DBG_Print("LPWR ");
  }
  DBG_Print("]\r\n");
  if (r == 0U) {
    DBG_Print("[NS] RSR=0 normal: FSBL efface RMVF apres trace; cause reelle = ligne [FSBL] Reset cause\r\n");
  }
}

#if NS_DIAG_NO_LVGL
/* Compare aux traces AppliSecure [7f3] si l'ecran reste noir : broches LTDC / PQ. */
static void DBG_PrintLcdGpioSnapshot(void)
{
  __HAL_RCC_GPIOB_CLK_ENABLE();
  __HAL_RCC_GPIOE_CLK_ENABLE();
  __HAL_RCC_GPIOG_CLK_ENABLE();
  __HAL_RCC_GPIOQ_CLK_ENABLE();
  DBG_Print("[NS] GPIO snap PB.MODER=");
  DBG_PrintHex32(GPIOB_NS->MODER);
  DBG_Print(" AFRH=");
  DBG_PrintHex32(GPIOB_NS->AFR[1]);
  DBG_Print(" PE.MODER=");
  DBG_PrintHex32(GPIOE_NS->MODER);
  DBG_Print(" PG.MODER=");
  DBG_PrintHex32(GPIOG_NS->MODER);
  DBG_Print(" PG.ODR=");
  DBG_PrintHex32(GPIOG_NS->ODR);
  DBG_Print(" PQ.ODR=");
  DBG_PrintHex32(GPIOQ_NS->ODR);
  DBG_Print(" (attendu ~ PB MODER 0xAABFFAEF AFRH 0xEEEEE000)\r\n");
}
#endif

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

  /* NE PAS toucher RCC/GPIO avant HAL_Init() : sur N6+TZ cela peut corrompre
   * l'init RCC interne -> fault dans LL_RCC (ex. MMFAR ~ 0x1043xxxx) et LEDs figees. */
  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/
  HAL_Init();

  /* USER CODE BEGIN Init */
  /* LED blanche PE13 = NS vivant (apres HAL : horloges / attributs coherents) */
  __HAL_RCC_GPIOE_CLK_ENABLE();
  {
    GPIO_InitTypeDef gi = {0};
    gi.Pin   = GPIO_PIN_13;
    gi.Mode  = GPIO_MODE_OUTPUT_PP;
    gi.Pull  = GPIO_NOPULL;
    gi.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOE, &gi);
    HAL_GPIO_WritePin(GPIOE, GPIO_PIN_13, GPIO_PIN_SET);
  }
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
  DBG_Print("[NS] BUILD=" __DATE__ " " __TIME__ "\r\n");
  DBG_PrintResetCauseNs();

  /* LCD + LTDC sont deja initialises par AppliSecure (ecran blanc).
   * NS ne fait que lancer LVGL et le display driver. */
  DBG_Print("[2] LCD deja init par Secure\r\n");

  /* PQ3/PQ6 = LCD_ON + backlight (RIF NSEC). Sans horloge ou si ODR retombe LOW,
   * l'image peut etre correcte mais invisible — reaffirmer avant test NSC. */
  __HAL_RCC_GPIOQ_CLK_ENABLE();
  HAL_GPIO_WritePin(GPIOQ, GPIO_PIN_3, GPIO_PIN_SET);
  HAL_GPIO_WritePin(GPIOQ, GPIO_PIN_6, GPIO_PIN_SET);

#if NS_DIAG_NO_LVGL
  DBG_PrintLcdGpioSnapshot();
#endif

  /* Test NSC : rouge = remplissage FB en monde Secure via CMSE (pas LVGL). */
  DBG_Print("[4a] NSC plein ecran rouge RGB565 0xF800\r\n");
  SECURE_LtdcFbFillRgb565(0xF800U);
  HAL_Delay(400U);

#if NS_DIAG_NO_LVGL
  DBG_Print("[NS-DIAG] LVGL OFF (ns_diag_config.h) - NSC display only\r\n");
  DBG_Print("[5] Boucle main (rouge NSC rafraichi toutes les 5 s)...\r\n");
#else
  DBG_Print("[3] LVGL init...\r\n");
  lv_port_init();
  DBG_Print("[4] LVGL OK\r\n");

  /* Sans ca, le 1er rafraichissement LVGL repeint le FB en noir (fond ecran defaut).
   * Le rouge [4a] disparait alors que LTDC+NSC sont OK. */
  {
    lv_display_t *const d = lv_display_get_default();
    if (d != NULL) {
      lv_obj_t *const scr = lv_display_get_screen_active(d);
      if (scr != NULL) {
        lv_obj_set_style_bg_color(scr, lv_color_hex(0xF800), LV_PART_MAIN);
        lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, LV_PART_MAIN);
      }
    }
  }
  (void)lv_timer_handler();

  DBG_Print("[4b] LTDC NS diag (souvent 0 en TZ) CFBAR=");
  DBG_PrintHex32(LTDC_Layer1_NS->CFBAR);
  DBG_Print("\r\n");

  DBG_Print("[5] Boucle main...\r\n");
#endif

  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  uint32_t chenillard_tick = 0;
  uint32_t hello_tick      = 0;
  uint32_t hello_count     = 0;
  int      chenillard_idx  = 0;
#if NS_DIAG_NO_LVGL
  uint32_t nsc_red_tick = 0;
  uint32_t nsc_color_phase = 0;
#endif

  /* Allumer la premiere LED du chenillard */
  DBG_LED_ON(led_port[0], led_pin[0]);

  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
#if !NS_DIAG_NO_LVGL
    lv_timer_handler();
#endif

    uint32_t now = HAL_GetTick();

#if NS_DIAG_NO_LVGL
    /* Rouge / vert alternes : si vous voyez les couleurs changer, LTDC+FB+NSC OK (panneau). */
    if ((now - nsc_red_tick) >= 5000U) {
      nsc_red_tick = now;
      const uint16_t c = ((nsc_color_phase++ & 1U) != 0U) ? 0xF800U : 0x07E0U;
      SECURE_LtdcFbFillRgb565(c);
      DBG_Print((c == 0xF800U) ? "[NS] NSC fill RED\r\n" : "[NS] NSC fill GREEN\r\n");
    }
#endif

    /* --- Chenillard : une LED a la fois (~120 ms = lisible, pas "hyper rapide") --- */
    if (now - chenillard_tick >= 120U)
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
#if NS_DIAG_NO_LVGL
  /* Ne pas reconfigurer les GPIO LTDC du .ioc NS : mapping != BSP Secure (AppliSecure
   * section 7a : PA/PB/PD/PE/PG/PH + PB13 CLK). Un MX_GPIO_Init complet casse l'AF
   * deja valide -> ecran noir malgre SECURE_LtdcFbFillRgb565. */
  __HAL_RCC_GPIOQ_CLK_ENABLE();
  HAL_GPIO_WritePin(GPIOQ, GPIO_PIN_3, GPIO_PIN_SET);
  HAL_GPIO_WritePin(GPIOQ, GPIO_PIN_6, GPIO_PIN_SET);
  return;
#endif
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
