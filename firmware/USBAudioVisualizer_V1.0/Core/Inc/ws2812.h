#pragma once
#include <stdint.h>

/* WS2812 bit timing (TIM2_CLK = 90MHz, Period = 112) */
/* Bit cycle = (112+1)/90MHz = 1.256us, bit rate ≈ 796kHz */
#define WS2812_CODE_0    34U   /* 34/90MHz ≈ 378ns high (matches demo) */
#define WS2812_CODE_1    76U   /* 76/90MHz ≈ 844ns high (matches demo) */
#define WS2812_FRAME_RST  240U /* 240 × 1.256us ≈ 301us > 50us reset */

#define WS2812_NUM_LEDS   16U
#define WS2812_LED_BITS   24U
/* Buffer: 16 LEDs × 24 bits + 240 reset slots = 624 words */
#define WS2812_BUF_SIZE   (WS2812_NUM_LEDS * WS2812_LED_BITS + WS2812_FRAME_RST)

/* Color: [31:24]=unused [23:16]=G [15:8]=R [7:0]=B (GRB order) */
void WS2812_SetColor(uint8_t index, uint8_t g, uint8_t r, uint8_t b);
void WS2812_Update(void);   /* generate frame + trigger DMA one-shot */
void WS2812_Init(void);
