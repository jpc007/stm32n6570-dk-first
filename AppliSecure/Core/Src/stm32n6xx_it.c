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

/* --- Minimal USART2 trace for fault handlers (no HAL dependency) --- */
static void fault_putc(char c)
{
  while (!(USART2->ISR & USART_ISR_TXE_TXFNF)) {}
  USART2->TDR = (uint8_t)c;
}
static void fault_puts(const char *s)
{
  while (*s) fault_putc(*s++);
}
static void fault_hex32(uint32_t v)
{
  fault_puts("0x");
  for (int i = 28; i >= 0; i -= 4) {
    uint8_t n = (v >> i) & 0xFU;
    fault_putc(n < 10 ? '0' + n : 'A' + n - 10);
  }
}

/* USER CODE END 0 */

/* External variables --------------------------------------------------------*/

/* USER CODE BEGIN EV */

/* USER CODE END EV */

/******************************************************************************/
/*           Cortex Processor Interruption and Exception Handlers          */
/******************************************************************************/
/**
  * @brief This function handles Non maskable interrupt.
  */
void NMI_Handler(void)
{
  /* USER CODE BEGIN NonMaskableInt_IRQn 0 */

  /* USER CODE END NonMaskableInt_IRQn 0 */
  /* USER CODE BEGIN NonMaskableInt_IRQn 1 */
   while (1)
  {
  }
  /* USER CODE END NonMaskableInt_IRQn 1 */
}

/**
  * @brief This function handles Hard fault interrupt.
  */
void HardFault_Handler(void)
{
  /* USER CODE BEGIN HardFault_IRQn 0 */
  fault_puts("\r\n!!! SECURE HardFault !!!\r\n");
  fault_puts("CFSR="); fault_hex32(SCB->CFSR); fault_puts("\r\n");
  fault_puts("HFSR="); fault_hex32(SCB->HFSR); fault_puts("\r\n");
  fault_puts("BFAR="); fault_hex32(SCB->BFAR); fault_puts("\r\n");
  fault_puts("MMFAR="); fault_hex32(SCB->MMFAR); fault_puts("\r\n");
  fault_puts("LR="); fault_hex32((uint32_t)__builtin_return_address(0)); fault_puts("\r\n");
  /* SFSR for SecureFault status */
  fault_puts("SFSR="); fault_hex32(SAU->SFSR); fault_puts("\r\n");
  fault_puts("SFAR="); fault_hex32(SAU->SFAR); fault_puts("\r\n");
  /* Dump NS exception frame (LR bit6=0 → fault from NS, bit2=0 → MSP) */
  {
    uint32_t ns_msp = __TZ_get_MSP_NS();
    fault_puts("MSP_NS="); fault_hex32(ns_msp); fault_puts("\r\n");
    if (ns_msp >= 0x24100000UL && ns_msp <= 0x24300000UL) {
      /* Read frame via NS alias */
      volatile uint32_t *f = (volatile uint32_t *)ns_msp;
      fault_puts("NS: PC ="); fault_hex32(f[6]); fault_puts(" LR="); fault_hex32(f[5]);
      fault_puts(" R0="); fault_hex32(f[0]); fault_puts(" PSR="); fault_hex32(f[7]);
      fault_puts("\r\n");
      /* Read SAME frame via SECURE alias (0x34xxxxxx) to check if aliasing is the issue */
      uint32_t s_msp = ns_msp | 0x10000000UL;  /* 0x241xxxxx → 0x341xxxxx */
      volatile uint32_t *sf = (volatile uint32_t *)s_msp;
      fault_puts("S:  PC ="); fault_hex32(sf[6]); fault_puts(" LR="); fault_hex32(sf[5]);
      fault_puts(" R0="); fault_hex32(sf[0]); fault_puts(" PSR="); fault_hex32(sf[7]);
      fault_puts("\r\n");
    }
    /* Verify NS code at CORRECT Reset_Handler addr (0x341015E8 = Secure alias) */
    volatile uint32_t *code = (volatile uint32_t *)0x341015E8UL;
    fault_puts("CODE@RST=");
    fault_hex32(code[0]); fault_puts(" ");
    fault_hex32(code[1]); fault_puts(" ");
    fault_hex32(code[2]); fault_puts(" ");
    fault_hex32(code[3]); fault_puts("\r\n");
    /* Also read SystemInit entry (0x3410157C from objdump) */
    volatile uint32_t *si = (volatile uint32_t *)0x3410157CUL;
    fault_puts("CODE@SI =");
    fault_hex32(si[0]); fault_puts(" ");
    fault_hex32(si[1]); fault_puts("\r\n");
    /* Read NS CONTROL register */
    fault_puts("CTRL_NS="); fault_hex32(__TZ_get_CONTROL_NS()); fault_puts("\r\n");
  }
  /* USER CODE END HardFault_IRQn 0 */
  while (1)
  {
    /* USER CODE BEGIN W1_HardFault_IRQn 0 */
    /* USER CODE END W1_HardFault_IRQn 0 */
  }
}

/**
  * @brief This function handles Memory management fault.
  */
void MemManage_Handler(void)
{
  /* USER CODE BEGIN MemoryManagement_IRQn 0 */
  fault_puts("\r\n!!! SECURE MemManage !!!\r\n");
  fault_puts("CFSR="); fault_hex32(SCB->CFSR); fault_puts("\r\n");
  fault_puts("MMFAR="); fault_hex32(SCB->MMFAR); fault_puts("\r\n");
  /* USER CODE END MemoryManagement_IRQn 0 */
  while (1)
  {
    /* USER CODE BEGIN W1_MemoryManagement_IRQn 0 */
    /* USER CODE END W1_MemoryManagement_IRQn 0 */
  }
}

/**
  * @brief This function handles Prefetch fault, memory access fault.
  */
void BusFault_Handler(void)
{
  /* USER CODE BEGIN BusFault_IRQn 0 */
  fault_puts("\r\n!!! SECURE BusFault !!!\r\n");
  fault_puts("CFSR="); fault_hex32(SCB->CFSR); fault_puts("\r\n");
  fault_puts("BFAR="); fault_hex32(SCB->BFAR); fault_puts("\r\n");
  /* USER CODE END BusFault_IRQn 0 */
  while (1)
  {
    /* USER CODE BEGIN W1_BusFault_IRQn 0 */
    /* USER CODE END W1_BusFault_IRQn 0 */
  }
}

/**
  * @brief This function handles Undefined instruction or illegal state.
  */
void UsageFault_Handler(void)
{
  /* USER CODE BEGIN UsageFault_IRQn 0 */
  fault_puts("\r\n!!! SECURE UsageFault !!!\r\n");
  fault_puts("CFSR="); fault_hex32(SCB->CFSR); fault_puts("\r\n");
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
  fault_puts("\r\n!!! SECURE SecureFault !!!\r\n");
  fault_puts("SFSR="); fault_hex32(SAU->SFSR); fault_puts("\r\n");
  fault_puts("SFAR="); fault_hex32(SAU->SFAR); fault_puts("\r\n");
  fault_puts("CFSR="); fault_hex32(SCB->CFSR); fault_puts("\r\n");
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
  * @brief This function handles Debug monitor.
  */
void DebugMon_Handler(void)
{
  /* USER CODE BEGIN DebugMonitor_IRQn 0 */

  /* USER CODE END DebugMonitor_IRQn 0 */
  /* USER CODE BEGIN DebugMonitor_IRQn 1 */

  /* USER CODE END DebugMonitor_IRQn 1 */
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
