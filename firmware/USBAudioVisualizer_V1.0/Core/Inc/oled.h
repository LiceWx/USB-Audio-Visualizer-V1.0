#ifndef __OLED_H
#define __OLED_H

#include "main.h"
#include <stdint.h>

/* ── SW SPI pin mapping (from CubeMX main.h). Short aliases for oled.c. ── */
#define OLED_DC_Port   OLED_DC_GPIO_Port
#define OLED_SCK_Port  OLED_SCK_GPIO_Port
#define OLED_MOSI_Port OLED_MOSI_GPIO_Port
#define OLED_RES_Port  OLED_RES_GPIO_Port

#define OLED_CMD  0
#define OLED_DATA 1

void OLED_Init(void);
void OLED_Clear(void);
void OLED_Refresh(void);
void OLED_ShowString(uint8_t x, uint8_t y, const char *str, uint8_t size, uint8_t mode);
void OLED_ShowNum(uint8_t x, uint8_t y, uint32_t num, uint8_t len, uint8_t size, uint8_t mode);
void OLED_DrawBar(uint8_t y0, uint8_t y1, uint8_t percent);
void OLED_DrawWave(const uint16_t *buf, uint16_t start, uint16_t count, uint8_t y0, uint8_t y1, uint8_t gain);
void OLED_DrawSpectrogram(const float *mel_data, float max_val);
void OLED_DrawODFBars(const float *odf, float max_val);
void OLED_DrawBitmap(uint8_t start_page, uint8_t num_pages, const uint8_t bitmap[][6]);
void OLED_PreRTOS_Test(void);

#endif
