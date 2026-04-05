/**
 * @file framebuffer_ns.h
 * @brief Symboles linker .framebuffer (FBRAM) — STM32N657 NS
 *
 * Ne pas declarer _fb_start/_fb_end comme char[] : la soustraction donnait 0
 * et lv_port_ltdc_flush_fb_range(..., 0) ne vidait pas le cache D.
 */
#ifndef FRAMEBUFFER_NS_H
#define FRAMEBUFFER_NS_H

#include <stdint.h>
#include <stddef.h>

extern uint8_t _fb_start;
extern uint8_t _fb_end;

#define LTDC_FB_PIX_BYTES  (800U * 480U * 2U)

/** Taille depuis le linker ; si 0 (symboles mal declares), retombe sur 768000. */
static inline uint32_t ltdc_fb_size_bytes(void)
{
  uintptr_t a = (uintptr_t)&_fb_start;
  uintptr_t b = (uintptr_t)&_fb_end;
  uint32_t  n = (b > a) ? (uint32_t)(b - a) : 0U;
  return (n != 0U) ? n : LTDC_FB_PIX_BYTES;
}

#endif /* FRAMEBUFFER_NS_H */
