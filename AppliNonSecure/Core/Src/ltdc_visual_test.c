/**
 * @file ltdc_visual_test.c
 * @brief Tests visuels LTDC 800x480 RGB565 — diagnostic sans LVGL
 */

#include "ltdc_visual_test.h"
#include "framebuffer_ns.h"
#include "lv_port_disp_ltdc.h"
#include "main.h"

#define TVT_W     800U
#define TVT_H     480U
#define TVT_PIX   (TVT_W * TVT_H)

/* RGB565 */
#define C_RED    0xF800U
#define C_GREEN  0x07E0U
#define C_BLUE   0x001FU
#define C_WHITE  0xFFFFU
#define C_BLACK  0x0000U
#define C_YELLOW 0xFFE0U

static void log_line(void (*log)(const char *s), const char *msg)
{
  if (log != NULL && msg != NULL) {
    log(msg);
  }
}

static void ltdc_reload_shadow(void)
{
  LTDC_Layer_TypeDef *ly = LTDC_Layer2_NS;
  ly->RCR = (1U << 0) | (1U << 2);
  LTDC_NS->SRCR = LTDC_SRCR_VBR;
  __DSB();
  uint32_t t0 = HAL_GetTick();
  while (!(LTDC_NS->ISR & LTDC_ISR_RRIF) && (HAL_GetTick() - t0) < 50U) {
  }
  LTDC_NS->ICR = LTDC_ICR_CRRIF;
}

static void fb_fill_solid(uint16_t color)
{
  volatile uint16_t *fb = (volatile uint16_t *)(uintptr_t)&_fb_start;
  for (uint32_t i = 0; i < TVT_PIX; i++) {
    fb[i] = color;
  }
  __DSB();
  lv_port_ltdc_flush_fb_range((const void *)&_fb_start, ltdc_fb_size_bytes());
}

static void fb_fill_stripes(uint16_t c0, uint16_t c1, uint32_t band_h)
{
  volatile uint16_t *fb = (volatile uint16_t *)(uintptr_t)&_fb_start;
  if (band_h == 0U) {
    band_h = 60U;
  }
  for (uint32_t y = 0; y < TVT_H; y++) {
    uint16_t c = ((y / band_h) & 1U) ? c1 : c0;
    uint32_t row = y * TVT_W;
    for (uint32_t x = 0; x < TVT_W; x++) {
      fb[row + x] = c;
    }
  }
  __DSB();
  lv_port_ltdc_flush_fb_range((const void *)&_fb_start, ltdc_fb_size_bytes());
}

static void layer_set_enable(int on)
{
  LTDC_Layer_TypeDef *ly = LTDC_Layer2_NS;
  if (on) {
    ly->CR |= LTDC_LxCR_LEN;
  } else {
    ly->CR &= ~LTDC_LxCR_LEN;
  }
  ltdc_reload_shadow();
}

void ltdc_visual_test_run_sequence(void (*log)(const char *s), uint32_t hold_ms)
{
  if (hold_ms < 500U) {
    hold_ms = 500U;
  }

  log_line(log, "\r\n=== LTDC VISUAL TEST (no LVGL) ===\r\n");

  /* 1 — Rouge plein ecran, layer actif */
  log_line(log, "[T1] Layer ON + FB solid RED (RGB565 0xF800)\r\n");
  layer_set_enable(1);
  LTDC_NS->BCCR = 0x00000000U;
  ltdc_reload_shadow();
  fb_fill_solid(C_RED);
  HAL_Delay(hold_ms);

  /* 2 — Vert */
  log_line(log, "[T2] FB solid GREEN\r\n");
  fb_fill_solid(C_GREEN);
  HAL_Delay(hold_ms);

  /* 3 — Bleu */
  log_line(log, "[T3] FB solid BLUE\r\n");
  fb_fill_solid(C_BLUE);
  HAL_Delay(hold_ms);

  /* 4 — Bandes H noir/blanc (pitch / fenetre) */
  log_line(log, "[T4] Horizontal stripes B/W (band 60px)\r\n");
  fb_fill_stripes(C_BLACK, C_WHITE, 60U);
  HAL_Delay(hold_ms);

  /* 5 — Bandes plus fines */
  log_line(log, "[T5] Horizontal stripes B/W (band 8px)\r\n");
  fb_fill_stripes(C_BLACK, C_WHITE, 8U);
  HAL_Delay(hold_ms);

  /* 6 — Fond magenta BCCR, layer OFF (chemin panneau sans FB) */
  log_line(log, "[T6] Layer OFF + BCCR magenta RGB888 — si ecran reste noir, piste signal/polarite\r\n");
  LTDC_NS->BCCR = 0x00FF00FFU;
  layer_set_enable(0);
  HAL_Delay(hold_ms);

  /* 7 — Fond cyan */
  log_line(log, "[T7] Layer OFF + BCCR cyan\r\n");
  LTDC_NS->BCCR = 0x0000FFFFU;
  ltdc_reload_shadow();
  HAL_Delay(hold_ms);

  /* 8 — Restaurer layer + fond noir + jaune FB */
  log_line(log, "[T8] Restore layer ON + BCCR black + FB YELLOW\r\n");
  LTDC_NS->BCCR = 0x00000000U;
  layer_set_enable(1);
  ltdc_reload_shadow();
  fb_fill_solid(C_YELLOW);
  HAL_Delay(hold_ms);

  log_line(log, "=== LTDC VISUAL TEST sequence END ===\r\n");
  log_line(log, "Entering color cycle loop (USART + LEDs)...\r\n");
}

void ltdc_visual_test_loop_cycle(void (*log)(const char *s), uint32_t step_ms)
{
  static uint32_t last;
  static uint8_t  idx;
  const uint16_t colors[] = { C_RED, C_GREEN, C_BLUE, C_WHITE };
  const char *const names[] = { "RED", "GREEN", "BLUE", "WHITE" };

  if (step_ms < 200U) {
    step_ms = 200U;
  }

  uint32_t now = HAL_GetTick();
  if (last != 0U && (now - last) < step_ms) {
    return;
  }
  last = now;

  layer_set_enable(1);
  LTDC_NS->BCCR = 0x00000000U;
  fb_fill_solid(colors[idx]);
  if (log != NULL) {
    log("cycle FB ");
    log(names[idx]);
    log("\r\n");
  }
  idx = (uint8_t)((idx + 1U) % 4U);
}
