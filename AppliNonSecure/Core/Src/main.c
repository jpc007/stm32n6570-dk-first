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
#include "framebuffer_ns.h"
#include "ltdc_visual_test.h"
#include "lv_port_disp_ltdc.h"
#if !APP_LTDC_VISUAL_TEST
#include "lvgl_port.h"
#include "lvgl.h"
#endif
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

/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */
  /* ==== EARLY DIAG : allumer LED VERTE via registres CMSIS bruts ==== */
  /* PD0 = LED VERTE, avant tout HAL_Init */
  RCC->AHB4ENR |= RCC_AHB4ENR_GPIODEN;  /* horloge GPIOD */
  __DSB();
  GPIOD_NS->MODER = (GPIOD_NS->MODER & ~(3UL << (0*2))) | (1UL << (0*2)); /* PD0 output */
  GPIOD_NS->BSRR = (1UL << 0);  /* PD0 HIGH = LED VERTE ON => "j'ai atteint main" */
  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/
  HAL_Init();

  /* USER CODE BEGIN Init */
#if APP_LTDC_VISUAL_TEST
#if defined(__DCACHE_PRESENT) && (__DCACHE_PRESENT == 1U)
  /* Diag LTDC : sans D-cache pour eliminer toute incoherence cache/AXI sur SRAM3. */
  SCB_DisableDCache();
#endif
#endif
  /* LED JAUNE = HAL_Init() passe */
  RCC->AHB4ENR |= RCC_AHB4ENR_GPIOEEN;
  __DSB();
  GPIOE_NS->MODER = (GPIOE_NS->MODER & ~(3UL << (9*2))) | (1UL << (9*2));
  GPIOE_NS->BSRR = (1UL << 9);  /* PE9 = JAUNE ON */
  /* USER CODE END Init */

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_USART2_UART_Init();
  /* USER CODE BEGIN 2 */
  /* --- Debug board init (5 LEDs + USART2) --- */
  DBG_Board_Init();
  DBG_LED_ON(DBG_LED_GREEN_PORT, DBG_LED_GREEN_PIN);  /* VERTE = NS atteint */
  DBG_Print("\r\n\r\n=== STM32N657 NS BOOT ===\r\n");
  DBG_Print("[1] HAL+GPIO+UART OK\r\n");

  /* --- LCD power ON + backlight ------------------------------------ */
  DBG_Print("[2] LCD power...\r\n");
  __HAL_RCC_GPIOQ_CLK_ENABLE();

  GPIO_InitTypeDef GPIO_InitStruct = {0};
  GPIO_InitStruct.Pin   = GPIO_PIN_3;
  GPIO_InitStruct.Mode  = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull  = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOQ, &GPIO_InitStruct);
  HAL_GPIO_WritePin(GPIOQ, GPIO_PIN_3, GPIO_PIN_SET);   /* LCD_ON */

  GPIO_InitStruct.Pin   = GPIO_PIN_6;
  HAL_GPIO_Init(GPIOQ, &GPIO_InitStruct);
  HAL_GPIO_WritePin(GPIOQ, GPIO_PIN_6, GPIO_PIN_SET);   /* backlight */

  /* LCD_NRST (PE1) : reset cycle du panneau LCD (LOW → delay → HIGH) */
  GPIO_InitStruct.Pin   = LCD_NRST_Pin;
  GPIO_InitStruct.Mode  = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull  = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(LCD_NRST_GPIO_Port, &GPIO_InitStruct);
  HAL_GPIO_WritePin(LCD_NRST_GPIO_Port, LCD_NRST_Pin, GPIO_PIN_RESET);
  HAL_Delay(2);   /* assert reset ~2 ms */
  HAL_GPIO_WritePin(LCD_NRST_GPIO_Port, LCD_NRST_Pin, GPIO_PIN_SET);
  HAL_Delay(120); /* panel startup time */
  DBG_Print("[3] LCD power OK\r\n");

  /* --- LTDC GPIO : pins manquants dans MX_GPIO_Init + speed fix ---- */
  /* LCD_CLK (PB13) — CRITIQUE : pixel clock, absent du .ioc ! */
  __HAL_RCC_GPIOB_CLK_ENABLE();
  GPIO_InitStruct.Pin       = LCD_CLK_Pin;
  GPIO_InitStruct.Mode      = GPIO_MODE_AF_PP;
  GPIO_InitStruct.Pull      = GPIO_NOPULL;
  GPIO_InitStruct.Speed     = GPIO_SPEED_FREQ_HIGH;
  GPIO_InitStruct.Alternate = GPIO_AF14_LCD;
  HAL_GPIO_Init(LCD_CLK_GPIO_Port, &GPIO_InitStruct);

  /* LCD_R0 (PG0) — absent du .ioc */
  __HAL_RCC_GPIOG_CLK_ENABLE();
  GPIO_InitStruct.Pin       = LCD_R0_Pin;
  HAL_GPIO_Init(LCD_R0_GPIO_Port, &GPIO_InitStruct);

  /* LCD_DE (PG13) : sur STM32N6570-DK c'est un signal "Display Enable"
   * statique (GPIO output HIGH) et NON le signal DE genere par le LTDC.
   * Le BSP ST officiel le configure en GPIO_OUTPUT_PP set HIGH.
   * Configurer en AF14 provoque un ecran noir car le LTDC met DE a LOW
   * pendant les periodes de blanking, ce que le panneau interprete comme
   * "display OFF". */
  GPIO_InitStruct.Pin       = LCD_DE_Pin;
  GPIO_InitStruct.Mode      = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull      = GPIO_NOPULL;
  GPIO_InitStruct.Speed     = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(LCD_DE_GPIO_Port, &GPIO_InitStruct);
  HAL_GPIO_WritePin(LCD_DE_GPIO_Port, LCD_DE_Pin, GPIO_PIN_SET);
  DBG_Print("[4] LTDC GPIO OK\r\n");

  /* ---- GPIO READBACK : verifier que TrustZone n'a pas bloque ---- */
  DBG_Print("[4b] GPIO readback:\r\n");
  {
    /* PB13 (LCD_CLK) doit etre AF14 (MODER=2, AFR=14) */
    uint32_t moder_b = GPIOB_NS->MODER;
    uint32_t afr1_b  = GPIOB_NS->AFR[1];
    uint32_t pb13_mode = (moder_b >> (13*2)) & 0x3;
    uint32_t pb13_af   = (afr1_b >> ((13-8)*4)) & 0xF;
    DBG_Print("  PB13(CLK) mode="); DBG_PrintDec(pb13_mode);
    DBG_Print(" AF="); DBG_PrintDec(pb13_af);
    if (pb13_mode == 2 && pb13_af == 14)
      DBG_Print(" -> AF14 OK\r\n");
    else
      DBG_Print(" -> !!! PAS AF14 - Recompiler AppliSecure! !!!\r\n");

    /* PG13 (LCD_DE) doit etre OUTPUT (mode=1), HIGH */
    uint32_t moder_g = GPIOG_NS->MODER;
    uint32_t odr_g   = GPIOG_NS->ODR;
    uint32_t pg13_mode = (moder_g >> (13*2)) & 0x3;
    uint32_t pg13_val  = (odr_g >> 13) & 0x1;
    DBG_Print("  PG13(DE) mode="); DBG_PrintDec(pg13_mode);
    DBG_Print(" val="); DBG_PrintDec(pg13_val);
    if (pg13_mode == 1 && pg13_val == 1)
      DBG_Print(" -> OUTPUT HIGH OK\r\n");
    else
      DBG_Print(" -> !!! LCD_DE pas correct !!!\r\n");

    /* PE1 (LCD_NRST) doit etre OUTPUT HIGH apres reset cycle */
    uint32_t moder_e = GPIOE_NS->MODER;
    uint32_t odr_e   = GPIOE_NS->ODR;
    uint32_t pe1_mode = (moder_e >> (1*2)) & 0x3;
    uint32_t pe1_val  = (odr_e >> 1) & 0x1;
    DBG_Print("  PE1(NRST) mode="); DBG_PrintDec(pe1_mode);
    DBG_Print(" val="); DBG_PrintDec(pe1_val);
    if (pe1_mode == 1 && pe1_val == 1)
      DBG_Print(" -> OUTPUT HIGH OK\r\n");
    else
      DBG_Print(" -> !!! NRST pas correct !!!\r\n");

    /* PQ3 (LCD_ON) et PQ6 (backlight) */
    uint32_t moder_q = GPIOQ->MODER;
    uint32_t odr_q   = GPIOQ->ODR;
    DBG_Print("  PQ3(ON) mode="); DBG_PrintDec((moder_q >> (3*2)) & 0x3);
    DBG_Print(" val="); DBG_PrintDec((odr_q >> 3) & 0x1); DBG_Print("\r\n");
    DBG_Print("  PQ6(BL) mode="); DBG_PrintDec((moder_q >> (6*2)) & 0x3);
    DBG_Print(" val="); DBG_PrintDec((odr_q >> 6) & 0x1); DBG_Print("\r\n");
  }

  /* =================================================================
   *  DIAGNOSTIC : test SRAM3 + LTDC avant LVGL
   * ================================================================= */
  DBG_Print("[5] DIAG START\r\n");

  /* Diagnostic : verifier que les horloges SRAM3/RAMCFG/RISAF sont actives
   * On lit les registres de status (MEMENR, AHB2ENR, AHB3ENR) qui refletent
   * l'etat reel des horloges. */
  {
    uint32_t memenr  = RCC->MEMENR;
    uint32_t ahb2enr = RCC->AHB2ENR;
    uint32_t ahb3enr = RCC->AHB3ENR;
    uint32_t apb5enr = RCC->APB5ENR;
    char h[12];

    DBG_Print("  MEMENR  = 0x");
    for (int i = 7; i >= 0; i--) { int n = (memenr >> (i*4)) & 0xF; h[7-i] = n < 10 ? '0'+n : 'A'+n-10; } h[8] = '\0';
    DBG_Print(h); DBG_Print("\r\n");

    DBG_Print("  AHB2ENR = 0x");
    for (int i = 7; i >= 0; i--) { int n = (ahb2enr >> (i*4)) & 0xF; h[7-i] = n < 10 ? '0'+n : 'A'+n-10; } h[8] = '\0';
    DBG_Print(h); DBG_Print("\r\n");

    DBG_Print("  AHB3ENR = 0x");
    for (int i = 7; i >= 0; i--) { int n = (ahb3enr >> (i*4)) & 0xF; h[7-i] = n < 10 ? '0'+n : 'A'+n-10; } h[8] = '\0';
    DBG_Print(h); DBG_Print("\r\n");

    DBG_Print("  APB5ENR = 0x");
    for (int i = 7; i >= 0; i--) { int n = (apb5enr >> (i*4)) & 0xF; h[7-i] = n < 10 ? '0'+n : 'A'+n-10; } h[8] = '\0';
    DBG_Print(h); DBG_Print("\r\n");

    /* Verifier les bits attendus */
    if (!(memenr & 0x3))  DBG_Print("  !!! SRAM3/4 clocks OFF !!!\r\n");
    if (!(ahb2enr & (1UL<<12))) DBG_Print("  !!! RAMCFG clock OFF !!!\r\n");
    if (!(ahb3enr & (1UL<<14))) DBG_Print("  !!! RISAF clock OFF !!!\r\n");
    if (!(apb5enr & (1UL<<1)))  DBG_Print("  !!! LTDC clock OFF !!!\r\n");

    /* Si SRAM3/4 OFF, tenter d'activer depuis NS (derniere chance) */
    if (!(memenr & 0x3)) {
      DBG_Print("  Trying NS MEMENSR write...\r\n");
      RCC->MEMENSR = 0x3;  /* write-1-to-set bits 0+1 */
      __DSB();
      uint32_t memenr2 = RCC->MEMENR;
      DBG_Print("  MEMENR after = 0x");
      for (int i = 7; i >= 0; i--) { int n = (memenr2 >> (i*4)) & 0xF; h[7-i] = n < 10 ? '0'+n : 'A'+n-10; } h[8] = '\0';
      DBG_Print(h); DBG_Print("\r\n");
      if (!(memenr2 & 0x3)) {
        DBG_Print("  SRAM3/4 CANNOT BE ENABLED - HALTING\r\n");
        while(1) { HAL_Delay(1000); }
      }
    }

    /* Sortir SRAM3/SRAM4 du shutdown (SRAMSD) depuis NS.
     * Le CLEAR_BIT dans AppliSecure a pu echouer si les clocks SRAM
     * n'etaient pas encore actives a ce moment. Ici les clocks sont
     * confirmees ON, donc le clear devrait prendre effet. */
    DBG_Print("  Clearing SRAMSD from NS...\r\n");
    {
      /* Active RAMCFG clock depuis NS au cas ou */
      RCC->AHB2ENSR = RCC_AHB2ENR_RAMCFGEN;
      __DSB();
      volatile RAMCFG_TypeDef *sram3 = RAMCFG_SRAM3_AXI_NS;
      volatile RAMCFG_TypeDef *sram4 = RAMCFG_SRAM4_AXI_NS;
      uint32_t cr3_before = sram3->CR;
      uint32_t cr4_before = sram4->CR;
      DBG_Print("  RAMCFG SRAM3 CR = 0x");
      for (int i = 7; i >= 0; i--) { int n = (cr3_before >> (i*4)) & 0xF; h[7-i] = n < 10 ? '0'+n : 'A'+n-10; } h[8] = '\0';
      DBG_Print(h); DBG_Print("\r\n");

      CLEAR_BIT(sram3->CR, RAMCFG_CR_SRAMSD);
      CLEAR_BIT(sram4->CR, RAMCFG_CR_SRAMSD);
      __DSB();
      /* Attendre que la SRAM sorte du shutdown */
      for (volatile int d = 0; d < 1000; d++) {}

      uint32_t cr3_after = sram3->CR;
      DBG_Print("  RAMCFG SRAM3 CR after = 0x");
      for (int i = 7; i >= 0; i--) { int n = (cr3_after >> (i*4)) & 0xF; h[7-i] = n < 10 ? '0'+n : 'A'+n-10; } h[8] = '\0';
      DBG_Print(h); DBG_Print("\r\n");
      if (cr3_after & RAMCFG_CR_SRAMSD) {
        DBG_Print("  !!! SRAMSD still set - RAMCFG is Secure-only !!!\r\n");
      }
    }
  }

  /* ---- RISAF4 : IASR/IAR lisibles en NS ; REG[] souvent 0 depuis NS (TrustZone) ---- */
  {
    char h[12];
    volatile RISAF_TypeDef *r4 = RISAF4_NS;

    #define PHEX(label, val) do { \
      uint32_t _v = (val); \
      DBG_Print(label " = 0x"); \
      for(int _i=7;_i>=0;_i--){int _n=(_v>>(_i*4))&0xF; h[7-_i]=_n<10?'0'+_n:'A'+_n-10;} h[8]='\0'; \
      DBG_Print(h); DBG_Print("\r\n"); } while(0)

    DBG_Print("[RISAF4] dump (NS):\r\n");
    PHEX("  CR      ", r4->CR);
    PHEX("  IASR    ", r4->IASR);
    PHEX("  R1.CFGR ", r4->REG[0].CFGR);
    PHEX("  R1.START", r4->REG[0].STARTR);
    PHEX("  R1.END  ", r4->REG[0].ENDR);
    PHEX("  R1.CIDCF", r4->REG[0].CIDCFGR);
    PHEX("  IAR.IAES", r4->IAR[0].IAESR);
    PHEX("  IAR.IADR", r4->IAR[0].IADDR);

    uint32_t cfgr = r4->REG[0].CFGR;
    uint32_t cidcfgr = r4->REG[0].CIDCFGR;
    uint32_t startr = r4->REG[0].STARTR;
    if (cfgr == 0U && cidcfgr == 0U && startr == 0U) {
      DBG_Print("  (REG[] all zero from NS — normal TZ; config done on RISAF4_S in AppliSecure)\r\n");
    } else {
      if (!(cfgr & 0x1))
        DBG_Print("  !!! RISAF4 R1 NOT ENABLED (BREN=0) !!!\r\n");
      if (cfgr & (1UL << 8))
        DBG_Print("  !!! RISAF4 R1 SEC bit set — LTDC is NSEC !!!\r\n");
      if (cfgr & (0xFFUL << 16))
        DBG_Print("  !!! RISAF4 R1 PRIVC bits set — check PRIV match !!!\r\n");
      if (!(cidcfgr & (1UL << 1)))
        DBG_Print("  !!! RISAF4 R1 RDENC1 not set — CID1 cannot read !!!\r\n");
      if ((cfgr & 0x1) && (cidcfgr & 0x00FF00FF) == 0x00FF00FF
          && !(cfgr & 0x00FF0100))
        DBG_Print("  RISAF4 R1 config looks GOOD (open, NSEC, NPRIV)\r\n");
    }

    #undef PHEX
  }

  /* SRAM3 @ 0x24200000 : test memoire (meme banque que FBRAM / FB LTDC dans .ld) */
  {
    volatile uint16_t *sram3_probe = (volatile uint16_t *)0x24200000U;
    sram3_probe[0] = 0xBEEFU;
    uint16_t readback = sram3_probe[0];
    DBG_Print("SRAM3 W/R test: wrote 0xBEEF, read 0x");
    {
      char hex[8];
      for (int i = 3; i >= 0; i--) {
        int nib = (readback >> (i*4)) & 0xF;
        hex[3-i] = (nib < 10) ? ('0'+nib) : ('A'+nib-10);
      }
      hex[4] = '\0';
      DBG_Print(hex);
    }
    if (readback == 0xBEEFU) {
      DBG_Print(" -> OK\r\n");
    } else {
      DBG_Print(" -> FAIL! SRAM3 inaccessible!\r\n");
    }
  }

  /* Framebuffer LTDC : section .framebuffer en SRAM3 (FBRAM dans .ld), pas SRAM1 */
  {
    uint32_t fb_bytes = ltdc_fb_size_bytes();
    volatile uint16_t *fb = (volatile uint16_t *)(uintptr_t)&_fb_start;
    DBG_Print("LTDC FB: _fb_start, size=");
    DBG_PrintDec(fb_bytes);
    DBG_Print(" B (expect 768000)\r\n");
    DBG_Print("Filling LTDC FB (SRAM3) RED RGB565 0xF800...\r\n");
    for (uint32_t i = 0; i < (800U * 480U); i++) {
      fb[i] = 0xF800U;
    }
    __DSB();
    lv_port_ltdc_flush_fb_range((const void *)&_fb_start, fb_bytes);
    {
      uint16_t samples[3] = { fb[0], fb[400U * 800U], fb[800U * 480U - 1U] };
      DBG_Print("FB CPU readback after fill: [0],[mid],[last] = ");
      for (int s = 0; s < 3; s++) {
        char hx[8];
        uint16_t v = samples[s];
        for (int i = 3; i >= 0; i--) {
          int nib = (v >> (i*4)) & 0xF;
          hx[3-i] = (nib < 10) ? ('0'+nib) : ('A'+nib-10);
        }
        hx[4] = '\0';
        DBG_Print("0x");
        DBG_Print(hx);
        if (s < 2) DBG_Print(" ");
      }
      DBG_Print("\r\n");
    }
  }

  /* Afficher les registres LTDC cles */
  DBG_Print("RCC->MEMENR = 0x");
  DBG_PrintDec(RCC->MEMENR);
  DBG_Print("\r\n");
  DBG_Print("RCC->APB5ENR = 0x");
  DBG_PrintDec(RCC->APB5ENR);
  DBG_Print("\r\n");

  /* =================================================================
   *  LTDC clock config : tenter depuis NS (fonctionne si PUBCFGR
   *  est configure par AppliSecure recompile)
   * ================================================================= */
  if (!(RCC->APB5ENR & RCC_APB5ENR_LTDCEN)) {
    DBG_Print("[5b] Trying LTDC clocks from NS...\r\n");

    /* IC16 = PLL1 / 45 = 26.67 MHz pixel clock */
    MODIFY_REG(RCC->IC16CFGR,
               RCC_IC16CFGR_IC16SEL | RCC_IC16CFGR_IC16INT,
               (0U  << RCC_IC16CFGR_IC16SEL_Pos) |
               (44U << RCC_IC16CFGR_IC16INT_Pos));
    RCC->DIVENSR = RCC_DIVENSR_IC16ENS;
    for (volatile int w = 0; w < 10000; w++) {}
    /* LTDC kernel clock source = IC16 */
    MODIFY_REG(RCC->CCIPR4, RCC_CCIPR4_LTDCSEL, RCC_CCIPR4_LTDCSEL_1);
    /* LTDC peripheral clock (APB5) */
    RCC->APB5ENSR = RCC_APB5ENR_LTDCEN;
    __DSB();
    for (volatile int w = 0; w < 10000; w++) {}

    uint32_t apb5  = RCC->APB5ENR;
    uint32_t divenr = RCC->DIVENR;
    if (apb5 & RCC_APB5ENR_LTDCEN)
      DBG_Print("  APB5 LTDC clock ON (NS write OK)\r\n");
    else
      DBG_Print("  !!! APB5 LTDC clock STILL OFF — recompile AppliSecure! !!!\r\n");
    if (divenr & RCC_DIVENR_IC16EN)
      DBG_Print("  IC16 pixel clock ON\r\n");
    else
      DBG_Print("  !!! IC16 STILL OFF — recompile AppliSecure! !!!\r\n");
  } else {
    DBG_Print("[5b] LTDC clock already ON (Secure config OK)\r\n");
  }

  /* =================================================================
   *  LVGL init : display driver + tick + bouton de test
   * ================================================================= */
#if APP_LTDC_VISUAL_TEST
  DBG_Print("[6] LTDC hw only (APP_LTDC_VISUAL_TEST)...\r\n");
  lv_port_ltdc_hw_init_only();
  DBG_Print("[7] LTDC hw OK (LVGL skipped)\r\n");
#else
  DBG_Print("[6] LVGL init...\r\n");
  lv_port_init();   /* lv_init() + display LTDC + tick */
  DBG_Print("[7] LVGL init OK\r\n");
#endif

  /* Adresse CFBAR du layer materiel utilise (L2, aligne BSP HAL layer 0). */
  {
    volatile uint32_t cfbar = ((LTDC_Layer_TypeDef *)LTDC_Layer2_BASE_NS)->CFBAR;
    DBG_Print("LTDC L2 CFBAR = 0x");
    char hex[12];
    for (int i = 7; i >= 0; i--) {
      int nib = (cfbar >> (i*4)) & 0xF;
      hex[7-i] = (nib < 10) ? ('0'+nib) : ('A'+nib-10);
    }
    hex[8] = '\0';
    DBG_Print(hex);
    DBG_Print("\r\n");
  }
  {
    volatile uint32_t gcr = LTDC_NS->GCR;
    DBG_Print("LTDC GCR = 0x");
    char hex[12];
    for (int i = 7; i >= 0; i--) {
      int nib = (gcr >> (i*4)) & 0xF;
      hex[7-i] = (nib < 10) ? ('0'+nib) : ('A'+nib-10);
    }
    hex[8] = '\0';
    DBG_Print(hex);
    DBG_Print("\r\n");
  }
  /* Verify LTDC clocks are active (configured by AppliSecure) */
  {
    char hex[12];
    uint32_t apb5 = RCC->APB5ENR;
    uint32_t divr = RCC->DIVENR;
    DBG_Print("APB5ENR(post) = 0x");
    for (int i = 7; i >= 0; i--) { int n = (apb5 >> (i*4)) & 0xF; hex[7-i] = n < 10 ? '0'+n : 'A'+n-10; } hex[8] = '\0';
    DBG_Print(hex); DBG_Print("\r\n");
    DBG_Print("DIVENR = 0x");
    for (int i = 7; i >= 0; i--) { int n = (divr >> (i*4)) & 0xF; hex[7-i] = n < 10 ? '0'+n : 'A'+n-10; } hex[8] = '\0';
    DBG_Print(hex); DBG_Print("\r\n");
    if (!(apb5 & RCC_APB5ENR_LTDCEN))
      DBG_Print("!!! APB5 LTDC clock STILL OFF !!!\r\n");
    if (!(divr & RCC_DIVENR_IC16EN))
      DBG_Print("!!! IC16 pixel clock STILL OFF !!!\r\n");
  }

  /* ---------- LTDC full register dump ---------- */
  {
    char h[12];
    LTDC_TypeDef       *lt = LTDC_NS;
    LTDC_Layer_TypeDef *ly = LTDC_Layer2_NS;

    #define PREG(label, val) do { \
      uint32_t _v = (val); \
      DBG_Print(label " = 0x"); \
      for(int _i=7;_i>=0;_i--){int _n=(_v>>(_i*4))&0xF; h[7-_i]=_n<10?'0'+_n:'A'+_n-10;} h[8]='\0'; \
      DBG_Print(h); DBG_Print("\r\n"); } while(0)

    PREG("SSCR ", lt->SSCR);
    PREG("BPCR ", lt->BPCR);
    PREG("AWCR ", lt->AWCR);
    PREG("TWCR ", lt->TWCR);
    PREG("ISR  ", lt->ISR);
    PREG("CDSR ", lt->CDSR);

    /* Read CPSR twice with delay to see if scan position moves */
    uint32_t cpsr1 = lt->CPSR;
    for(volatile int d=0; d<100000; d++){}
    uint32_t cpsr2 = lt->CPSR;
    PREG("CPSR1", cpsr1);
    PREG("CPSR2", cpsr2);
    if (cpsr1 == cpsr2)
      DBG_Print("!!! CPSR not moving - LTDC not scanning !!!\r\n");
    else
      DBG_Print("CPSR moving - LTDC is scanning OK\r\n");

    PREG("L2.CR   ", ly->CR);
    PREG("L2.RCR  ", ly->RCR);
    PREG("L2.WHPCR", ly->WHPCR);
    PREG("L2.WVPCR", ly->WVPCR);
    PREG("L2.PFCR ", ly->PFCR);
    PREG("L2.CACR ", ly->CACR);
    PREG("L2.BFCR ", ly->BFCR);
    PREG("L2.CFBAR", ly->CFBAR);
    PREG("L2.CFBLR", ly->CFBLR);
    PREG("L2.CFBLNR", ly->CFBLNR);

    {
      LTDC_Layer_TypeDef *l1 = LTDC_Layer1_NS;
      PREG("L1.CR   ", l1->CR);
      PREG("L1.CFBAR", l1->CFBAR);
    }

    /* Check ISR error bits */
    uint32_t isr = lt->ISR;
    if (isr & (1<<1))  DBG_Print("!!! FIFO underrun WARNING !!!\r\n");
    if (isr & (1<<2))  DBG_Print("!!! Transfer ERROR (RISAF?) !!!\r\n");
    if (isr & (1<<6))  DBG_Print("!!! FIFO underrun KILL !!!\r\n");

    #undef PREG
  }

  /* Relecture CPU du meme FB apres LTDC layer actif : si 0x0000 alors bus/RISAF bloque les reads NS */
  {
    volatile uint16_t *fb = (volatile uint16_t *)(uintptr_t)&_fb_start;
    uint16_t samples[3] = { fb[0], fb[400U * 800U], fb[800U * 480U - 1U] };
    DBG_Print("FB CPU readback after LTDC scan: [0],[mid],[last] = ");
    for (int s = 0; s < 3; s++) {
      char hx[8];
      uint16_t v = samples[s];
      for (int i = 3; i >= 0; i--) {
        int nib = (v >> (i*4)) & 0xF;
        hx[3-i] = (nib < 10) ? ('0'+nib) : ('A'+nib-10);
      }
      hx[4] = '\0';
      DBG_Print("0x");
      DBG_Print(hx);
      if (s < 2) DBG_Print(" ");
    }
    DBG_Print("\r\n");
  }

  DBG_Print("--- DIAG END ---\r\n");

  /* --- RISAF4 post-LTDC check : le LTDC a deja commence a scanner ---
   * Si RISAF4 a refuse des acces, IASR sera non-zero */
  {
    char h[12];
    uint32_t iasr = RISAF4_NS->IASR;
    uint32_t iaesr = RISAF4_NS->IAR[0].IAESR;
    uint32_t iaddr = RISAF4_NS->IAR[0].IADDR;
    if (iasr) {
      DBG_Print("[RISAF4] !!! ILLEGAL ACCESS DETECTED !!!\r\n");
      DBG_Print("  IASR  = 0x");
      for(int i=7;i>=0;i--){int n=(iasr>>(i*4))&0xF;h[7-i]=n<10?'0'+n:'A'+n-10;} h[8]='\0';
      DBG_Print(h); DBG_Print("\r\n");
      DBG_Print("  IAESR = 0x");
      for(int i=7;i>=0;i--){int n=(iaesr>>(i*4))&0xF;h[7-i]=n<10?'0'+n:'A'+n-10;} h[8]='\0';
      DBG_Print(h); DBG_Print("\r\n");
      DBG_Print("  IADDR = 0x");
      for(int i=7;i>=0;i--){int n=(iaddr>>(i*4))&0xF;h[7-i]=n<10?'0'+n:'A'+n-10;} h[8]='\0';
      DBG_Print(h); DBG_Print("\r\n");
    } else {
      DBG_Print("[RISAF4] No illegal access (IASR=0) — good\r\n");
    }
  }

  /* RISAF2 / SRAM1 : IASR != 0 => acces vers FB refuses par le filtre */
  {
    char h[12];
    uint32_t i2 = RISAF2_NS->IASR;
    DBG_Print("[RISAF2] IASR = 0x");
    for (int i = 7; i >= 0; i--) {
      int n = (i2 >> (i*4)) & 0xF;
      h[7-i] = n < 10 ? '0'+n : 'A'+n-10;
    }
    h[8] = '\0';
    DBG_Print(h);
    if (i2) {
      uint32_t iaesr = RISAF2_NS->IAR[0].IAESR;
      uint32_t iaddr = RISAF2_NS->IAR[0].IADDR;
      DBG_Print(" IAESR=0x");
      for (int i = 7; i >= 0; i--) {
        int n = (iaesr >> (i*4)) & 0xF;
        h[7-i] = n < 10 ? '0'+n : 'A'+n-10;
      }
      h[8] = '\0';
      DBG_Print(h);
      DBG_Print(" IADDR=0x");
      for (int i = 7; i >= 0; i--) {
        int n = (iaddr >> (i*4)) & 0xF;
        h[7-i] = n < 10 ? '0'+n : 'A'+n-10;
      }
      h[8] = '\0';
      DBG_Print(h);
      DBG_Print(" !!!\r\n");
    } else {
      DBG_Print(" OK\r\n");
    }
  }

  /* === TEST COULEUR DE FOND (Background Color Test) ===
   * Desactive le layer, met BCCR en BLEU VIF.
   * BLEU visible = LTDC output OK (signaux atteignent le panneau)
   * Toujours NOIR = GPIO/signal physique ne sort pas */
  /* Ne pas utiliser LTDC_SRCR_IMR : sur STM32N6 cela peut corrompre l'etat des layers
   * (voir lv_port_disp_ltdc.c). Reload vertical blanking uniquement. */
#if !APP_LTDC_VISUAL_TEST
  DBG_Print("[BG TEST] Layer OFF + BCCR=BLEU... observer 5s\r\n");
  {
    LTDC_Layer_TypeDef *ly = LTDC_Layer2_NS;
    LTDC_NS->BCCR = 0x000000FF;          /* bleu vif RGB888 */
    ly->CR &= ~0x01U;                     /* desactiver layer 2 */
    ly->RCR = (1U << 0) | (1U << 2);     /* per-layer reload IMR|GRMSK */
    LTDC_NS->SRCR = LTDC_SRCR_VBR;
    __DSB();
    {
      uint32_t t0 = HAL_GetTick();
      while (!(LTDC_NS->ISR & LTDC_ISR_RRIF) && (HAL_GetTick() - t0) < 50U) {}
      LTDC_NS->ICR = LTDC_ICR_CRRIF;
    }
  }
  HAL_Delay(5000);  /* 5 secondes pour observer */

  /* Remettre le layer et fond noir */
  {
    LTDC_Layer_TypeDef *ly = LTDC_Layer2_NS;
    LTDC_NS->BCCR = 0x00000000;  /* fond noir */
    ly->CR |= 0x01U;
    ly->RCR = (1U << 0) | (1U << 2);
    LTDC_NS->SRCR = LTDC_SRCR_VBR;
    __DSB();
    {
      uint32_t t0 = HAL_GetTick();
      while (!(LTDC_NS->ISR & LTDC_ISR_RRIF) && (HAL_GetTick() - t0) < 50U) {}
      LTDC_NS->ICR = LTDC_ICR_CRRIF;
    }
  }
  DBG_Print("[BG TEST] Fin - layer reactive\r\n");
  HAL_Delay(1000);

  /* Creer un bouton avec label au centre de l'ecran */
  lv_obj_t *btn = lv_button_create(lv_screen_active());
  lv_obj_center(btn);
  lv_obj_set_size(btn, 200, 60);

  lv_obj_t *label = lv_label_create(btn);
  lv_label_set_text(label, "Hello LVGL!");
  lv_obj_center(label);

  DBG_Print("LVGL ready - button created\r\n");
#else
  ltdc_visual_test_run_sequence(DBG_Print, 3000U);
#endif
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
#if APP_LTDC_VISUAL_TEST
    ltdc_visual_test_loop_cycle(DBG_Print, 2500U);
#else
    lv_timer_handler();   /* LVGL : traiter les taches en attente */
#endif

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
