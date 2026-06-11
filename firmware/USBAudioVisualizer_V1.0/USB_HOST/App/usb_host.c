/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file            : usb_host.c
  * @version         : v1.0_Cube
  * @brief           : This file implements the USB Host
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

#include "usb_host.h"
#include "usbh_core.h"
#include "usbh_audio.h"

/* USER CODE BEGIN Includes */
#include "audio_bridge.h"
/* USER CODE END Includes */

/* USER CODE BEGIN PV */
/* Private variables ---------------------------------------------------------*/

/* ── Audio sample rate (shared by Device + Host) ── */
#define AUDIO_SAMPLE_RATE    48000U
#define AUDIO_CHANNELS       2U
#define AUDIO_BIT_DEPTH      16U

/* ── Bridge ring buffer: Device (PC→STM32) writes, Host (STM32→headset) reads ── */
#define BRIDGE_BUF_PACKETS   100U                  /* 100 ms of audio */

uint8_t  bridge_buffer[BRIDGE_BUF_SIZE];
volatile uint16_t bridge_wr = 0U;  /* Device advances this (ISR context) */
volatile uint16_t bridge_rd = 0U;  /* Host advances this   (SOF context) */
static uint8_t  bridge_primed = 0U;       /* 1 = enough data to start playback */
static uint8_t  audio_playing = 0U;

/* Debug counters */
volatile uint32_t dbg_pc_packets   = 0U;
volatile uint32_t dbg_hp_frames    = 0U;
volatile uint8_t  dbg_hp_connected = 0U;
volatile uint8_t  dbg_audio_init   = 0U;
volatile uint8_t  dbg_audio_start  = 0U;

/* USER CODE END PV */

/* USER CODE BEGIN PFP */
/* Private function prototypes -----------------------------------------------*/

/* USER CODE END PFP */

/* USB Host core handle declaration */
USBH_HandleTypeDef hUsbHostHS;
ApplicationTypeDef Appli_state = APPLICATION_IDLE;

/*
 * -- Insert your variables declaration here --
 */
/* USER CODE BEGIN 0 */
/**
  * @brief  Reset the bridge buffer pointers (called on new device connection).
  */
static void Bridge_Reset(void)
{
  bridge_wr = 0U;
  bridge_rd = 0U;
  bridge_primed = 0U;
}

/**
  * @brief  Check how many bytes are available in the bridge ring buffer.
  */
static uint16_t Bridge_Available(void)
{
  if (bridge_wr >= bridge_rd)
  {
    return bridge_wr - bridge_rd;
  }
  return (uint16_t)(BRIDGE_BUF_SIZE - bridge_rd + bridge_wr);
}

/**
  * @brief  Start playback forwarding PC audio → USB headset.
  *         Must be called when play_state == AUDIO_PLAYBACK_IDLE.
  */
static void Bridge_StartPlayback(USBH_HandleTypeDef *phost)
{
  if (audio_playing != 0U)
  {
    return;
  }

  /* Start playback with bridge_buffer and infinite total_length.
   * Actual data is fed by Device ISR into bridge_wr positions. */
  if (USBH_AUDIO_Play(phost, bridge_buffer, 0xFFFFFF00U) == USBH_OK)
  {
    audio_playing = 1U;
  }
}

/**
  * @brief  SOF-driven isochronous audio pump (called every 1ms from SOF ISR).
  *         - Ring buffer maintenance (extend total_length, wrap cbuf)
  *         - START_OUT → OUT transition
  *         - Send one audio frame per call
  *         Runs entirely in interrupt context → main loop blocking has no effect.
  */
void AudioIsochPump(USBH_HandleTypeDef *phost)
{
  if (audio_playing == 0U || phost->gState != HOST_CLASS
      || phost->pActiveClass == NULL)
  {
    return;
  }

  AUDIO_HandleTypeDef *haudio = (AUDIO_HandleTypeDef *)phost->pActiveClass->pData;

  /* ── Ring buffer maintenance ── */
  if (haudio->headphone.total_length - haudio->headphone.global_ptr < BRIDGE_BUF_SIZE / 2U)
  {
    haudio->headphone.total_length += BRIDGE_BUF_SIZE;
  }
  if (haudio->headphone.cbuf >= bridge_buffer + BRIDGE_BUF_SIZE)
  {
    uint32_t excess = (uint32_t)(haudio->headphone.cbuf - bridge_buffer);
    haudio->headphone.cbuf = bridge_buffer + (excess % BRIDGE_BUF_SIZE);
  }

  /* ── Data pump (reads from bridge ring buffer) ── */
  if (haudio->play_state != AUDIO_PLAYBACK_PLAY)
  {
    return;
  }

  switch (haudio->processing_state)
  {
    case AUDIO_DATA_START_OUT:
      /* Wait until bridge has at least 50ms of data before starting */
      if (Bridge_Available() < BRIDGE_BUF_SIZE / 5U)
      {
        return;
      }
      if ((phost->Timer & 1U) == 0U)
      {
        haudio->headphone.timer = phost->Timer;
        haudio->processing_state = AUDIO_DATA_OUT;
        (void)USBH_IsocSendData(phost,
                                &bridge_buffer[bridge_rd],
                                (uint32_t)AUDIO_FRAME_SIZE,
                                haudio->headphone.Pipe);
        bridge_rd = (bridge_rd + AUDIO_FRAME_SIZE) % BRIDGE_BUF_SIZE;
        haudio->headphone.partial_ptr = AUDIO_FRAME_SIZE;
        haudio->headphone.global_ptr = AUDIO_FRAME_SIZE;
        haudio->headphone.cbuf = &bridge_buffer[bridge_rd];
      }
      break;

    case AUDIO_DATA_OUT:
      if ((USBH_LL_GetURBState(phost, haudio->headphone.Pipe) == USBH_URB_DONE)
          && ((phost->Timer - haudio->headphone.timer) >= haudio->headphone.Poll))
      {
        haudio->headphone.timer = phost->Timer;

        /* Send only if data is available; otherwise send silence */
        if (Bridge_Available() >= AUDIO_FRAME_SIZE)
        {
          (void)USBH_IsocSendData(phost,
                                  &bridge_buffer[bridge_rd],
                                  (uint32_t)AUDIO_FRAME_SIZE,
                                  haudio->headphone.Pipe);
          bridge_rd = (bridge_rd + AUDIO_FRAME_SIZE) % BRIDGE_BUF_SIZE;
        }
        else
        {
          /* Under-run: resend last valid data position */
          uint16_t safe_rd = (bridge_rd >= AUDIO_FRAME_SIZE)
                             ? (bridge_rd - AUDIO_FRAME_SIZE)
                             : (uint16_t)(BRIDGE_BUF_SIZE - AUDIO_FRAME_SIZE);
          (void)USBH_IsocSendData(phost,
                                  &bridge_buffer[safe_rd],
                                  (uint32_t)AUDIO_FRAME_SIZE,
                                  haudio->headphone.Pipe);
        }
        haudio->headphone.cbuf = &bridge_buffer[bridge_rd];
        haudio->headphone.partial_ptr += AUDIO_FRAME_SIZE;
        haudio->headphone.global_ptr += AUDIO_FRAME_SIZE;
        dbg_hp_frames++;
      }
      break;

    default:
      break;
  }
}

/**
  * @brief  Override __weak USBH_AUDIO_FrequencySet.
  *         Called when endpoint frequency config is done (play_state == IDLE).
  */
void USBH_AUDIO_FrequencySet(USBH_HandleTypeDef *phost)
{
  static uint8_t freq_configured = 0U;

  if (freq_configured == 0U)
  {
    if (USBH_AUDIO_SetFrequency(phost, AUDIO_SAMPLE_RATE,
                                AUDIO_CHANNELS, AUDIO_BIT_DEPTH) == USBH_OK)
    {
      freq_configured = 1U;
    }
  }
  else
  {
    freq_configured = 0U;
    Bridge_StartPlayback(phost);
  }
}

/**
  * @brief  Override __weak USBH_AUDIO_BufferEmptyCallback.
  *         Safety net — should never fire with ring buffer, but restart if needed.
  */
void USBH_AUDIO_BufferEmptyCallback(USBH_HandleTypeDef *phost)
{
  audio_playing = 0U;
  Bridge_Reset();
  Bridge_StartPlayback(phost);
}

/* USER CODE END 0 */

/*
 * user callback declaration
 */
static void USBH_UserProcess(USBH_HandleTypeDef *phost, uint8_t id);

/*
 * -- Insert your external function declaration here --
 */
/* USER CODE BEGIN 1 */

/* USER CODE END 1 */

/**
  * Init USB host library, add supported class and start the library
  * @retval None
  */
void MX_USB_HOST_Init(void)
{
  /* USER CODE BEGIN USB_HOST_Init_PreTreatment */

  /* USER CODE END USB_HOST_Init_PreTreatment */

  /* Init host Library, add supported class and start the library. */
  if (USBH_Init(&hUsbHostHS, USBH_UserProcess, HOST_HS) != USBH_OK)
  {
    Error_Handler();
  }
  if (USBH_RegisterClass(&hUsbHostHS, USBH_AUDIO_CLASS) != USBH_OK)
  {
    Error_Handler();
  }
  if (USBH_Start(&hUsbHostHS) != USBH_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USB_HOST_Init_PostTreatment */
  /* Force-enable SOF interrupt — CubeMX defaults to DISABLE but isochronous
   * audio needs SOF for frame timing (phost->Timer increments via SOF). */
  USB_OTG_HS->GINTMSK |= USB_OTG_GINTMSK_SOFM;
  /* USER CODE END USB_HOST_Init_PostTreatment */
}

/*
 * user callback definition
 */
static void USBH_UserProcess  (USBH_HandleTypeDef *phost, uint8_t id)
{
  /* USER CODE BEGIN CALL_BACK_1 */
  switch(id)
  {
  case HOST_USER_SELECT_CONFIGURATION:
  break;

  case HOST_USER_DISCONNECTION:
  Appli_state = APPLICATION_DISCONNECT;
  audio_playing = 0U;
  Bridge_Reset();
  break;

  case HOST_USER_CLASS_ACTIVE:
  Appli_state = APPLICATION_READY;
  dbg_hp_connected = 1U;
  break;

  case HOST_USER_CONNECTION:
  Appli_state = APPLICATION_START;
  break;

  default:
  break;
  }
  /* USER CODE END CALL_BACK_1 */
}

/**
  * @}
  */

/**
  * @}
  */

