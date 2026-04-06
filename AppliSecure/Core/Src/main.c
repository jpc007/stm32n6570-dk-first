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
#include "stm32n6xx_hal_cortex.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

#define VECT_TAB_NS_OFFSET  0x00400
#define VTOR_TABLE_NS_START_ADDR (SRAM2_AXI_BASE_NS|VECT_TAB_NS_OFFSET)
#define VTOR_TABLE_S_START_ADDR  (SRAM2_AXI_BASE_S |VECT_TAB_NS_OFFSET)

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
static void NonSecure_Init(void);
static void SystemIsolation_Config(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

/* --- Boot trace via USART2 (configure par FSBL, registres persistent) --- */
static void boot_trace_putc(char c)
{
    while (!(USART2->ISR & USART_ISR_TXE_TXFNF)) {}
    USART2->TDR = (uint8_t)c;
}
static void boot_trace(const char *s)
{
    while (*s) boot_trace_putc(*s++);
}

/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */
  /* ==== EARLY DIAG : boot_trace avant HAL_Init ==== */
  /* USART2 toujours configure par FSBL (registres persistent apres jump) */
  boot_trace("\r\n--- AppliSecure: Reset_Handler OK ---\r\n");
  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/
  HAL_Init();

  /* USER CODE BEGIN Init */
  boot_trace("[AppliSecure] HAL_Init OK\r\n");
  /* USER CODE END Init */

  /* USER CODE BEGIN SysInit */
  boot_trace("[AppliSecure] before SystemIsolation_Config\r\n");
  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  SystemIsolation_Config();
  /* USER CODE BEGIN 2 */

  /* ============================================================
   * BOOT STAGE 2 : LED JAUNE (PE9) 3s, puis LED ROUGE (PH5) 3s
   * USART2 configure par FSBL (registres persistent apres jump).
   * SysTick tourne (HAL_SuspendTick pas encore appele).
   * ============================================================ */
  {
    GPIO_InitTypeDef gpio = {0};
    gpio.Mode  = GPIO_MODE_OUTPUT_PP;
    gpio.Pull  = GPIO_NOPULL;
    gpio.Speed = GPIO_SPEED_FREQ_LOW;

    /* LED JAUNE (PE9) — GPIOE clock deja ON (SystemIsolation_Config) */
    gpio.Pin = GPIO_PIN_9;
    HAL_GPIO_Init(GPIOE, &gpio);
    HAL_GPIO_WritePin(GPIOE, GPIO_PIN_9, GPIO_PIN_SET);

    boot_trace("=== [2/3 AppliSecure] LED JAUNE ===\r\n");
    HAL_Delay(500);
    HAL_GPIO_WritePin(GPIOE, GPIO_PIN_9, GPIO_PIN_RESET);

    /* LED ROUGE (PH5) — GPIOH clock deja ON (SystemIsolation_Config) */
    gpio.Pin = GPIO_PIN_5;
    HAL_GPIO_Init(GPIOH, &gpio);
    HAL_GPIO_WritePin(GPIOH, GPIO_PIN_5, GPIO_PIN_SET);

    boot_trace("=== [2/3 AppliSecure] LED ROUGE ===\r\n");
    HAL_Delay(500);
    HAL_GPIO_WritePin(GPIOH, GPIO_PIN_5, GPIO_PIN_RESET);

    boot_trace("=== [2/3 AppliSecure] -> NonSecure ===\r\n");
  }

  /* USER CODE END 2 */

  /* Secure SysTick should rather be suspended before calling non-secure  */
  /* in order to avoid wake-up from sleep mode entered by non-secure      */
  /* The Secure SysTick shall be resumed on non-secure callable functions */
  HAL_SuspendTick();

  /*************** Setup and jump to non-secure *******************************/

  NonSecure_Init();

  /* Non-secure software does not return, this code is not executed */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
  }
  /* USER CODE END 3 */
}

/**
  * @brief  Non-secure call function
  *         This function is responsible for Non-secure initialization and switch
  *         to non-secure state
  * @retval None
  */
static void NonSecure_Init(void)
{
  funcptr_NS NonSecure_ResetHandler;

  SCB_NS->VTOR = VTOR_TABLE_NS_START_ADDR;

  /* Read vector table via NS alias (0x24100400).
   * RISAF3 is now configured NSEC → S alias (0x34100400) is BLOCKED.
   * Secure CPU can read NS memory via NS alias without issue. */
  uint32_t ns_msp   = (*(uint32_t *)VTOR_TABLE_NS_START_ADDR);
  uint32_t ns_reset = (*((uint32_t *)((VTOR_TABLE_NS_START_ADDR) + 4U)));

  boot_trace("[NS] VTOR=0x");
  { char h[9]; uint32_t v=VTOR_TABLE_NS_START_ADDR; for(int i=7;i>=0;i--){uint8_t n=(v>>(i*4))&0xF;h[7-i]=n<10?'0'+n:'A'+n-10;}h[8]=0;boot_trace(h); }
  boot_trace(" MSP=0x");
  { char h[9]; uint32_t v=ns_msp; for(int i=7;i>=0;i--){uint8_t n=(v>>(i*4))&0xF;h[7-i]=n<10?'0'+n:'A'+n-10;}h[8]=0;boot_trace(h); }
  boot_trace(" RST=0x");
  { char h[9]; uint32_t v=ns_reset; for(int i=7;i>=0;i--){uint8_t n=(v>>(i*4))&0xF;h[7-i]=n<10?'0'+n:'A'+n-10;}h[8]=0;boot_trace(h); }
  boot_trace("\r\n");

  /* Set non-secure main stack (MSP_NS) */
  __TZ_set_MSP_NS(ns_msp);

  /* Get non-secure reset handler */
  NonSecure_ResetHandler = (funcptr_NS)(ns_reset);

  boot_trace("[NS] jumping...\r\n");

  /* Start non-secure state software application */
  NonSecure_ResetHandler();
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
  * @brief RIF Initialization Function
  * @param None
  * @retval None
  */
  static void SystemIsolation_Config(void)
{

  /* USER CODE BEGIN RIF_Init 0 */
  /* WORKAROUND CubeMX: certains HAL_GPIO_ConfigPinAttributes() sont appeles
   * AVANT le __HAL_RCC_GPIOx_CLK_ENABLE() correspondant.
   * Acceder aux registres SECCFGR sans clock = BusFault.
   * On active toutes les horloges GPIO en avance. */
  boot_trace("[RIF] clock all GPIO\r\n");
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOD_CLK_ENABLE();
  __HAL_RCC_GPIOE_CLK_ENABLE();
  __HAL_RCC_GPIOF_CLK_ENABLE();
  __HAL_RCC_GPIOG_CLK_ENABLE();
  __HAL_RCC_GPIOH_CLK_ENABLE();
  __HAL_RCC_GPION_CLK_ENABLE();
  __HAL_RCC_GPIOO_CLK_ENABLE();
  __HAL_RCC_GPIOP_CLK_ENABLE();
  __HAL_RCC_GPIOQ_CLK_ENABLE();
  __DSB();
  /* USER CODE END RIF_Init 0 */

  /* set all required IPs as secure privileged */
  __HAL_RCC_RIFSC_CLK_ENABLE();

  /*RIMC configuration*/
  RIMC_MasterConfig_t RIMC_master = {0};
  RIMC_master.MasterCID = RIF_CID_1;
  RIMC_master.SecPriv = RIF_ATTRIBUTE_SEC | RIF_ATTRIBUTE_NPRIV;
  HAL_RIF_RIMC_ConfigMasterAttributes(RIF_MASTER_INDEX_ETH1, &RIMC_master);

  HAL_RIF_RIMC_ConfigMasterAttributes(RIF_MASTER_INDEX_SDMMC2, &RIMC_master);

  /*RISUP configuration*/
  HAL_RIF_RISC_SetSlaveSecureAttributes(RIF_RISC_PERIPH_INDEX_SAI1 , RIF_ATTRIBUTE_SEC | RIF_ATTRIBUTE_NPRIV);
  HAL_RIF_RISC_SetSlaveSecureAttributes(RIF_RISC_PERIPH_INDEX_I2C1 , RIF_ATTRIBUTE_SEC | RIF_ATTRIBUTE_NPRIV);
  HAL_RIF_RISC_SetSlaveSecureAttributes(RIF_RISC_PERIPH_INDEX_I2C2 , RIF_ATTRIBUTE_SEC | RIF_ATTRIBUTE_NPRIV);
  HAL_RIF_RISC_SetSlaveSecureAttributes(RIF_RISC_PERIPH_INDEX_USART1 , RIF_ATTRIBUTE_SEC | RIF_ATTRIBUTE_NPRIV);
  HAL_RIF_RISC_SetSlaveSecureAttributes(RIF_RISC_PERIPH_INDEX_MDF1 , RIF_ATTRIBUTE_SEC | RIF_ATTRIBUTE_NPRIV);
  HAL_RIF_RISC_SetSlaveSecureAttributes(RIF_RISC_PERIPH_INDEX_SDMMC2 , RIF_ATTRIBUTE_SEC | RIF_ATTRIBUTE_NPRIV);
  HAL_RIF_RISC_SetSlaveSecureAttributes(RIF_RISC_PERIPH_INDEX_OTG1HS , RIF_ATTRIBUTE_SEC | RIF_ATTRIBUTE_NPRIV);
  HAL_RIF_RISC_SetSlaveSecureAttributes(RIF_RISC_PERIPH_INDEX_OTG2HS , RIF_ATTRIBUTE_SEC | RIF_ATTRIBUTE_NPRIV);
  HAL_RIF_RISC_SetSlaveSecureAttributes(RIF_RISC_PERIPH_INDEX_UCPD1 , RIF_ATTRIBUTE_SEC | RIF_ATTRIBUTE_NPRIV);
  HAL_RIF_RISC_SetSlaveSecureAttributes(RIF_RISC_PERIPH_INDEX_ETH1 , RIF_ATTRIBUTE_SEC | RIF_ATTRIBUTE_NPRIV);
  HAL_RIF_RISC_SetSlaveSecureAttributes(RIF_RISC_PERIPH_INDEX_ADC12 , RIF_ATTRIBUTE_SEC | RIF_ATTRIBUTE_NPRIV);
  HAL_RIF_RISC_SetSlaveSecureAttributes(RIF_RISC_PERIPH_INDEX_XSPI1 , RIF_ATTRIBUTE_SEC | RIF_ATTRIBUTE_NPRIV);
  HAL_RIF_RISC_SetSlaveSecureAttributes(RIF_RISC_PERIPH_INDEX_XSPI2 , RIF_ATTRIBUTE_SEC | RIF_ATTRIBUTE_NPRIV);
  HAL_RIF_RISC_SetSlaveSecureAttributes(RIF_RISC_PERIPH_INDEX_XSPIM , RIF_ATTRIBUTE_SEC | RIF_ATTRIBUTE_NPRIV);
  HAL_RIF_RISC_SetSlaveSecureAttributes(RIF_RISC_PERIPH_INDEX_CSI , RIF_ATTRIBUTE_SEC | RIF_ATTRIBUTE_NPRIV);

  /* RIF-Aware IPs Config */

  /* set up GPIO configuration */
  /* GPIOA Non Secure Ports Clock Enable */
  __HAL_RCC_GPIOA_CLK_ENABLE();
  HAL_GPIO_ConfigPinAttributes(GPIOA,GPIO_PIN_0,GPIO_PIN_NSEC|GPIO_PIN_NPRIV);
  HAL_GPIO_ConfigPinAttributes(GPIOA,GPIO_PIN_1,GPIO_PIN_NSEC|GPIO_PIN_NPRIV);
  HAL_GPIO_ConfigPinAttributes(GPIOA,GPIO_PIN_2,GPIO_PIN_NSEC|GPIO_PIN_NPRIV);
  HAL_GPIO_ConfigPinAttributes(GPIOA,GPIO_PIN_7,GPIO_PIN_NSEC|GPIO_PIN_NPRIV);
  HAL_GPIO_ConfigPinAttributes(GPIOA,GPIO_PIN_8,GPIO_PIN_NSEC|GPIO_PIN_NPRIV);
  HAL_GPIO_ConfigPinAttributes(GPIOA,GPIO_PIN_11,GPIO_PIN_SEC|GPIO_PIN_NPRIV);
  HAL_GPIO_ConfigPinAttributes(GPIOB,GPIO_PIN_0,GPIO_PIN_SEC|GPIO_PIN_NPRIV);
  /* GPIOB Non Secure Ports Clock Enable */
  __HAL_RCC_GPIOB_CLK_ENABLE();
  HAL_GPIO_ConfigPinAttributes(GPIOB,GPIO_PIN_2,GPIO_PIN_NSEC|GPIO_PIN_NPRIV);
  HAL_GPIO_ConfigPinAttributes(GPIOB,GPIO_PIN_6,GPIO_PIN_SEC|GPIO_PIN_NPRIV);
  HAL_GPIO_ConfigPinAttributes(GPIOB,GPIO_PIN_7,GPIO_PIN_SEC|GPIO_PIN_NPRIV);
  HAL_GPIO_ConfigPinAttributes(GPIOB,GPIO_PIN_11,GPIO_PIN_NSEC|GPIO_PIN_NPRIV);
  HAL_GPIO_ConfigPinAttributes(GPIOB,GPIO_PIN_12,GPIO_PIN_NSEC|GPIO_PIN_NPRIV);
  HAL_GPIO_ConfigPinAttributes(GPIOB,GPIO_PIN_14,GPIO_PIN_NSEC|GPIO_PIN_NPRIV);
  HAL_GPIO_ConfigPinAttributes(GPIOB,GPIO_PIN_15,GPIO_PIN_NSEC|GPIO_PIN_NPRIV);
  HAL_GPIO_ConfigPinAttributes(GPIOC,GPIO_PIN_0,GPIO_PIN_SEC|GPIO_PIN_NPRIV);
  HAL_GPIO_ConfigPinAttributes(GPIOC,GPIO_PIN_1,GPIO_PIN_SEC|GPIO_PIN_NPRIV);
  HAL_GPIO_ConfigPinAttributes(GPIOC,GPIO_PIN_2,GPIO_PIN_SEC|GPIO_PIN_NPRIV);
  HAL_GPIO_ConfigPinAttributes(GPIOC,GPIO_PIN_3,GPIO_PIN_SEC|GPIO_PIN_NPRIV);
  HAL_GPIO_ConfigPinAttributes(GPIOC,GPIO_PIN_4,GPIO_PIN_SEC|GPIO_PIN_NPRIV);
  HAL_GPIO_ConfigPinAttributes(GPIOC,GPIO_PIN_5,GPIO_PIN_SEC|GPIO_PIN_NPRIV);
  HAL_GPIO_ConfigPinAttributes(GPIOC,GPIO_PIN_13,GPIO_PIN_SEC|GPIO_PIN_NPRIV);
  /* GPIOD Non Secure Ports Clock Enable */
  __HAL_RCC_GPIOD_CLK_ENABLE();
  HAL_GPIO_ConfigPinAttributes(GPIOD,GPIO_PIN_1,GPIO_PIN_NSEC|GPIO_PIN_NPRIV);
  HAL_GPIO_ConfigPinAttributes(GPIOD,GPIO_PIN_3,GPIO_PIN_NSEC|GPIO_PIN_NPRIV);
  HAL_GPIO_ConfigPinAttributes(GPIOD,GPIO_PIN_4,GPIO_PIN_SEC|GPIO_PIN_NPRIV);
  HAL_GPIO_ConfigPinAttributes(GPIOD,GPIO_PIN_5,GPIO_PIN_NSEC|GPIO_PIN_NPRIV);
  HAL_GPIO_ConfigPinAttributes(GPIOD,GPIO_PIN_8,GPIO_PIN_NSEC|GPIO_PIN_NPRIV);
  HAL_GPIO_ConfigPinAttributes(GPIOD,GPIO_PIN_9,GPIO_PIN_NSEC|GPIO_PIN_NPRIV);
  HAL_GPIO_ConfigPinAttributes(GPIOD,GPIO_PIN_12,GPIO_PIN_NSEC|GPIO_PIN_NPRIV);
  HAL_GPIO_ConfigPinAttributes(GPIOD,GPIO_PIN_14,GPIO_PIN_SEC|GPIO_PIN_NPRIV);
  HAL_GPIO_ConfigPinAttributes(GPIOD,GPIO_PIN_15,GPIO_PIN_NSEC|GPIO_PIN_NPRIV);
  HAL_GPIO_ConfigPinAttributes(GPIOE,GPIO_PIN_2,GPIO_PIN_SEC|GPIO_PIN_NPRIV);
  HAL_GPIO_ConfigPinAttributes(GPIOE,GPIO_PIN_3,GPIO_PIN_SEC|GPIO_PIN_NPRIV);
  HAL_GPIO_ConfigPinAttributes(GPIOE,GPIO_PIN_4,GPIO_PIN_SEC|GPIO_PIN_NPRIV);
  HAL_GPIO_ConfigPinAttributes(GPIOE,GPIO_PIN_5,GPIO_PIN_SEC|GPIO_PIN_NPRIV);
  HAL_GPIO_ConfigPinAttributes(GPIOE,GPIO_PIN_6,GPIO_PIN_SEC|GPIO_PIN_NPRIV);
  HAL_GPIO_ConfigPinAttributes(GPIOE,GPIO_PIN_7,GPIO_PIN_SEC|GPIO_PIN_NPRIV);
  HAL_GPIO_ConfigPinAttributes(GPIOE,GPIO_PIN_8,GPIO_PIN_SEC|GPIO_PIN_NPRIV);
  /* GPIOE Non Secure Ports Clock Enable */
  __HAL_RCC_GPIOE_CLK_ENABLE();
  HAL_GPIO_ConfigPinAttributes(GPIOE,GPIO_PIN_11,GPIO_PIN_NSEC|GPIO_PIN_NPRIV);
  /* GPIOF Non Secure Ports Clock Enable */
  __HAL_RCC_GPIOF_CLK_ENABLE();
  HAL_GPIO_ConfigPinAttributes(GPIOF,GPIO_PIN_0,GPIO_PIN_NSEC|GPIO_PIN_NPRIV);
  HAL_GPIO_ConfigPinAttributes(GPIOF,GPIO_PIN_2,GPIO_PIN_NSEC|GPIO_PIN_NPRIV);
  HAL_GPIO_ConfigPinAttributes(GPIOF,GPIO_PIN_5,GPIO_PIN_NSEC|GPIO_PIN_NPRIV);
  HAL_GPIO_ConfigPinAttributes(GPIOF,GPIO_PIN_6,GPIO_PIN_NSEC|GPIO_PIN_NPRIV);
  HAL_GPIO_ConfigPinAttributes(GPIOF,GPIO_PIN_7,GPIO_PIN_NSEC|GPIO_PIN_NPRIV);
  HAL_GPIO_ConfigPinAttributes(GPIOF,GPIO_PIN_8,GPIO_PIN_NSEC|GPIO_PIN_NPRIV);
  HAL_GPIO_ConfigPinAttributes(GPIOF,GPIO_PIN_9,GPIO_PIN_NSEC|GPIO_PIN_NPRIV);
  HAL_GPIO_ConfigPinAttributes(GPIOF,GPIO_PIN_10,GPIO_PIN_NSEC|GPIO_PIN_NPRIV);
  HAL_GPIO_ConfigPinAttributes(GPIOF,GPIO_PIN_11,GPIO_PIN_NSEC|GPIO_PIN_NPRIV);
  HAL_GPIO_ConfigPinAttributes(GPIOF,GPIO_PIN_12,GPIO_PIN_NSEC|GPIO_PIN_NPRIV);
  HAL_GPIO_ConfigPinAttributes(GPIOF,GPIO_PIN_13,GPIO_PIN_NSEC|GPIO_PIN_NPRIV);
  HAL_GPIO_ConfigPinAttributes(GPIOF,GPIO_PIN_14,GPIO_PIN_NSEC|GPIO_PIN_NPRIV);
  HAL_GPIO_ConfigPinAttributes(GPIOF,GPIO_PIN_15,GPIO_PIN_NSEC|GPIO_PIN_NPRIV);
  /* GPIOG Non Secure Ports Clock Enable */
  __HAL_RCC_GPIOG_CLK_ENABLE();
  HAL_GPIO_ConfigPinAttributes(GPIOG,GPIO_PIN_1,GPIO_PIN_NSEC|GPIO_PIN_NPRIV);
  HAL_GPIO_ConfigPinAttributes(GPIOG,GPIO_PIN_3,GPIO_PIN_NSEC|GPIO_PIN_NPRIV);
  HAL_GPIO_ConfigPinAttributes(GPIOG,GPIO_PIN_4,GPIO_PIN_NSEC|GPIO_PIN_NPRIV);
  HAL_GPIO_ConfigPinAttributes(GPIOG,GPIO_PIN_6,GPIO_PIN_NSEC|GPIO_PIN_NPRIV);
  HAL_GPIO_ConfigPinAttributes(GPIOG,GPIO_PIN_7,GPIO_PIN_SEC|GPIO_PIN_NPRIV);
  HAL_GPIO_ConfigPinAttributes(GPIOG,GPIO_PIN_8,GPIO_PIN_NSEC|GPIO_PIN_NPRIV);
  HAL_GPIO_ConfigPinAttributes(GPIOG,GPIO_PIN_11,GPIO_PIN_NSEC|GPIO_PIN_NPRIV);
  HAL_GPIO_ConfigPinAttributes(GPIOG,GPIO_PIN_12,GPIO_PIN_NSEC|GPIO_PIN_NPRIV);
  HAL_GPIO_ConfigPinAttributes(GPIOG,GPIO_PIN_13,GPIO_PIN_NSEC|GPIO_PIN_NPRIV);
  HAL_GPIO_ConfigPinAttributes(GPIOG,GPIO_PIN_15,GPIO_PIN_NSEC|GPIO_PIN_NPRIV);
  /* GPIOH Non Secure Ports Clock Enable */
  __HAL_RCC_GPIOH_CLK_ENABLE();
  HAL_GPIO_ConfigPinAttributes(GPIOH,GPIO_PIN_3,GPIO_PIN_NSEC|GPIO_PIN_NPRIV);
  HAL_GPIO_ConfigPinAttributes(GPIOH,GPIO_PIN_4,GPIO_PIN_NSEC|GPIO_PIN_NPRIV);
  HAL_GPIO_ConfigPinAttributes(GPIOH,GPIO_PIN_6,GPIO_PIN_NSEC|GPIO_PIN_NPRIV);
  HAL_GPIO_ConfigPinAttributes(GPIOH,GPIO_PIN_9,GPIO_PIN_SEC|GPIO_PIN_NPRIV);
  HAL_GPIO_ConfigPinAttributes(GPION,GPIO_PIN_0,GPIO_PIN_SEC|GPIO_PIN_NPRIV);
  HAL_GPIO_ConfigPinAttributes(GPION,GPIO_PIN_1,GPIO_PIN_SEC|GPIO_PIN_NPRIV);
  HAL_GPIO_ConfigPinAttributes(GPION,GPIO_PIN_2,GPIO_PIN_SEC|GPIO_PIN_NPRIV);
  HAL_GPIO_ConfigPinAttributes(GPION,GPIO_PIN_3,GPIO_PIN_SEC|GPIO_PIN_NPRIV);
  HAL_GPIO_ConfigPinAttributes(GPION,GPIO_PIN_4,GPIO_PIN_SEC|GPIO_PIN_NPRIV);
  HAL_GPIO_ConfigPinAttributes(GPION,GPIO_PIN_5,GPIO_PIN_SEC|GPIO_PIN_NPRIV);
  HAL_GPIO_ConfigPinAttributes(GPION,GPIO_PIN_6,GPIO_PIN_SEC|GPIO_PIN_NPRIV);
  HAL_GPIO_ConfigPinAttributes(GPION,GPIO_PIN_8,GPIO_PIN_SEC|GPIO_PIN_NPRIV);
  HAL_GPIO_ConfigPinAttributes(GPION,GPIO_PIN_9,GPIO_PIN_SEC|GPIO_PIN_NPRIV);
  HAL_GPIO_ConfigPinAttributes(GPION,GPIO_PIN_10,GPIO_PIN_SEC|GPIO_PIN_NPRIV);
  HAL_GPIO_ConfigPinAttributes(GPION,GPIO_PIN_11,GPIO_PIN_SEC|GPIO_PIN_NPRIV);
  HAL_GPIO_ConfigPinAttributes(GPIOO,GPIO_PIN_0,GPIO_PIN_SEC|GPIO_PIN_NPRIV);
  HAL_GPIO_ConfigPinAttributes(GPIOO,GPIO_PIN_2,GPIO_PIN_SEC|GPIO_PIN_NPRIV);
  HAL_GPIO_ConfigPinAttributes(GPIOO,GPIO_PIN_3,GPIO_PIN_SEC|GPIO_PIN_NPRIV);
  HAL_GPIO_ConfigPinAttributes(GPIOO,GPIO_PIN_4,GPIO_PIN_SEC|GPIO_PIN_NPRIV);
  HAL_GPIO_ConfigPinAttributes(GPIOP,GPIO_PIN_0,GPIO_PIN_SEC|GPIO_PIN_NPRIV);
  HAL_GPIO_ConfigPinAttributes(GPIOP,GPIO_PIN_1,GPIO_PIN_SEC|GPIO_PIN_NPRIV);
  HAL_GPIO_ConfigPinAttributes(GPIOP,GPIO_PIN_2,GPIO_PIN_SEC|GPIO_PIN_NPRIV);
  HAL_GPIO_ConfigPinAttributes(GPIOP,GPIO_PIN_3,GPIO_PIN_SEC|GPIO_PIN_NPRIV);
  HAL_GPIO_ConfigPinAttributes(GPIOP,GPIO_PIN_4,GPIO_PIN_SEC|GPIO_PIN_NPRIV);
  HAL_GPIO_ConfigPinAttributes(GPIOP,GPIO_PIN_5,GPIO_PIN_SEC|GPIO_PIN_NPRIV);
  HAL_GPIO_ConfigPinAttributes(GPIOP,GPIO_PIN_6,GPIO_PIN_SEC|GPIO_PIN_NPRIV);
  HAL_GPIO_ConfigPinAttributes(GPIOP,GPIO_PIN_7,GPIO_PIN_SEC|GPIO_PIN_NPRIV);
  HAL_GPIO_ConfigPinAttributes(GPIOP,GPIO_PIN_8,GPIO_PIN_SEC|GPIO_PIN_NPRIV);
  HAL_GPIO_ConfigPinAttributes(GPIOP,GPIO_PIN_9,GPIO_PIN_SEC|GPIO_PIN_NPRIV);
  HAL_GPIO_ConfigPinAttributes(GPIOP,GPIO_PIN_10,GPIO_PIN_SEC|GPIO_PIN_NPRIV);
  HAL_GPIO_ConfigPinAttributes(GPIOP,GPIO_PIN_11,GPIO_PIN_SEC|GPIO_PIN_NPRIV);
  HAL_GPIO_ConfigPinAttributes(GPIOP,GPIO_PIN_12,GPIO_PIN_SEC|GPIO_PIN_NPRIV);
  HAL_GPIO_ConfigPinAttributes(GPIOP,GPIO_PIN_13,GPIO_PIN_SEC|GPIO_PIN_NPRIV);
  HAL_GPIO_ConfigPinAttributes(GPIOP,GPIO_PIN_14,GPIO_PIN_SEC|GPIO_PIN_NPRIV);
  HAL_GPIO_ConfigPinAttributes(GPIOP,GPIO_PIN_15,GPIO_PIN_SEC|GPIO_PIN_NPRIV);

  /* USER CODE BEGIN RIF_Init 1 */
  boot_trace("[RIF] CubeMX config done\r\n");
  /* USER CODE END RIF_Init 1 */
  /* USER CODE BEGIN RIF_Init 2 */

  /* ---------------------------------------------------------------
   * Configurations NON generees par CubeMX — doivent rester ici.
   * Survivent a la regeneration car dans USER CODE.
   * --------------------------------------------------------------- */

  /* 1) RISUP NSEC pour peripheriques AppliNonSecure
   *    (CubeMX ne genere que des RISUP SEC pour le FSBL) */
  boot_trace("[RIF2] 1-RISUP...\r\n");
  HAL_RIF_RISC_SetSlaveSecureAttributes(RIF_RISC_PERIPH_INDEX_USART2,
                                        RIF_ATTRIBUTE_NSEC | RIF_ATTRIBUTE_NPRIV);
  /* LTDC/LTDCL1/LTDCL2 : forcer SEC+PRIV (comme BSP) pour que le DMA LTDC
   * puisse acceder SRAM3 via l'alias Secure 0x34200000. */
  HAL_RIF_RISC_SetSlaveSecureAttributes(RIF_RISC_PERIPH_INDEX_LTDC,
                                        RIF_ATTRIBUTE_SEC | RIF_ATTRIBUTE_PRIV);
  HAL_RIF_RISC_SetSlaveSecureAttributes(RIF_RISC_PERIPH_INDEX_LTDCL1,
                                        RIF_ATTRIBUTE_SEC | RIF_ATTRIBUTE_PRIV);
  HAL_RIF_RISC_SetSlaveSecureAttributes(RIF_RISC_PERIPH_INDEX_LTDCL2,
                                        RIF_ATTRIBUTE_SEC | RIF_ATTRIBUTE_PRIV);

  boot_trace("[RIF2] 1-RISUP OK\r\n");

  /* 2) RIMC master : LTDC L1 + L2 DMA avec CID=1, SEC+PRIV
   *    Le DMA LTDC doit acceder la SRAM3 via l'alias Secure (0x34200000)
   *    car l'acces NS a SRAM3 est bloque par l'interconnect NoC.
   *    Comme l'exemple BSP ST : LTDC master = SEC + PRIV. */
  {
    RIMC_MasterConfig_t ltdc_master = {0};
    ltdc_master.MasterCID = RIF_CID_1;
    ltdc_master.SecPriv   = RIF_ATTRIBUTE_SEC | RIF_ATTRIBUTE_PRIV;
    HAL_RIF_RIMC_ConfigMasterAttributes(RIF_MASTER_INDEX_LTDC1, &ltdc_master);
    HAL_RIF_RIMC_ConfigMasterAttributes(RIF_MASTER_INDEX_LTDC2, &ltdc_master);
    /* Bit 31 : maitre vu comme « DMA » par le RIF — requis pour que RISAF autorise
     * les lectures LTDC vers la SRAM ; HAL_RIF_RIMC_ConfigMasterAttributes ne le pose pas. */
    RIFSC->RIMC_ATTRx[RIF_MASTER_INDEX_LTDC1] |= (1UL << 31);
    RIFSC->RIMC_ATTRx[RIF_MASTER_INDEX_LTDC2] |= (1UL << 31);
    __DSB();
  }

  boot_trace("[RIF2] 2-RIMC OK\r\n");

  /* 3) GPIOQ — LCD_ON (PQ3) et backlight (PQ6)
   *    (CubeMX ne genere aucun code pour GPIOQ) */
  __HAL_RCC_GPIOQ_CLK_ENABLE();
  HAL_GPIO_ConfigPinAttributes(GPIOQ, GPIO_PIN_3, GPIO_PIN_NSEC | GPIO_PIN_NPRIV);
  HAL_GPIO_ConfigPinAttributes(GPIOQ, GPIO_PIN_6, GPIO_PIN_NSEC | GPIO_PIN_NPRIV);

  /* 4) Pins LCD ignores par CubeMX — doivent etre NSEC pour AppliNonSecure */
  /* PA15 (LTDC_R5) et PB4 (LTDC_R3) — JTAG alias, absents du .ioc */
  HAL_GPIO_ConfigPinAttributes(GPIOA, GPIO_PIN_15, GPIO_PIN_NSEC | GPIO_PIN_NPRIV);
  HAL_GPIO_ConfigPinAttributes(GPIOB, GPIO_PIN_4,  GPIO_PIN_NSEC | GPIO_PIN_NPRIV);

  /* PB13 (LCD_CLK) — CRITIQUE : pixel clock ! Absent du .ioc.
   * Sans ce flag NSEC, le HAL_GPIO_Init depuis AppliNonSecure est
   * SILENCIEUSEMENT IGNORE par TrustZone → pas de clock pixel → ecran noir */
  HAL_GPIO_ConfigPinAttributes(GPIOB, GPIO_PIN_13, GPIO_PIN_NSEC | GPIO_PIN_NPRIV);

  /* PE1 (LCD_NRST) — reset panneau LCD, absent du .ioc */
  HAL_GPIO_ConfigPinAttributes(GPIOE, GPIO_PIN_1,  GPIO_PIN_NSEC | GPIO_PIN_NPRIV);

  /* PG0 (LCD_R0) — absent du .ioc (R0 = LSB rouge, non utilise en RGB565
   * mais doit quand meme etre NSEC pour eviter un fault si on y accede) */
  HAL_GPIO_ConfigPinAttributes(GPIOG, GPIO_PIN_0,  GPIO_PIN_NSEC | GPIO_PIN_NPRIV);

  /* 4b) Debug board LEDs — explicitement NSEC pour que les fault handlers NS
   *     puissent signaler les erreurs sans double-fault */
  HAL_GPIO_ConfigPinAttributes(GPIOD, GPIO_PIN_0,  GPIO_PIN_NSEC | GPIO_PIN_NPRIV); /* GREEN  */
  HAL_GPIO_ConfigPinAttributes(GPIOE, GPIO_PIN_9,  GPIO_PIN_NSEC | GPIO_PIN_NPRIV); /* YELLOW */
  HAL_GPIO_ConfigPinAttributes(GPIOH, GPIO_PIN_5,  GPIO_PIN_NSEC | GPIO_PIN_NPRIV); /* RED    */
  HAL_GPIO_ConfigPinAttributes(GPIOE, GPIO_PIN_10, GPIO_PIN_NSEC | GPIO_PIN_NPRIV); /* BLUE   */
  HAL_GPIO_ConfigPinAttributes(GPIOE, GPIO_PIN_13, GPIO_PIN_NSEC | GPIO_PIN_NPRIV); /* WHITE  */

  boot_trace("[RIF2] 4-GPIO OK\r\n");

  /* 5) RISAF4 — ouvrir SRAM3+SRAM4 en NS pour le framebuffer LTDC
   *    RISAF4 protege NPU port 0 = SRAM3(0x24200000)+SRAM4(0x24270000)
   *    Espace d'adresse = 4 GB → adresses absolues dans STARTR/ENDR
   *    On utilise Region 1 (index 0 dans le tableau REG[]) */

  /* 5a) Activer RISAF (AHB3) + RAMCFG (AHB2) horloges */
  boot_trace("[5a] RISAF+RAMCFG clk\r\n");
  __HAL_RCC_RISAF_CLK_ENABLE();
  __HAL_RCC_RAMCFG_CLK_ENABLE();
  __DSB();

  /* 5b) Activer SRAM3/SRAM4 horloges memoire.
   *     Utilisation de CMSIS direct dans les deux alias (S et NS) car le
   *     mecanisme exact de protection RCC varie selon le state TZ. */
  boot_trace("[5b] MEMENSR\r\n");
  RCC_S->MEMENSR = RCC_MEMENR_AXISRAM3EN | RCC_MEMENR_AXISRAM4EN;
  __DSB();
  /* Fallback : forcer aussi via ENR direct */
  if (!(RCC_S->MEMENR & (RCC_MEMENR_AXISRAM3EN | RCC_MEMENR_AXISRAM4EN)))
  {
    RCC_S->MEMENR |= (RCC_MEMENR_AXISRAM3EN | RCC_MEMENR_AXISRAM4EN);
    __DSB();
  }
  /* Rendre TOUTES les ressources RCC publiques (NS accessible) via PUBCFGR.
   * Sans cela, le code NS ne peut pas activer les clocks GPIO, USART, etc. */
  boot_trace("[5b] PUBCFGR ALL\r\n");
  RCC_S->PUBCFGR0 = 0xFFFFFFFFU;  /* oscillators */
  RCC_S->PUBCFGR1 = 0xFFFFFFFFU;  /* PLLs */
  RCC_S->PUBCFGR2 = 0xFFFFFFFFU;  /* dividers (IC16...) */
  RCC_S->PUBCFGR3 = 0xFFFFFFFFU;  /* CCIPR peripherals mux */
  RCC_S->PUBCFGR4 = 0xFFFFFFFFU;  /* bus clocks (AHB1-5, APB1-5) */
  RCC_S->PUBCFGR5 = 0xFFFFFFFFU;  /* memory clocks (SRAMs) */
  __DSB();

  /* 5c) Sortir SRAM3/SRAM4 du mode shutdown via RAMCFG */
  boot_trace("[5c] RAMCFG\r\n");
  CLEAR_BIT(RAMCFG_SRAM3_AXI_S->CR, RAMCFG_CR_SRAMSD);
  CLEAR_BIT(RAMCFG_SRAM4_AXI_S->CR, RAMCFG_CR_SRAMSD);
  __DSB();

  /* 5c2) RISAF3 — Ouvrir SRAM2 en NS pour AppliNonSecure
   *      RISAF3 protege AXISRAM2 (0x24100000-0x241FFFFF).
   *      Par defaut aucun acces NS n'est autorise → le CPU NS ne peut
   *      ni fetcher les instructions ni ecrire sur la stack → IBUSERR.
   *      On configure une region couvrant tout SRAM2 en NSEC, tous CIDs. */
  boot_trace("[5c2] RISAF3 SRAM2\r\n");
  {
    RISAF_TypeDef *risaf3 = RISAF3_S;

    /* Diagnostic : afficher CR */
    {
      volatile uint32_t cr = risaf3->CR;
      boot_trace("[5c2] CR=0x");
      char h[9]; for(int i=7;i>=0;i--){uint8_t n=(cr>>(i*4))&0xF; h[7-i]=n<10?'0'+n:'A'+n-10;} h[8]=0;
      boot_trace(h); boot_trace("\r\n");
    }

    /* Desactiver region 0 avant configuration */
    risaf3->REG[0].CFGR = 0;
    __DSB();

    /* Effacer les flags d'acces illegaux */
    risaf3->IACR = risaf3->IASR;
    __DSB();

    /* Region 0 : couvre tout l'espace SRAM2 */
    risaf3->REG[0].STARTR  = 0x00000000UL;   /* debut relatif */
    risaf3->REG[0].ENDR    = 0xFFFFFFFFUL;   /* tout l'espace */
    risaf3->REG[0].CIDCFGR = 0x00FF00FFU;    /* tous CIDs R+W */
    /* CFGR: BREN=1 (enable), SEC=0 (NSEC) → acces NS autorise */
    risaf3->REG[0].CFGR    = RISAF_REGx_CFGR_BREN;
    __DSB();
    boot_trace("[5c2] RISAF3 OK\r\n");
  }

  /* 5d) RISAF4 — differe a la section 7 (apres remplissage FB via alias S,
   *     avant activation LTDC). NPU clock active ici car requis pour ecriture. */
  boot_trace("[5d] RISAF4 => sec7\r\n");
  __HAL_RCC_NPU_CLK_ENABLE();
  __DSB();

  /* 5e) RISAF2 — TEMPORAIREMENT DESACTIVE pour debug */
  boot_trace("[5e] RISAF2 SKIP\r\n");
#if 0
  {
    RISAF_TypeDef *risaf2 = RISAF2_S;

    risaf2->REG[0].CFGR = 0;
    __DSB();
    risaf2->IACR = risaf2->IASR;
    __DSB();
  }
#endif

  /* 5f) MPU non-secure : framebuffer LTDC (SRAM3 0x24200000, 800x480x2) en non-cacheable.
   *     Le CM55 ecrit souvent dans le D-cache ; le LTDC lit l'AXI/SRAM hors cache.
   *     Sans cette region, seul BCCR (registre) est correct — layer FB = noir. */
  boot_trace("[5e] RISAF2 OK\r\n");
#if defined(HAL_CORTEX_MODULE_ENABLED) && defined(MPU_NS)
  boot_trace("[5f] MPU NS\r\n");
  {
    MPU_Attributes_InitTypeDef mpu_attr = {0};
    MPU_Region_InitTypeDef     mpu_reg  = {0};
    const uint32_t fb_base = 0x24200000UL;
    const uint32_t fb_size = 800UL * 480UL * 2UL;

    mpu_attr.Number     = MPU_ATTRIBUTES_NUMBER0;
    mpu_attr.Attributes = INNER_OUTER(MPU_NOT_CACHEABLE);
    HAL_MPU_ConfigMemoryAttributes_NS(&mpu_attr);

    mpu_reg.Enable            = MPU_REGION_ENABLE;
    mpu_reg.Number            = MPU_REGION_NUMBER7;
    mpu_reg.AttributesIndex   = MPU_ATTRIBUTES_NUMBER0;
    mpu_reg.BaseAddress       = fb_base;
    mpu_reg.LimitAddress      = fb_base + fb_size - 1UL;
    mpu_reg.AccessPermission  = MPU_REGION_ALL_RW;
    mpu_reg.DisableExec       = MPU_INSTRUCTION_ACCESS_DISABLE;
    mpu_reg.DisablePrivExec   = MPU_PRIV_INSTRUCTION_ACCESS_DISABLE;
    mpu_reg.IsShareable       = MPU_ACCESS_OUTER_SHAREABLE;
    HAL_MPU_ConfigRegion_NS(&mpu_reg);

    /* Fond de carte memoire NS + HF/NMI : le reste de la NS garde les attributs par defaut */
    HAL_MPU_Enable_NS(MPU_HFNMI_PRIVDEF);
  }

  /* 5f2) MPU Secure : alias S du framebuffer (0x34200000) en non-cacheable.
   *      Le remplissage du FB se fait depuis le Secure via l'alias S.
   *      Sans cette region, les ecritures vont dans le D-cache Secure
   *      (background region = cacheable) et n'atteignent jamais la SRAM.
   *      Le LTDC DMA et le CPU NS lisent la SRAM directement → ecran noir. */
  boot_trace("[5f2] MPU S FB\r\n");
  {
    MPU_Attributes_InitTypeDef mpu_attr = {0};
    MPU_Region_InitTypeDef     mpu_reg  = {0};
    const uint32_t fb_base_s = 0x34200000UL;
    const uint32_t fb_size   = 800UL * 480UL * 2UL;

    mpu_attr.Number     = MPU_ATTRIBUTES_NUMBER1;  /* index 1 pour S MPU */
    mpu_attr.Attributes = INNER_OUTER(MPU_NOT_CACHEABLE);
    HAL_MPU_ConfigMemoryAttributes(&mpu_attr);     /* Secure MPU */

    mpu_reg.Enable            = MPU_REGION_ENABLE;
    mpu_reg.Number            = MPU_REGION_NUMBER6;  /* region 6 pour S */
    mpu_reg.AttributesIndex   = MPU_ATTRIBUTES_NUMBER1;
    mpu_reg.BaseAddress       = fb_base_s;
    mpu_reg.LimitAddress      = fb_base_s + fb_size - 1UL;
    mpu_reg.AccessPermission  = MPU_REGION_ALL_RW;
    mpu_reg.DisableExec       = MPU_INSTRUCTION_ACCESS_DISABLE;
    mpu_reg.IsShareable       = MPU_ACCESS_OUTER_SHAREABLE;
    HAL_MPU_ConfigRegion(&mpu_reg);                /* Secure MPU */

    HAL_MPU_Enable(MPU_HFNMI_PRIVDEF);
  }
#endif

  boot_trace("[RIF2] 5-RISAF OK\r\n");

  /* 6) LTDC clock configuration — MUST be done from Secure context
   *    (IC16CFGR, DIVENSR, CCIPR4, APB5ENSR are Secure-only RCC registers,
   *     writes from NS are silently ignored without PUBCFGR bits) */

  /* 6a) Pixel clock : IC16 = PLL1 (1200 MHz) / 45 = 26.67 MHz */
  MODIFY_REG(RCC_S->IC16CFGR,
             RCC_IC16CFGR_IC16SEL | RCC_IC16CFGR_IC16INT,
             (0U  << RCC_IC16CFGR_IC16SEL_Pos) |   /* source = PLL1 */
             (44U << RCC_IC16CFGR_IC16INT_Pos));    /* divider-1 = 44 -> /45 */
  SET_BIT(RCC_S->DIVENSR, RCC_DIVENSR_IC16ENS);    /* enable IC16 */
  while (!(RCC_S->DIVENR & RCC_DIVENR_IC16EN)) {}  /* wait enabled */

  /* 6b) LTDC kernel clock source = IC16 */
  MODIFY_REG(RCC_S->CCIPR4, RCC_CCIPR4_LTDCSEL,
             RCC_CCIPR4_LTDCSEL_1);                 /* LTDCSEL = 10 = IC16 */

  /* 6c) LTDC peripheral clock (APB5) */
  RCC_S->APB5ENSR = RCC_APB5ENR_LTDCEN;
  __DSB();
  while (!(RCC_S->APB5ENR & RCC_APB5ENR_LTDCEN)) {}

  /* 6d) LTDC peripheral reset APRES clock enable (sequence BSP officielle) */
  RCC_S->APB5RSTSR = RCC_APB5RSTR_LTDCRST;   /* assert reset */
  for (volatile int _d = 0; _d < 100; _d++) {} /* brief delay */
  RCC_S->APB5RSTCR = RCC_APB5RSTR_LTDCRST;   /* release reset */
  __DSB();

  boot_trace("[RIF2] 6-CLK OK\r\n");

  /* ================================================================
   * 7) LTDC FULL INIT FROM SECURE
   *    Les ecritures NS vers LTDC_NS sont silencieusement ignorees
   *    (CFBAR reste a 0 apres write depuis NS). On programme donc
   *    TOUT ici avec les alias _S, puis NS n'a plus qu'a ecrire
   *    dans le framebuffer (SRAM3 0x24200000, deja NSEC).
   * ================================================================ */
  {
    /* --- 7a) GPIO AF14 pour toutes les broches LCD (comme LTDC_MspInit BSP) --- */
    GPIO_InitTypeDef gpio = {0};

    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_GPIOD_CLK_ENABLE();
    __HAL_RCC_GPIOE_CLK_ENABLE();
    __HAL_RCC_GPIOG_CLK_ENABLE();
    __HAL_RCC_GPIOH_CLK_ENABLE();
    __HAL_RCC_GPIOQ_CLK_ENABLE();

    gpio.Mode      = GPIO_MODE_AF_PP;
    gpio.Pull      = GPIO_NOPULL;
    gpio.Speed     = GPIO_SPEED_FREQ_HIGH;

    /* PA0 R1, PA1 G0, PA2 B0, PA7 B1, PA8 G1, PA15 R5 */
    gpio.Pin       = GPIO_PIN_0|GPIO_PIN_1|GPIO_PIN_2|GPIO_PIN_7|GPIO_PIN_8|GPIO_PIN_15;
    gpio.Alternate = GPIO_AF14_LCD;
    HAL_GPIO_Init(GPIOA, &gpio);

    /* PB2 G4, PB4 R3, PB11 G5, PB12 G6, PB13 CLK, PB14 VSYNC, PB15 B7 */
    gpio.Pin       = GPIO_PIN_13|GPIO_PIN_14|GPIO_PIN_2|GPIO_PIN_4|GPIO_PIN_11|GPIO_PIN_12|GPIO_PIN_15;
    gpio.Alternate = GPIO_AF14_LCD;
    HAL_GPIO_Init(GPIOB, &gpio);

    /* PD8 B6, PD9 B3, PD15 R7 */
    gpio.Pin       = GPIO_PIN_8|GPIO_PIN_9|GPIO_PIN_15;
    gpio.Alternate = GPIO_AF14_LCD;
    HAL_GPIO_Init(GPIOD, &gpio);

    /* PE11 HSYNC */
    gpio.Pin       = GPIO_PIN_11;
    gpio.Alternate = GPIO_AF14_LCD;
    HAL_GPIO_Init(GPIOE, &gpio);

    /* PG0 R0, PG1 R2, PG6 R4, PG8 G2, PG11 B2, PG12 G3 */
    gpio.Pin       = GPIO_PIN_0|GPIO_PIN_1|GPIO_PIN_6|GPIO_PIN_8|GPIO_PIN_11|GPIO_PIN_12;
    gpio.Alternate = GPIO_AF14_LCD;
    HAL_GPIO_Init(GPIOG, &gpio);

    /* PH3 B4, PH4 R4, PH6 B5 */
    gpio.Pin       = GPIO_PIN_3|GPIO_PIN_4|GPIO_PIN_6;
    gpio.Alternate = GPIO_AF14_LCD;
    HAL_GPIO_Init(GPIOH, &gpio);

    /* --- 7b) LCD power / backlight / DE / NRST (sorties PP) --- */
    gpio.Mode  = GPIO_MODE_OUTPUT_PP;
    gpio.Speed = GPIO_SPEED_FREQ_LOW;
    gpio.Pull  = GPIO_NOPULL;

    /* PQ3 LCD_ON, PQ6 LCD_BL_CTRL */
    gpio.Pin = GPIO_PIN_3|GPIO_PIN_6;
    HAL_GPIO_Init(GPIOQ, &gpio);
    HAL_GPIO_WritePin(GPIOQ, GPIO_PIN_3, GPIO_PIN_SET);
    HAL_GPIO_WritePin(GPIOQ, GPIO_PIN_6, GPIO_PIN_SET);

    /* PG13 LCD_DE (Display Enable) — avec pull-up comme BSP_LCD_DisplayOn */
    gpio.Pin  = GPIO_PIN_13;
    gpio.Pull = GPIO_PULLUP;
    HAL_GPIO_Init(GPIOG, &gpio);
    HAL_GPIO_WritePin(GPIOG, GPIO_PIN_13, GPIO_PIN_SET);

    /* PE1 LCD_NRST — cycle reset panneau */
    gpio.Pin  = GPIO_PIN_1;
    gpio.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(GPIOE, &gpio);
    HAL_GPIO_WritePin(GPIOE, GPIO_PIN_1, GPIO_PIN_RESET);
    HAL_Delay(2);
    HAL_GPIO_WritePin(GPIOE, GPIO_PIN_1, GPIO_PIN_SET);
    HAL_Delay(120);

    /* --- 7c) Programmer les registres LTDC via alias Secure --- */
    LTDC_TypeDef       *ltdc  = LTDC_S;
    LTDC_Layer_TypeDef *layer = LTDC_Layer1_S;

    /* Diagnostic : adresses LTDC + PPSR protection status */
    {
      boot_trace("[7c] LTDC_S=0x");
      { uint32_t v=(uint32_t)ltdc; char h[9]; for(int i=7;i>=0;i--){uint8_t n=(v>>(i*4))&0xF; h[7-i]=n<10?'0'+n:'A'+n-10;} h[8]=0; boot_trace(h); }
      boot_trace(" L1_S=0x");
      { uint32_t v=(uint32_t)layer; char h[9]; for(int i=7;i>=0;i--){uint8_t n=(v>>(i*4))&0xF; h[7-i]=n<10?'0'+n:'A'+n-10;} h[8]=0; boot_trace(h); }
      boot_trace("\r\n");

      /* PPSR3 = protection status pour REG3 (ou se trouve LTDC/LTDCL1/LTDCL2) */
      volatile uint32_t ppsr3 = RIFSC_S->PPSRx[3];
      boot_trace("[7c] PPSR3=0x");
      { char h[9]; for(int i=7;i>=0;i--){uint8_t n=(ppsr3>>(i*4))&0xF; h[7-i]=n<10?'0'+n:'A'+n-10;} h[8]=0; boot_trace(h); }
      boot_trace("\r\n");

      /* SEC + PRIV status */
      volatile uint32_t sec3  = RIFSC_S->RISC_SECCFGRx[3];
      volatile uint32_t priv3 = RIFSC_S->RISC_PRIVCFGRx[3];
      volatile uint32_t lock3 = RIFSC_S->RISC_RCFGLOCKRx[3];
      boot_trace("[7c] SEC3=0x");
      { char h[9]; for(int i=7;i>=0;i--){uint8_t n=(sec3>>(i*4))&0xF; h[7-i]=n<10?'0'+n:'A'+n-10;} h[8]=0; boot_trace(h); }
      boot_trace(" PRV3=0x");
      { char h[9]; for(int i=7;i>=0;i--){uint8_t n=(priv3>>(i*4))&0xF; h[7-i]=n<10?'0'+n:'A'+n-10;} h[8]=0; boot_trace(h); }
      boot_trace(" LCK3=0x");
      { char h[9]; for(int i=7;i>=0;i--){uint8_t n=(lock3>>(i*4))&0xF; h[7-i]=n<10?'0'+n:'A'+n-10;} h[8]=0; boot_trace(h); }
      boot_trace("\r\n");

      /* Test ecriture shadow + per-layer reload :
       * Sur N6, la lecture CFBAR renvoie le registre ACTIF (pas shadow).
       * Ecrire shadow → IMR reload → relire actif = valeur ecrite. */
      layer->CFBAR = 0xDEADBEEFU;
      __DSB();
      layer->RCR = LTDC_LxRCR_IMR;   /* per-layer reload shadow → actif */
      __DSB();
      while ((layer->RCR & LTDC_LxRCR_IMR) != 0U) {}
      volatile uint32_t cfbar_test = layer->CFBAR;
      boot_trace("[7c] WR CFBAR=DEADBEEF IMR RD=0x");
      { char h[9]; for(int i=7;i>=0;i--){uint8_t n=(cfbar_test>>(i*4))&0xF; h[7-i]=n<10?'0'+n:'A'+n-10;} h[8]=0; boot_trace(h); }
      boot_trace("\r\n");
      /* Remettre CFBAR shadow a 0 pour la suite */
      layer->CFBAR = 0U;
      layer->RCR = LTDC_LxRCR_IMR;
      __DSB();
      while ((layer->RCR & LTDC_LxRCR_IMR) != 0U) {}
    }

    ltdc->GCR &= ~LTDC_GCR_LTDCEN;
    __DSB();
    ltdc->GCR = 0U;
    __DSB();

    LTDC_Layer1_S->CR    = 0U;
    LTDC_Layer1_S->CFBAR = 0U;
    LTDC_Layer1_S->BFCR  = 0U;
    LTDC_Layer2_S->CR    = 0U;
    LTDC_Layer2_S->CFBAR = 0U;
    LTDC_Layer2_S->BFCR  = 0U;

    /* RK050HR18 : 800x480, HSYNC/HBP/HFP/VSYNC/VBP/VFP = 4 */
    const uint32_t w = 800U, h = 480U;
    const uint32_t hsw = 4U, hbp = 4U, hfp = 4U;
    const uint32_t vsw = 4U, vbp = 4U, vfp = 4U;

    uint32_t accum_hbp = hsw + hbp - 1U;
    uint32_t accum_vbp = vsw + vbp - 1U;
    uint32_t accum_aw  = hsw + w + hbp - 1U;
    uint32_t accum_ah  = vsw + h + vbp - 1U;
    uint32_t total_w   = hsw + w + hbp + hfp - 1U;
    uint32_t total_h   = vsw + h + vbp + vfp - 1U;

    ltdc->SSCR = ((hsw - 1U) << LTDC_SSCR_HSW_Pos) | ((vsw - 1U) << LTDC_SSCR_VSH_Pos);
    ltdc->BPCR = (accum_hbp << LTDC_BPCR_AHBP_Pos) | (accum_vbp << LTDC_BPCR_AVBP_Pos);
    ltdc->AWCR = (accum_aw  << LTDC_AWCR_AAW_Pos)  | (accum_ah  << LTDC_AWCR_AAH_Pos);
    ltdc->TWCR = (total_w   << LTDC_TWCR_TOTALW_Pos)| (total_h   << LTDC_TWCR_TOTALH_Pos);

    /* HSPOL/VSPOL/DEPOL = active-low (0), PCPOL = IPC (inverted) */
    ltdc->GCR = LTDC_GCR_BCKEN | LTDC_GCR_PCPOL;
    ltdc->BCCR = 0x00000000U;

    uint32_t h_start = accum_hbp + 1U;
    uint32_t h_stop  = accum_hbp + w;
    uint32_t v_start = accum_vbp + 1U;
    uint32_t v_stop  = accum_vbp + h;

    layer->WHPCR  = (h_start << 0) | (h_stop << 16);
    layer->WVPCR  = (v_start << 0) | (v_stop << 16);
    layer->PFCR   = 2U;           /* RGB565 */
    layer->CACR   = 0xFFU;        /* alpha constant = opaque */
    layer->DCCR   = 0x00000000U;
    layer->BFCR   = (0x06U << LTDC_LxBFCR_BF1_Pos) | (0x07U << LTDC_LxBFCR_BF2_Pos);
    layer->CFBAR  = 0x34200000U;  /* framebuffer SRAM3 (adresse S car DMA LTDC = SEC+PRIV) */
    layer->CFBLR  = ((w * 2U) << 16) | ((w * 2U) + 7U);
    layer->CFBLNR = h;

    /* Diagnostic immediat : les ecritures Layer ont-elles ete acceptees ? */
    {
      volatile uint32_t cfbar_chk = LTDC_Layer1_S->CFBAR;
      volatile uint32_t whpcr_chk = LTDC_Layer1_S->WHPCR;
      volatile uint32_t cr_chk    = LTDC_Layer1_S->CR;
      boot_trace("[7c] CFBAR=0x");
      { char h[9]; for(int i=7;i>=0;i--){uint8_t n=(cfbar_chk>>(i*4))&0xF; h[7-i]=n<10?'0'+n:'A'+n-10;} h[8]=0; boot_trace(h); }
      boot_trace(" WHPCR=0x");
      { char h[9]; for(int i=7;i>=0;i--){uint8_t n=(whpcr_chk>>(i*4))&0xF; h[7-i]=n<10?'0'+n:'A'+n-10;} h[8]=0; boot_trace(h); }
      boot_trace("\r\n");
    }

    /* --- 7d1) Configurer RISAF4 NSEC AVANT le remplissage FB. */
    {
      RISAF_TypeDef *risaf4 = RISAF4_S;

      /* Meme sequence que RISAF3 qui fonctionne : disable → IACR → config → enable */
      risaf4->REG[0].CFGR = 0U;   /* disable region 0 */
      __DSB();
      risaf4->IACR = risaf4->IASR; /* clear pending violations */
      __DSB();

      risaf4->REG[0].STARTR  = 0x00000000UL;
      risaf4->REG[0].ENDR    = 0xFFFFFFFFUL;
      risaf4->REG[0].CIDCFGR = 0x00FF00FFU;
      risaf4->REG[0].CFGR    = RISAF_REGx_CFGR_BREN;
      __DSB();

      /* Readback RISAF4 pour verification */
      volatile uint32_t r4_cfgr = risaf4->REG[0].CFGR;
      volatile uint32_t r4_cid  = risaf4->REG[0].CIDCFGR;
      volatile uint32_t r4_sr   = risaf4->REG[0].STARTR;
      volatile uint32_t r4_er   = risaf4->REG[0].ENDR;
      boot_trace("[7d1] R4 CFGR=0x");
      { char h[9]; for(int i=7;i>=0;i--){uint8_t n=(r4_cfgr>>(i*4))&0xF; h[7-i]=n<10?'0'+n:'A'+n-10;} h[8]=0; boot_trace(h); }
      boot_trace(" CID=0x");
      { char h[9]; for(int i=7;i>=0;i--){uint8_t n=(r4_cid>>(i*4))&0xF; h[7-i]=n<10?'0'+n:'A'+n-10;} h[8]=0; boot_trace(h); }
      boot_trace("\r\n");
      boot_trace("[7d1] SR=0x");
      { char h[9]; for(int i=7;i>=0;i--){uint8_t n=(r4_sr>>(i*4))&0xF; h[7-i]=n<10?'0'+n:'A'+n-10;} h[8]=0; boot_trace(h); }
      boot_trace(" ER=0x");
      { char h[9]; for(int i=7;i>=0;i--){uint8_t n=(r4_er>>(i*4))&0xF; h[7-i]=n<10?'0'+n:'A'+n-10;} h[8]=0; boot_trace(h); }
      boot_trace("\r\n");
    }

    /* --- 7d) Remplir le framebuffer avec du BLANC (RGB565 = 0xFFFF) ---
     *    Utilise l'alias S (0x34200000) qui est maintenant non-cacheable
     *    grace a la MPU Secure (section 5f2). Les ecritures vont
     *    directement en SRAM, pas de D-cache flush necessaire. */
    {
      volatile uint16_t *fb_s = (volatile uint16_t *)0x34200000UL;
      /* Ecrire un pattern reconnaissable : premiers pixels = 0xA5A5 */
      fb_s[0] = 0xA5A5U;
      fb_s[1] = 0x5A5AU;
      fb_s[2] = 0xFFFFU;
      __DSB();

      /* Test immediat : relire via S et NS */
      volatile uint16_t test_s0  = *(volatile uint16_t *)0x34200000UL;
      volatile uint16_t test_ns0 = *(volatile uint16_t *)0x24200000UL;
      volatile uint16_t test_s1  = *(volatile uint16_t *)0x34200002UL;
      volatile uint16_t test_ns1 = *(volatile uint16_t *)0x24200002UL;

      boot_trace("[7d] S[0]=0x");
      { uint32_t v=test_s0; char h[5]; for(int i=3;i>=0;i--){uint8_t n=(v>>(i*4))&0xF; h[3-i]=n<10?'0'+n:'A'+n-10;} h[4]=0; boot_trace(h); }
      boot_trace(" NS[0]=0x");
      { uint32_t v=test_ns0; char h[5]; for(int i=3;i>=0;i--){uint8_t n=(v>>(i*4))&0xF; h[3-i]=n<10?'0'+n:'A'+n-10;} h[4]=0; boot_trace(h); }
      boot_trace(" S[1]=0x");
      { uint32_t v=test_s1; char h[5]; for(int i=3;i>=0;i--){uint8_t n=(v>>(i*4))&0xF; h[3-i]=n<10?'0'+n:'A'+n-10;} h[4]=0; boot_trace(h); }
      boot_trace(" NS[1]=0x");
      { uint32_t v=test_ns1; char h[5]; for(int i=3;i>=0;i--){uint8_t n=(v>>(i*4))&0xF; h[3-i]=n<10?'0'+n:'A'+n-10;} h[4]=0; boot_trace(h); }
      boot_trace("\r\n");

      /* Verifier si RISAF4 a un flag d'acces illegal apres lecture NS */
      volatile uint32_t r4_iasr = RISAF4_S->IASR;
      boot_trace("[7d] R4 IASR=0x");
      { char h[9]; for(int i=7;i>=0;i--){uint8_t n=(r4_iasr>>(i*4))&0xF; h[7-i]=n<10?'0'+n:'A'+n-10;} h[8]=0; boot_trace(h); }
      boot_trace("\r\n");

      /* Remplir le reste en blanc */
      for (uint32_t i = 3; i < w * h; i++) {
        fb_s[i] = 0xFFFFU;
      }
      __DSB();
    }

    /* --- 7e) Activer layer 1 + LTDC + rechargement immediat --- */
    layer->CR = LTDC_LxCR_LEN;   /* shadow : LEN=1 */

    /* Activer LTDC (requis avant per-layer reload) */
    ltdc->GCR |= LTDC_GCR_LTDCEN;
    __DSB();

    /* Per-layer immediate reload (comme HAL_LTDC_ConfigLayer sur STM32N6).
     * Sur N6, chaque layer a un RCR avec IMR (bit 0) = trigger direct.
     * SRCR global ne fonctionne PAS si GRMSK est actif dans le RCR actif
     * (valeur reset possiblement GRMSK=1, et RCR est shadow aussi).
     * IMR transfere shadow → actif pour tous les registres de ce layer. */
    layer->RCR = LTDC_LxRCR_IMR;   /* IMR=1, GRMSK=0 */
    __DSB();
    /* IMR est auto-clear par hardware apres reload */
    while ((layer->RCR & LTDC_LxRCR_IMR) != 0U) {}  /* attendre fin reload */

    /* --- 7f) Diagnostic Secure : verifier FB + LTDC apres config --- */
    {
      /* Lire LTDC registres via S alias */
      volatile uint32_t gcr_s  = LTDC_S->GCR;
      volatile uint32_t srcr_s = LTDC_S->SRCR;
      volatile uint32_t cfbar_s = LTDC_Layer1_S->CFBAR;
      volatile uint32_t cr_s    = LTDC_Layer1_S->CR;
      volatile uint32_t whpcr_s = LTDC_Layer1_S->WHPCR;
      volatile uint32_t cfblr_s = LTDC_Layer1_S->CFBLR;

      boot_trace("[7f] GCR_S=0x");
      { char h[9]; for(int i=7;i>=0;i--){uint8_t n=(gcr_s>>(i*4))&0xF; h[7-i]=n<10?'0'+n:'A'+n-10;} h[8]=0; boot_trace(h); }
      boot_trace("\r\n");
      boot_trace("[7f] CFBAR_S=0x");
      { char h[9]; for(int i=7;i>=0;i--){uint8_t n=(cfbar_s>>(i*4))&0xF; h[7-i]=n<10?'0'+n:'A'+n-10;} h[8]=0; boot_trace(h); }
      boot_trace("\r\n");
      boot_trace("[7f] CR_S=0x");
      { char h[9]; for(int i=7;i>=0;i--){uint8_t n=(cr_s>>(i*4))&0xF; h[7-i]=n<10?'0'+n:'A'+n-10;} h[8]=0; boot_trace(h); }
      boot_trace("\r\n");
      boot_trace("[7f] WHPCR_S=0x");
      { char h[9]; for(int i=7;i>=0;i--){uint8_t n=(whpcr_s>>(i*4))&0xF; h[7-i]=n<10?'0'+n:'A'+n-10;} h[8]=0; boot_trace(h); }
      boot_trace("\r\n");
      boot_trace("[7f] CFBLR_S=0x");
      { char h[9]; for(int i=7;i>=0;i--){uint8_t n=(cfblr_s>>(i*4))&0xF; h[7-i]=n<10?'0'+n:'A'+n-10;} h[8]=0; boot_trace(h); }
      boot_trace("\r\n");

      /* Verifier FB via NS alias (0x24200000) et S alias (0x34200000) */
      volatile uint16_t fb_ns = *(volatile uint16_t *)0x24200000UL;
      volatile uint16_t fb_s  = *(volatile uint16_t *)0x34200000UL;
      boot_trace("[7f] FB@NS=0x");
      { uint32_t v=fb_ns; char h[5]; for(int i=3;i>=0;i--){uint8_t n=(v>>(i*4))&0xF; h[3-i]=n<10?'0'+n:'A'+n-10;} h[4]=0; boot_trace(h); }
      boot_trace(" FB@S=0x");
      { uint32_t v=fb_s; char h[5]; for(int i=3;i>=0;i--){uint8_t n=(v>>(i*4))&0xF; h[3-i]=n<10?'0'+n:'A'+n-10;} h[4]=0; boot_trace(h); }
      boot_trace("\r\n");
    }
  }

  /* 7f2) Diagnostic LTDC scanning + erreurs DMA.
   *      Apres 50 ms (~3 frames a 60fps), verifier :
   *      - TERRIF (bit 2) : erreur bus DMA → LTDC ne peut pas lire le FB
   *      - FUWIF  (bit 1) : underrun warning FIFO
   *      - FUIF   (bit 6) : underrun fatal FIFO
   *      - CPSR   : position scan (doit changer entre 2 lectures) */
  {
    /* Mettre BCCR vert vif pour distinguer du noir si le layer DMA echoue */
    LTDC_S->BCCR = 0x0000FF00U;  /* RRGGBB : vert */
    __DSB();

    HAL_Delay(50);

    volatile uint32_t isr1   = LTDC_S->ISR;
    volatile uint32_t cpsr1  = LTDC_S->CPSR;
    HAL_Delay(2);
    volatile uint32_t cpsr2  = LTDC_S->CPSR;

    boot_trace("[7f2] ISR=0x");
    { char h[9]; for(int i=7;i>=0;i--){uint8_t n=(isr1>>(i*4))&0xF; h[7-i]=n<10?'0'+n:'A'+n-10;} h[8]=0; boot_trace(h); }
    boot_trace("\r\n");
    boot_trace("[7f2] CPSR1=0x");
    { char h[9]; for(int i=7;i>=0;i--){uint8_t n=(cpsr1>>(i*4))&0xF; h[7-i]=n<10?'0'+n:'A'+n-10;} h[8]=0; boot_trace(h); }
    boot_trace(" CPSR2=0x");
    { char h[9]; for(int i=7;i>=0;i--){uint8_t n=(cpsr2>>(i*4))&0xF; h[7-i]=n<10?'0'+n:'A'+n-10;} h[8]=0; boot_trace(h); }
    boot_trace("\r\n");

    if (isr1 & (1UL << 2))  boot_trace("[7f2] *** TERRIF: DMA bus error! ***\r\n");
    if (isr1 & (1UL << 1))  boot_trace("[7f2] *** FUWIF: FIFO underrun warn ***\r\n");
    if (isr1 & (1UL << 6))  boot_trace("[7f2] *** FUIF: FIFO underrun fatal ***\r\n");
    if (cpsr1 == cpsr2)      boot_trace("[7f2] *** CPSR stuck — not scanning ***\r\n");
    else                     boot_trace("[7f2] LTDC scanning OK\r\n");

    /* 7f3) Diagnostic GPIO : verifier que les pins LCD sont en AF14
     *      et que le backlight / power sont effectivement HIGH.
     *      PB13 (LCD_CLK) = le plus critique — si pas AF14, pas de pixel clock */
    {
      /* GPIOB : PB13 = LCD_CLK
       *   MODER[27:26] doit etre 10 (AF)
       *   AFR[1] bits [23:20] doivent etre 0xE (=14=AF14) */
      volatile uint32_t pb_moder = GPIOB_S->MODER;
      volatile uint32_t pb_afrh  = GPIOB_S->AFR[1]; /* AFRH pour pin 8-15 */
      /* PB14 = LCD_HSYNC : MODER[29:28]=10, AFR[1][27:24]=0xE */
      boot_trace("[7f3] PB MODER=0x");
      { char h[9]; for(int i=7;i>=0;i--){uint8_t n=(pb_moder>>(i*4))&0xF; h[7-i]=n<10?'0'+n:'A'+n-10;} h[8]=0; boot_trace(h); }
      boot_trace(" AFRH=0x");
      { char h[9]; for(int i=7;i>=0;i--){uint8_t n=(pb_afrh>>(i*4))&0xF; h[7-i]=n<10?'0'+n:'A'+n-10;} h[8]=0; boot_trace(h); }
      boot_trace("\r\n");

      uint32_t pb13_mode = (pb_moder >> 26) & 0x3;
      uint32_t pb13_af   = (pb_afrh >> 20)  & 0xF;
      if (pb13_mode != 2 || pb13_af != 14)
        boot_trace("[7f3] *** PB13(CLK) NOT AF14! ***\r\n");
      else
        boot_trace("[7f3] PB13(CLK) = AF14 OK\r\n");

      /* PE11 = LCD_VSYNC : MODER[23:22]=10, AFR[1][15:12]=0xE */
      volatile uint32_t pe_moder = GPIOE_S->MODER;
      volatile uint32_t pe_afrh  = GPIOE_S->AFR[1];
      uint32_t pe11_mode = (pe_moder >> 22) & 0x3;
      uint32_t pe11_af   = (pe_afrh >> 12) & 0xF;
      if (pe11_mode != 2 || pe11_af != 14)
        boot_trace("[7f3] *** PE11(VSYNC) NOT AF14! ***\r\n");
      else
        boot_trace("[7f3] PE11(VSYNC) = AF14 OK\r\n");

      /* PG13 = LCD_DE : MODER[27:26]=01 (OUTPUT) voulue, ODR bit13=1 */
      volatile uint32_t pg_moder = GPIOG_S->MODER;
      volatile uint32_t pg_odr   = GPIOG_S->ODR;
      uint32_t pg13_mode = (pg_moder >> 26) & 0x3;
      if (pg13_mode != 1)
        boot_trace("[7f3] *** PG13(DE) NOT OUTPUT! ***\r\n");
      else if (!(pg_odr & (1U << 13)))
        boot_trace("[7f3] *** PG13(DE) OUTPUT but LOW! ***\r\n");
      else
        boot_trace("[7f3] PG13(DE) = OUT HIGH OK\r\n");

      /* PQ3 = LCD_ON, PQ6 = LCD_BL : ODR */
      volatile uint32_t pq_moder = GPIOQ_S->MODER;
      volatile uint32_t pq_odr   = GPIOQ_S->ODR;
      boot_trace("[7f3] PQ MODER=0x");
      { char h[9]; for(int i=7;i>=0;i--){uint8_t n=(pq_moder>>(i*4))&0xF; h[7-i]=n<10?'0'+n:'A'+n-10;} h[8]=0; boot_trace(h); }
      boot_trace(" ODR=0x");
      { char h[9]; for(int i=3;i>=0;i--){uint8_t n=(pq_odr>>(i*4))&0xF; h[3-i]=n<10?'0'+n:'A'+n-10;} h[4]=0; boot_trace(h); }
      boot_trace("\r\n");
      if (!(pq_odr & (1U << 3)))  boot_trace("[7f3] *** PQ3(LCD_ON) LOW! ***\r\n");
      if (!(pq_odr & (1U << 6)))  boot_trace("[7f3] *** PQ6(LCD_BL) LOW! ***\r\n");
      if ((pq_odr & ((1U<<3)|(1U<<6))) == ((1U<<3)|(1U<<6)))
        boot_trace("[7f3] PQ3+PQ6 HIGH OK\r\n");

      /* RIMC: verifier attributs LTDC1 master */
      volatile uint32_t rimc1 = RIFSC_S->RIMC_ATTRx[RIF_MASTER_INDEX_LTDC1];
      boot_trace("[7f3] RIMC_LTDC1=0x");
      { char h[9]; for(int i=7;i>=0;i--){uint8_t n=(rimc1>>(i*4))&0xF; h[7-i]=n<10?'0'+n:'A'+n-10;} h[8]=0; boot_trace(h); }
      boot_trace("\r\n");
    }

    /* Attendre 2s pour observer l'ecran avant le saut NS */
    boot_trace("[7f2] Waiting 2s — observe screen...\r\n");
    HAL_Delay(2000);
  }

  /* 7g) LTDC reste SEC : le DMA LTDC fonctionne en SEC+PRIV pour acceder
   *     SRAM3 via l'alias Secure 0x34200000. Si on bascule en NSEC, le DMA
   *     ne pourrait plus lire le framebuffer (acces NS a SRAM3 bloque).
   *     AppliNonSecure accede LTDC via NSC veneers si necessaire. */
  boot_trace("[7g] LTDC stays SEC\r\n");

  boot_trace("[RIF2] 7-LTDC OK\r\n");

  /* USER CODE END RIF_Init 2 */

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
  /* NON-BLOQUANT : __BKPT(0) sans debugger = HardFault.
   * On trace et on continue pour ne pas bloquer la chaine de boot. */
  boot_trace("[AppliSecure] ERROR_HANDLER!\r\n");
  __NOP();
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
