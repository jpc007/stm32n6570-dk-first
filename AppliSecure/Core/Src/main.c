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

  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* USER CODE BEGIN SysInit */

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

  /* Set non-secure main stack (MSP_NS) */
  __TZ_set_MSP_NS((*(uint32_t *)VTOR_TABLE_NS_START_ADDR));

  /* Get non-secure reset handler */
  NonSecure_ResetHandler = (funcptr_NS)(*((uint32_t *)((VTOR_TABLE_NS_START_ADDR) + 4U)));

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

  /* USER CODE END RIF_Init 1 */
  /* USER CODE BEGIN RIF_Init 2 */

  /* ---------------------------------------------------------------
   * Configurations NON generees par CubeMX — doivent rester ici.
   * Survivent a la regeneration car dans USER CODE.
   * --------------------------------------------------------------- */

  /* 1) RISUP NSEC pour peripheriques AppliNonSecure
   *    (CubeMX ne genere que des RISUP SEC pour le FSBL) */
  HAL_RIF_RISC_SetSlaveSecureAttributes(RIF_RISC_PERIPH_INDEX_USART2,
                                        RIF_ATTRIBUTE_NSEC | RIF_ATTRIBUTE_NPRIV);
  HAL_RIF_RISC_SetSlaveSecureAttributes(RIF_RISC_PERIPH_INDEX_LTDC,
                                        RIF_ATTRIBUTE_NSEC | RIF_ATTRIBUTE_NPRIV);
  HAL_RIF_RISC_SetSlaveSecureAttributes(RIF_RISC_PERIPH_INDEX_LTDCL1,
                                        RIF_ATTRIBUTE_NSEC | RIF_ATTRIBUTE_NPRIV);
  HAL_RIF_RISC_SetSlaveSecureAttributes(RIF_RISC_PERIPH_INDEX_LTDCL2,
                                        RIF_ATTRIBUTE_NSEC | RIF_ATTRIBUTE_NPRIV);

  /* 2) RIMC master : LTDC L1 + L2 DMA avec CID=1 (NS)
   *    (CubeMX ne genere pas de RIMC pour LTDC) */
  {
    RIMC_MasterConfig_t ltdc_master = {0};
    ltdc_master.MasterCID = RIF_CID_1;
    ltdc_master.SecPriv   = RIF_ATTRIBUTE_NSEC | RIF_ATTRIBUTE_NPRIV;
    HAL_RIF_RIMC_ConfigMasterAttributes(RIF_MASTER_INDEX_LTDC1, &ltdc_master);
    HAL_RIF_RIMC_ConfigMasterAttributes(RIF_MASTER_INDEX_LTDC2, &ltdc_master);
    /* Bit 31 : maitre vu comme « DMA » par le RIF — requis pour que RISAF autorise
     * les lectures LTDC vers la SRAM ; HAL_RIF_RIMC_ConfigMasterAttributes ne le pose pas. */
    RIFSC->RIMC_ATTRx[RIF_MASTER_INDEX_LTDC1] |= (1UL << 31);
    RIFSC->RIMC_ATTRx[RIF_MASTER_INDEX_LTDC2] |= (1UL << 31);
    __DSB();
  }

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

  /* 5) RISAF4 — ouvrir SRAM3+SRAM4 en NS pour le framebuffer LTDC
   *    RISAF4 protege NPU port 0 = SRAM3(0x24200000)+SRAM4(0x24270000)
   *    Espace d'adresse = 4 GB → adresses absolues dans STARTR/ENDR
   *    On utilise Region 1 (index 0 dans le tableau REG[]) */

  /* 5a) Activer RISAF (AHB3) + RAMCFG (AHB2) horloges */
  __HAL_RCC_RISAF_CLK_ENABLE();
  __HAL_RCC_RAMCFG_CLK_ENABLE();
  __DSB();

  /* 5b) Activer SRAM3/SRAM4 horloges memoire.
   *     Utilisation de CMSIS direct dans les deux alias (S et NS) car le
   *     mecanisme exact de protection RCC varie selon le state TZ. */
  RCC_S->MEMENSR = RCC_MEMENR_AXISRAM3EN | RCC_MEMENR_AXISRAM4EN;
  __DSB();
  /* Fallback : forcer aussi via ENR direct */
  if (!(RCC_S->MEMENR & (RCC_MEMENR_AXISRAM3EN | RCC_MEMENR_AXISRAM4EN)))
  {
    RCC_S->MEMENR |= (RCC_MEMENR_AXISRAM3EN | RCC_MEMENR_AXISRAM4EN);
    __DSB();
  }
  /* Rendre les ressources RCC publiques (NS accessible) via PUBCFGR */
  SET_BIT(RCC_S->PUBCFGR5, RCC_PUBCFGR5_AXISRAM3PUB | RCC_PUBCFGR5_AXISRAM4PUB);
  SET_BIT(RCC_S->PUBCFGR2, RCC_PUBCFGR2_IC16PUB);    /* IC16 divider config depuis NS */
  SET_BIT(RCC_S->PUBCFGR3, RCC_PUBCFGR3_PERPUB);     /* CCIPR peripherals clock mux depuis NS */
  SET_BIT(RCC_S->PUBCFGR4, RCC_PUBCFGR4_APB5PUB);    /* APB5 bus clock depuis NS */
  __DSB();

  /* 5c) Sortir SRAM3/SRAM4 du mode shutdown via RAMCFG */
  CLEAR_BIT(RAMCFG_SRAM3_AXI_S->CR, RAMCFG_CR_SRAMSD);
  CLEAR_BIT(RAMCFG_SRAM4_AXI_S->CR, RAMCFG_CR_SRAMSD);
  __DSB();

  /* 5d) Configurer RISAF4 region 1 (index 0) — GRAND OUVERT
   *     Le LTDC DMA (CID1, NSEC, NPRIV via RIMC) doit pouvoir lire le
   *     framebuffer dans SRAM3/SRAM4.  L'ancienne config avait PRIVC1
   *     (privilege requis pour CID1) alors que LTDC est NPRIV ⇒ acces
   *     refuse silencieusement ⇒ ecran noir.
   *     Fix : pas de restriction SEC/PRIV, tous CIDs autorises. */
  {
    RISAF_TypeDef *risaf4 = RISAF4_S;

    /* Desactiver la region d'abord */
    risaf4->REG[0].CFGR = 0;
    __DSB();

    /* Effacer tout illegal-access pending de tentatives precedentes */
    risaf4->IACR = risaf4->IASR;
    __DSB();

    /* Start = 0x24200000, End = 0x242DFFFF (SRAM3+SRAM4, 896 KB)
     * Aligne sur 4 KB (granularite RISAF) */
    risaf4->REG[0].STARTR = 0x24200000;
    risaf4->REG[0].ENDR   = 0x242DFFFF;

    /* CID whitelist : TOUS les CIDs (0-7) pour read ET write.
     *   RDENC0-7 = bits [0..7]   = 0xFF
     *   WRENC0-7 = bits [16..23] = 0xFF << 16 = 0x00FF0000 */
    risaf4->REG[0].CIDCFGR = 0x00FF00FFU;

    /* Activer la region, NSEC, aucun privilege requis (PRIVC = 0) */
    risaf4->REG[0].CFGR = RISAF_REGx_CFGR_BREN;  /* enable, NSEC, NPRIV */
    __DSB();
  }

  /* 5e) RISAF2 / SRAM1 : ne pas activer BREN (voir FSBL) — sinon IASR et LTDC bloque. */
  {
    RISAF_TypeDef *risaf2 = RISAF2_S;

    risaf2->REG[0].CFGR = 0;
    __DSB();
    risaf2->IACR = risaf2->IASR;
    __DSB();
  }

  /* 5f) MPU non-secure : framebuffer LTDC (SRAM3 0x24200000, 800x480x2) en non-cacheable.
   *     Le CM55 ecrit souvent dans le D-cache ; le LTDC lit l'AXI/SRAM hors cache.
   *     Sans cette region, seul BCCR (registre) est correct — layer FB = noir. */
#if defined(HAL_CORTEX_MODULE_ENABLED) && defined(MPU_NS)
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
#endif

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
    layer->CFBAR  = 0x24200000U;  /* framebuffer SRAM3 (adresse NS = identique pour DMA LTDC) */
    layer->CFBLR  = ((w * 2U) << 16) | ((w * 2U) + 7U);
    layer->CFBLNR = h;

    /* --- 7d) Remplir le framebuffer avec du BLANC (RGB565 = 0xFFFF) --- */
    {
      volatile uint16_t *fb = (volatile uint16_t *)0x24200000UL;
      for (uint32_t i = 0; i < w * h; i++) {
        fb[i] = 0xFFFFU;
      }
      __DSB();
    }

    /* --- 7e) Activer layer 1 + LTDC + rechargement immediat --- */
    layer->CR = LTDC_LxCR_LEN;
    ltdc->GCR |= LTDC_GCR_LTDCEN;

    layer->RCR = 0U;              /* PAS de GRMSK ! */
    __DSB();
    ltdc->SRCR = LTDC_SRCR_IMR;   /* rechargement immediat */
    __DSB();
    while ((ltdc->SRCR & LTDC_SRCR_IMR) != 0U) {}  /* attendre fin reload */
  }

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
