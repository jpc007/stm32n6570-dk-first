/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    stm32n6xx_it.c
  * @brief   Interrupt Service Routines.
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
#include "stm32n6xx_it.h"
/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "ns_diag_config.h"
#if !NS_DIAG_NO_LVGL
#include "lvgl_port.h"
#endif

/* Fault diagnostic : UART print + blink pattern on LEDs (CMSIS direct) */
static void fault_uart_print(const char *s)
{
  /* Minimal UART print from fault context — USART2 should already be init */
  while (*s) {
    while (!(USART2_NS->ISR & USART_ISR_TXE_TXFNF)) {}
    USART2_NS->TDR = (uint8_t)*s++;
  }
}

static void fault_blink(uint32_t pattern)
{
  /* Enable GPIOD/E/H clocks */
  RCC->AHB4ENR |= RCC_AHB4ENR_GPIODEN | RCC_AHB4ENR_GPIOEEN | RCC_AHB4ENR_GPIOHEN;
  __DSB();
  /* PD0 output (GREEN) */
  GPIOD_NS->MODER = (GPIOD_NS->MODER & ~(3UL<<0))  | (1UL<<0);
  /* PE9 output (YELLOW) */
  GPIOE_NS->MODER = (GPIOE_NS->MODER & ~(3UL<<18)) | (1UL<<18);
  /* PH5 output (RED) */
  GPIOH_NS->MODER = (GPIOH_NS->MODER & ~(3UL<<10)) | (1UL<<10);
  /* PE10 output (BLUE) */
  GPIOE_NS->MODER = (GPIOE_NS->MODER & ~(3UL<<20)) | (1UL<<20);
  /* PE13 output (WHITE) */
  GPIOE_NS->MODER = (GPIOE_NS->MODER & ~(3UL<<26)) | (1UL<<26);

  /* Turn all OFF then set pattern:
   * pattern bits: 0=GREEN, 1=YELLOW, 2=RED, 3=BLUE, 4=WHITE */
  GPIOD_NS->BSRR = (pattern & 1) ? (1UL<<0) : (1UL<<16);
  GPIOE_NS->BSRR = (pattern & 2) ? (1UL<<9) : (1UL<<25);
  GPIOH_NS->BSRR = (pattern & 4) ? (1UL<<5) : (1UL<<21);
  GPIOE_NS->BSRR = (pattern & 8) ? (1UL<<10) : (1UL<<26);
  GPIOE_NS->BSRR = (pattern & 16) ? (1UL<<13) : (1UL<<29);

  /* Blink forever */
  while (1) {
    for (volatile uint32_t d = 0; d < 2000000; d++) {}
    GPIOH_NS->ODR ^= (1UL<<5); /* toggle RED */
  }
}
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN TD */

/* USER CODE END TD */

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
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

/* USER CODE END 0 */

/* External variables --------------------------------------------------------*/

/* USER CODE BEGIN EV */

/* USER CODE END EV */

/******************************************************************************/
/*           Cortex Processor Interruption and Exception Handlers          */
/******************************************************************************/
/**
  * @brief This function handles Memory management fault.
  */
void MemManage_Handler(void)
{
  /* USER CODE BEGIN MemoryManagement_IRQn 0 */
  fault_uart_print("\r\n*** MEMMANAGE ***\r\n");
  fault_blink(0x04);  /* RED only = MemManage */
  /* USER CODE END MemoryManagement_IRQn 0 */
  while (1)
  {
    /* USER CODE BEGIN W1_MemoryManagement_IRQn 0 */
    /* USER CODE END W1_MemoryManagement_IRQn 0 */
  }
}

/**
  * @brief This function handles Undefined instruction or illegal state.
  */
void UsageFault_Handler(void)
{
  /* USER CODE BEGIN UsageFault_IRQn 0 */
  fault_uart_print("\r\n*** USAGEFAULT ***\r\n");
  fault_blink(0x06);  /* YELLOW+RED = UsageFault */
  /* USER CODE END UsageFault_IRQn 0 */
  while (1)
  {
    /* USER CODE BEGIN W1_UsageFault_IRQn 0 */
    /* USER CODE END W1_UsageFault_IRQn 0 */
  }
}

/**
  * @brief This function handles Secure fault.
  */
void SecureFault_Handler(void)
{
  /* USER CODE BEGIN SecureFault_IRQn 0 */
  fault_uart_print("\r\n*** SECUREFAULT ***\r\n");
  fault_blink(0x08);  /* BLUE only = SecureFault */
  /* USER CODE END SecureFault_IRQn 0 */
  while (1)
  {
    /* USER CODE BEGIN W1_SecureFault_IRQn 0 */
    /* USER CODE END W1_SecureFault_IRQn 0 */
  }
}

/**
  * @brief This function handles System service call via SWI instruction.
  */
void SVC_Handler(void)
{
  /* USER CODE BEGIN SVCall_IRQn 0 */

  /* USER CODE END SVCall_IRQn 0 */
  /* USER CODE BEGIN SVCall_IRQn 1 */

  /* USER CODE END SVCall_IRQn 1 */
}

/**
  * @brief This function handles Pendable request for system service.
  */
void PendSV_Handler(void)
{
  /* USER CODE BEGIN PendSV_IRQn 0 */

  /* USER CODE END PendSV_IRQn 0 */
  /* USER CODE BEGIN PendSV_IRQn 1 */

  /* USER CODE END PendSV_IRQn 1 */
}

/**
  * @brief This function handles System tick timer.
  */
void SysTick_Handler(void)
{
  /* USER CODE BEGIN SysTick_IRQn 0 */

  /* USER CODE END SysTick_IRQn 0 */
  HAL_IncTick();
  /* USER CODE BEGIN SysTick_IRQn 1 */
#if !NS_DIAG_NO_LVGL
  lv_port_tick_inc(1);
#endif
  /* USER CODE END SysTick_IRQn 1 */
}

/******************************************************************************/
/* STM32N6xx Peripheral Interrupt Handlers                                    */
/* Add here the Interrupt Handlers for the used peripherals.                  */
/* For the available peripheral interrupt handler names,                      */
/* please refer to the startup file (startup_stm32n6xx.s).                    */
/******************************************************************************/

/* USER CODE BEGIN 1 */

/* USER CODE END 1 */
