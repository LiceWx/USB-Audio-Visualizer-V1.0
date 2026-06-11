/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * File Name          : freertos.c
  * Description        : Code for freertos applications
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
#include "FreeRTOS.h"
#include "task.h"
#include "main.h"
#include "cmsis_os.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "audio_bridge.h"
#include "oled.h"
#include "audio_dsp.h"
#include "ws2812.h"
#include <math.h>
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
/* USER CODE BEGIN Variables */

/* USER CODE END Variables */
/* Definitions for defaultTask */
osThreadId_t defaultTaskHandle;
const osThreadAttr_t defaultTask_attributes = {
  .name = "defaultTask",
  .stack_size = 1024 * 4,
  .priority = (osPriority_t) osPriorityNormal,
};
/* Definitions for audioProcTask */
osThreadId_t audioProcTaskHandle;
const osThreadAttr_t audioProcTask_attributes = {
  .name = "audioProcTask",
  .stack_size = 1024 * 4,
  .priority = (osPriority_t) osPriorityBelowNormal,
};

/* Private function prototypes -----------------------------------------------*/
/* USER CODE BEGIN FunctionPrototypes */
extern void AudioMerge(uint16_t *dst, uint32_t start, uint32_t sample_pairs_count);
extern uint32_t audio_rms_last;
/* USER CODE END FunctionPrototypes */

void StartDefaultTask(void *argument);
extern void StartAudioProcTask(void *argument);

extern void MX_USB_DEVICE_Init(void);
extern void MX_USB_HOST_Init(void);
void MX_FREERTOS_Init(void); /* (MISRA C 2004 rule 8.1) */

/**
  * @brief  FreeRTOS initialization
  * @param  None
  * @retval None
  */
void MX_FREERTOS_Init(void) {
  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* USER CODE BEGIN RTOS_MUTEX */
  /* add mutexes, ... */
  /* USER CODE END RTOS_MUTEX */

  /* USER CODE BEGIN RTOS_SEMAPHORES */
  /* add semaphores, ... */
  /* USER CODE END RTOS_SEMAPHORES */

  /* USER CODE BEGIN RTOS_TIMERS */
  /* start timers, add new ones, ... */
  /* USER CODE END RTOS_TIMERS */

  /* USER CODE BEGIN RTOS_QUEUES */
  /* add queues, ... */
  /* USER CODE END RTOS_QUEUES */

  /* Create the thread(s) */
  /* creation of defaultTask */
  defaultTaskHandle = osThreadNew(StartDefaultTask, NULL, &defaultTask_attributes);

  /* creation of audioProcTask */
  audioProcTaskHandle = osThreadNew(StartAudioProcTask, NULL, &audioProcTask_attributes);

  /* USER CODE BEGIN RTOS_THREADS */
  /* add threads, ... */
  /* USER CODE END RTOS_THREADS */

  /* USER CODE BEGIN RTOS_EVENTS */
  /* add events, ... */
  /* USER CODE END RTOS_EVENTS */

}

/* USER CODE BEGIN Header_StartDefaultTask */
/**
  * @brief  Function implementing the defaultTask thread.
  * @param  argument: Not used
  * @retval None
  */
/* USER CODE END Header_StartDefaultTask */
void StartDefaultTask(void *argument)
{
  /* init code for USB_DEVICE */
  MX_USB_DEVICE_Init();

  /* init code for USB_HOST */
  MX_USB_HOST_Init();
  /* USER CODE BEGIN StartDefaultTask */
  /* Infinite loop */
  for(;;)
  {
    osDelay(1);
  }
  /* USER CODE END StartDefaultTask */
}

/* Private application code --------------------------------------------------*/
/* USER CODE BEGIN Application */

#define WIN_LEN 1024U
// 1024 samples = 1024 / 48k = 21.3ms window length

const uint32_t displayedVolume[6] = {
  100U, 400U, 2000U, 5000U, 10000U, 23000
};
const uint32_t onsetBarBrightness[6] = {
  0, 20U, 50U, 90U, 170U, 255U
};
const float onsetThreshold[6] = {
  1.f, 1.25f, 1.5f, 2.2f, 4.f, 8.f
};

int8_t checkSettingsChange(uint8_t* dv, uint8_t* ob, uint8_t* ot) {
  uint8_t cur_dv = GetDisplayedVolume();
  uint8_t cur_ob = GetOnsetBarBrightness();
  uint8_t cur_ot = GetOnsetThreshold();
  
  if (cur_dv != *dv) {
    *dv = cur_dv;
    return 1;
  }
  if (cur_ob != *ob) {
    *ob = cur_ob;
    return 2;
  }
  if (cur_ot != *ot) {
    *ot = cur_ot;
    return 3;
  }
  return 0;
}

/* USER CODE BEGIN Header_StartAudioProcTask */
/**
* @brief Function implementing the audioProcTask thread.
* @param argument: Not used
* @retval None
*/
/* USER CODE END Header_StartAudioProcTask */
void StartAudioProcTask(void *argument)
{
  /* USER CODE BEGIN StartAudioProcTask */

  OLED_Init();
  MelSpectrogram_Init();
  WS2812_Init();

  uint8_t dv = GetDisplayedVolume();
  uint8_t ob = GetOnsetBarBrightness();
  uint8_t ot = GetOnsetThreshold();

  uint8_t changed_type = 0;
  uint32_t changed_time = 0;

  /* Infinite loop */
  for (; ; ) {
    
    changed_type = checkSettingsChange(&dv, &ob, &ot);
    if (changed_type != 0) {
      changed_time = osKernelGetTickCount();
    }

    if (osKernelGetTickCount() - changed_time < 1000U) {
      switch (changed_type) {
        case 1 : DisplaySettingsChange(1, dv); break;
        case 2 : DisplaySettingsChange(2, ob); break;
        case 3 : DisplaySettingsChange(3, ot); break;
        default: break;
      }
      continue;
    }
    
    uint16_t window[WIN_LEN];
    uint16_t end = bridge_rd;
    uint16_t start = (end >= 4*WIN_LEN) ? (end - 4*WIN_LEN) : (BRIDGE_BUF_SIZE + end - 4*WIN_LEN);
    AudioMerge(window, start, WIN_LEN);
    uint32_t rms = audio_rms_last;

    // print rms value on oled
    /*
    OLED_Clear();
    OLED_ShowString(0, 0, "RMS:", 8, 1);
    char buf[16];
    snprintf(buf, sizeof(buf), "%lu", rms);
    OLED_ShowString(30, 0, buf, 8, 1);
    */

    // print rms bar (same as demo: no Clear, bar on row 0-15, XOR text)
    uint32_t pct = (rms * 100U) / displayedVolume[dv];
    if (pct > 100U) pct = 100U;
    OLED_DrawBar(0, 15, (uint8_t)pct);

    OLED_ShowString(0, 1, "RMS:", 8, 2);
    OLED_ShowNum(24, 1, rms, 5, 8, 2);

    /* FPS counter (smoothed) */
    static uint32_t last_tick = 0;
    static uint32_t fps_smooth = 0;
    uint32_t now = osKernelGetTickCount();
    uint32_t delta = (now > last_tick) ? (now - last_tick) : 1U;
    last_tick = now;
    uint32_t fps = (delta > 0U) ? (1000U / delta) : 999U;
    if (fps_smooth == 0U) fps_smooth = fps;
    else fps_smooth = (fps_smooth * 7U + fps) / 8U;
    OLED_ShowString(74, 7, "FPS:", 8, 2);
    OLED_ShowNum(98, 7, fps_smooth, 3, 8, 2);

    //
    static float mel_data[POOLED_BINS];
    MelSpectrogram_Process(window, mel_data);
    float mel_max = MelSpectrogram_GetMax();

    /* Silence gate: suppress spectrogram when RMS or pct is negligible */
    #define SILENCE_RMS_THRESH  20U
    if (rms < SILENCE_RMS_THRESH || pct == 0U) {
      for (uint8_t i = 0; i < POOLED_BINS; i++) mel_data[i] = 0.0f;
      mel_max = 0.0f;
    }

    /* Scale spectrogram so tallest bar matches RMS bar percentage.
       Floor pct to 1% to avoid division-by-zero amplification of noise. */
    {
      uint32_t pct_s = (pct > 0U) ? pct : 1U;
      float spec_scale = mel_max * 100.0f / (float)pct_s;
      OLED_DrawSpectrogram(mel_data, spec_scale);
    }
    OLED_Refresh();

    // ws2812: onset queue — shift every frame, LED[i] = onset from i frames ago
    {
      static uint8_t trail[WS2812_NUM_LEDS] = {0};
      float onset = MelSpectrogram_GetOnset();

      /* Auto-normalize onset ceiling */
      static float onset_ceil = 0.001f;
      if (onset > onset_ceil)
        onset_ceil = onset;
      else
        onset_ceil = onset_ceil * 0.999f;
      if (onset_ceil < 0.0001f) onset_ceil = 0.0001f;

      float level = onset / onset_ceil;
      if (level > 1.0f) level = 1.0f;

      // nonlinear transform
      level = tanhf(onsetThreshold[ot] * level);

      /* Shift queue: LED[i+1] = LED[i], new value at LED[0] */
      for (int8_t i = WS2812_NUM_LEDS - 1; i > 0; i--)
        trail[i] = trail[i - 1];
      trail[0] = (uint8_t)(level * onsetBarBrightness[ob]);

      for (uint8_t i = 0; i < WS2812_NUM_LEDS; i++)
        WS2812_SetColor(i, trail[i], trail[i], trail[i]);
      WS2812_Update();
    }

    // superflux onset
    ;

    osDelay(1);
  }
  /* USER CODE END StartAudioProcTask */
}

/* USER CODE END Application */

