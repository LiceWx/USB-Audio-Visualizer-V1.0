/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.h
  * @brief          : Header for main.c file.
  *                   This file contains the common defines of the application.
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

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __MAIN_H
#define __MAIN_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "stm32f4xx_hal.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

/* Exported types ------------------------------------------------------------*/
/* USER CODE BEGIN ET */

/* USER CODE END ET */

/* Exported constants --------------------------------------------------------*/
/* USER CODE BEGIN EC */

/* USER CODE END EC */

/* Exported macro ------------------------------------------------------------*/
/* USER CODE BEGIN EM */

/* USER CODE END EM */

/* Exported functions prototypes ---------------------------------------------*/
void Error_Handler(void);

/* USER CODE BEGIN EFP */
uint8_t GetOnsetThreshold();
uint8_t GetOnsetBarBrightness();
uint8_t GetDisplayedVolume();
void DisplaySettingsChange(uint8_t type, uint8_t level);
void DisplayLogo();
/* USER CODE END EFP */

/* Private defines -----------------------------------------------------------*/
#define OLED_DC_Pin GPIO_PIN_4
#define OLED_DC_GPIO_Port GPIOA
#define OLED_SCK_Pin GPIO_PIN_5
#define OLED_SCK_GPIO_Port GPIOA
#define OLED_MOSI_Pin GPIO_PIN_6
#define OLED_MOSI_GPIO_Port GPIOA
#define OLED_RES_Pin GPIO_PIN_7
#define OLED_RES_GPIO_Port GPIOA

/* USER CODE BEGIN Private defines */

/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
