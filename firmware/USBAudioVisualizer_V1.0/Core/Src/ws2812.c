#include "ws2812.h"
#include "tim.h"

extern DMA_HandleTypeDef hdma_tim2_ch2_ch4;

/* DMA buffer: 32-bit words (DMA WORD aligned → CCR4 is 32-bit register) */
static uint32_t ws2812_buf[WS2812_BUF_SIZE];

/* LED color state (GRB packed) */
static uint32_t ws2812_colors[WS2812_NUM_LEDS];

/* Generate WS2812 signal into uint32_t DMA buffer.
   Returns total length in 32-bit words. */
static int16_t ws2812_genSignal(uint32_t *colors, int16_t n, uint32_t *signal)
{
  int16_t cur = 0;

  /* 1. Encode each LED: GRB order, MSB first */
  for (int16_t i = 0; i < n; i++) {
    for (int8_t c = 2; c >= 0; c--) {          /* G[23:16] → R[15:8] → B[7:0] */
      uint8_t byte = (colors[i] >> (8U * c)) & 0xFFU;
      for (int8_t b = 7; b >= 0; b--) {
        signal[cur++] = (byte >> b) & 1U ? WS2812_CODE_1 : WS2812_CODE_0;
      }
    }
  }

  /* 2. Append reset pulse (all zero = LOW for WS2812 latch) */
  for (int16_t i = 0; i < (int16_t)WS2812_FRAME_RST; i++) {
    signal[cur++] = 0U;
  }

  return cur;
}

/* DMA Transfer Complete callback */
void HAL_TIM_PWM_PulseFinishedCallback(TIM_HandleTypeDef *htim)
{
  if (htim->Instance == TIM2) {
    HAL_TIM_PWM_Stop_DMA(&htim2, TIM_CHANNEL_4);
  }
}

void WS2812_Init(void)
{
  /* Ensure PA3 is AF1 (TIM2_CH4) — may have been reconfigured by OLED */
  GPIO_InitTypeDef GPIO_InitStruct = {0};
  GPIO_InitStruct.Pin       = GPIO_PIN_3;
  GPIO_InitStruct.Mode      = GPIO_MODE_AF_PP;
  GPIO_InitStruct.Pull      = GPIO_NOPULL;
  GPIO_InitStruct.Speed     = GPIO_SPEED_FREQ_VERY_HIGH;
  GPIO_InitStruct.Alternate = GPIO_AF1_TIM2;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /* Initialize all LEDs to OFF (black) */
  for (uint8_t i = 0; i < WS2812_NUM_LEDS; i++)
    ws2812_colors[i] = 0U;

  /* Send initial black frame */
  WS2812_Update();
}

void WS2812_SetColor(uint8_t index, uint8_t g, uint8_t r, uint8_t b)
{
  if (index >= WS2812_NUM_LEDS) return;
  ws2812_colors[index] = ((uint32_t)g << 16) | ((uint32_t)r << 8) | b;
}

void WS2812_Update(void)
{
  int16_t len = ws2812_genSignal(ws2812_colors, (int16_t)WS2812_NUM_LEDS, ws2812_buf);

  /* Clear pending flags, disable Update DMA (we use CC4 DMA only) */
  __HAL_TIM_CLEAR_FLAG(&htim2, TIM_FLAG_CC4 | TIM_FLAG_UPDATE);
  __HAL_TIM_DISABLE_DMA(&htim2, TIM_DMA_UPDATE);

  /* One-shot DMA transfer (WORD aligned, Normal mode) */
  HAL_TIM_PWM_Start_DMA(&htim2, TIM_CHANNEL_4, ws2812_buf, (uint32_t)len);
}
